// Launcher backdrop: a red/blue colour wave like the one behind the game's menus. The game shades a 4x4 grid of
// vertices across the screen, each vertex swinging between red and blue at its own rate (func_80041B24; see
// docs/GAME.md, Menu background). ColorWaveElement draws the same kind of field with RmlUi geometry, which the
// renderer shades per vertex; it is our own code from that description, with no game data.
//
// Layers, back to front, in the launcher's background container:
// - the colour wave;
// - shade.svg: a mild vignette so the title and buttons stay readable.
// The default title is replaced by an Orbitron wordmark.
//
// The launcher hides itself before opening the settings (RecompFrontend ui_launcher.cpp), which would leave
// them on black, so a copy of the backdrop is also placed behind the settings window (SettingsBackdrop).

#include "robotron64_launcher.h"
#include "recompui/config.h"
#include "ultramodern/ultramodern.hpp"

#include <array>
#include <chrono>
#include <cmath>

namespace {
    constexpr const char* color_wave_tag = "robotron64-colorwave";
    constexpr int grid = 4;                                // vertices per side
    constexpr float wave_rate = 0.5752f;                   // rad/s for vertex 1; vertex k runs k times as fast
    const Rml::ColourbPremultiplied wave_red{ 128, 0, 0, 255 };
    const Rml::ColourbPremultiplied wave_blue{ 0, 0, 128, 255 };

    const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    recompui::Label* subtitle = nullptr;
    bool settings_backdrop_created = false;

    float seconds() {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - start_time).count();
    }

    Rml::ColourbPremultiplied mix(float c) {
        auto lerp = [c](Rml::byte a, Rml::byte b) { return Rml::byte(std::lround(a + (b - a) * c)); };
        return { lerp(wave_blue.red, wave_red.red), lerp(wave_blue.green, wave_red.green),
                 lerp(wave_blue.blue, wave_red.blue), 255 };
    }

    // A grid of grid x grid vertices spanning the element. Vertex k = row * grid + col counts from the
    // bottom-right corner, as in the game's screen space. Each cell is split along the same diagonal, which gives
    // the shading its diagonal bands.
    class ColorWaveElement : public Rml::Element {
    public:
        explicit ColorWaveElement(const Rml::String& tag) : Rml::Element(tag) {}
    protected:
        void OnRender() override {
            Rml::Vector2f origin = GetAbsoluteOffset(Rml::BoxArea::Border);
            Rml::Vector2f size = GetBox().GetSize(Rml::BoxArea::Border);
            float t = seconds();

            Rml::Mesh mesh;
            mesh.vertices.resize(grid * grid);
            for (int row = 0; row < grid; row++) {
                for (int col = 0; col < grid; col++) {
                    int k = row * grid + col;
                    float c = 0.5f + 0.5f * std::sin(k * wave_rate * t);
                    Rml::Vertex& v = mesh.vertices[k];
                    v.position = origin + Rml::Vector2f(size.x * (1.0f - col / float(grid - 1)),
                                                        size.y * (1.0f - row / float(grid - 1)));
                    v.colour = mix(c);
                    v.tex_coord = Rml::Vector2f(0.0f, 0.0f);
                }
            }
            auto at = [](int row, int col) { return row * grid + col; };
            for (int row = 0; row < grid - 1; row++) {
                for (int col = 0; col < grid - 1; col++) {
                    mesh.indices.insert(mesh.indices.end(), {
                        at(row, col + 1), at(row + 1, col + 1), at(row + 1, col),
                        at(row, col + 1), at(row + 1, col), at(row, col) });
                }
            }
            GetRenderManager()->MakeGeometry(std::move(mesh)).Render(Rml::Vector2f(0.0f, 0.0f));
        }
    };

    void register_color_wave() {
        static Rml::ElementInstancerGeneric<ColorWaveElement> instancer;
        static bool registered = false;
        if (!registered) {
            registered = true;
            Rml::Factory::RegisterElementInstancer(color_wave_tag, &instancer);
        }
    }

    void fill_parent(recompui::Element* element) {
        element->set_position(recompui::Position::Absolute);
        element->set_top(0);
        element->set_left(0);
        element->set_width(100.0f, recompui::Unit::Percent);
        element->set_height(100.0f, recompui::Unit::Percent);
    }

    // The colour wave with the shade over it, filling parent.
    void create_backdrop_layers(recompui::Element* parent) {
        recompui::ContextId context = recompui::get_current_context();
        recompui::Element* wave = context.create_element<recompui::Element>(parent, 0, color_wave_tag, false);
        fill_parent(wave);
        recompui::Svg* shade = context.create_element<recompui::Svg>(parent, "launcher/shade.svg");
        fill_parent(shade);
    }

    // Backdrop behind the settings window, in the config modal's context. A negative z-index draws it before the
    // modal (RmlUi RenderOrder::StackNegative). It hides while a game runs, so the settings opened in game still
    // show the game behind them.
    class SettingsBackdrop : public recompui::Element {
    public:
        SettingsBackdrop(recompui::ResourceId rid, recompui::Element* parent) : Element(rid, parent, 0, "div", false) {
            using namespace recompui;
            // recompui has no z-index setter; set it as an inline style before the other properties.
            set_attribute("style", "z-index: -1;");
            set_position(Position::Absolute);
            set_top(0);
            set_right(0);
            set_bottom(0);
            set_left(0);
            create_backdrop_layers(this);
            queue_update();
        }
    protected:
        void process_event(const recompui::Event& e) override {
            if (e.type != recompui::EventType::Update) {
                return;
            }
            bool show = !ultramodern::is_game_started();
            if (show != shown) {
                shown = show;
                if (show) {
                    display_show();
                } else {
                    display_hide();
                }
            }
            queue_update();
        }
        std::string_view get_type_name() override { return "SettingsBackdrop"; }
    private:
        bool shown = true;
    };

    // The config modal is created after the launcher (RecompFrontend ui_state.cpp create_menus), so this runs on
    // the first launcher update. That update has the launcher context open, and contexts can't be nested.
    void create_settings_backdrop() {
        recompui::ContextId launcher_context = recompui::get_current_context();
        recompui::ContextId config_context = recompui::config::get_config_context_id();
        launcher_context.close();
        config_context.open();
        config_context.create_element<SettingsBackdrop>(config_context.get_root_element());
        config_context.close();
        launcher_context.open();
    }
}

void robotron64::launcher_animation_setup(recompui::LauncherMenu *menu) {
    using namespace recompui;
    auto context = get_current_context();

    register_color_wave();
    create_backdrop_layers(menu->get_background_container());

    // Wordmark in place of the default title (same anchor: centred, 25% from the top). White and lavender,
    // like the selected and unselected items of the game's menus.
    menu->remove_default_title();
    Element* title = context.create_element<Element>(menu, 0, "div", false);
    title->set_position(Position::Absolute);
    title->set_top(25.0f, Unit::Percent);
    title->set_left(50.0f, Unit::Percent);
    title->set_translate_2D(-50.0f, -50.0f, Unit::Percent);
    title->set_display(Display::Flex);
    title->set_flex_direction(FlexDirection::Column);
    title->set_align_items(AlignItems::Center);

    Label* name = context.create_element<Label>(title, "ROBOTRON 64", theme::Typography::Header1);
    name->set_font_family("Orbitron");
    name->set_font_weight(900);
    name->set_font_size(96.0f);
    name->set_letter_spacing(6.0f);
    name->set_color(theme::color::TextActive);

    subtitle = context.create_element<Label>(title, "RECOMPILED", theme::Typography::Header3);
    subtitle->set_font_family("Orbitron");
    subtitle->set_font_weight(700);
    subtitle->set_font_size(30.0f);
    subtitle->set_letter_spacing(18.0f);
    subtitle->set_color(theme::color::Primary);
}

void robotron64::launcher_animation_update(recompui::LauncherMenu *menu) {
    (void)menu;
    if (!settings_backdrop_created) {
        settings_backdrop_created = true;
        create_settings_backdrop();
    }
    if (subtitle != nullptr) {
        subtitle->set_opacity(0.8f + 0.2f * std::sin(seconds() * 2.0f));
    }
}
