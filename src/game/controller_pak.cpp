// File-backed Controller Pak for controller port 1 (channel 0), the only one the game uses
// (decomp src/game/controller_pak_directory.c, pak_file_open.c).
//
// The pak is emulated at the level of the libultra file API (osPfsFindFile, osPfsReadWriteFile, ...),
// not as a raw 32 KiB filesystem image: 16 note slots and 123 data pages of 256 bytes, as on a real
// pak. Every change is written to <config>/saves/robotron64.us.controller_pak_1.bin with the runtime's
// backup helpers. These functions replace the runtime's PFS_ERR_NOPACK stubs (librecomp/src/pak.cpp);
// all of that file's symbols are defined here so the static library never links it in.

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

#include "recomp.h"
#include "librecomp/files.hpp"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"

namespace {
    constexpr s32 PFS_ERR_NOPACK = 1;
    constexpr s32 PFS_ERR_INVALID = 5;
    constexpr s32 PFS_DATA_FULL = 7;
    constexpr s32 PFS_DIR_FULL = 8;
    constexpr s32 PFS_ERR_EXIST = 9;
    constexpr s32 PFS_INITIALIZED = 1;
    constexpr u8 PFS_WRITE = 1;

    constexpr int max_notes = 16;
    constexpr int page_size = 256;
    constexpr int data_pages = 123;

    struct Note {
        bool used = false;
        u16 company_code = 0;
        u32 game_code = 0;
        std::array<u8, 16> game_name{};
        std::array<u8, 4> ext_name{};
        std::vector<u8> data;
    };

    constexpr char file_magic[8] = { 'R', '6', '4', 'C', 'P', 'A', 'K', '1' };

    std::mutex pak_mutex;
    std::array<Note, max_notes> notes;
    bool loaded = false;

    std::filesystem::path pak_path() {
        return recomp::get_config_path() / "saves" / "robotron64.us.controller_pak_1.bin";
    }

    template <typename T>
    void write_value(std::ofstream& out, T value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    template <typename T>
    bool read_value(std::ifstream& in, T& value) {
        return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(value)));
    }

    // Format (host byte order): magic, note count, then per used note: slot, company code,
    // game code, game name, extension, size, data.
    void load_pak() {
        loaded = true;
        std::ifstream in = recomp::open_input_file_with_backup(pak_path(), std::ios_base::binary);
        if (!in.good()) {
            return;
        }
        char magic[sizeof(file_magic)];
        u32 count = 0;
        if (!in.read(magic, sizeof(magic)) || std::memcmp(magic, file_magic, sizeof(magic)) != 0 || !read_value(in, count)) {
            fprintf(stderr, "[robotron] Controller Pak file %s is not valid; starting with an empty pak\n", pak_path().string().c_str());
            return;
        }
        std::array<Note, max_notes> loaded_notes;
        for (u32 i = 0; i < count; i++) {
            u32 slot = 0;
            u32 size = 0;
            Note note;
            note.used = true;
            if (!read_value(in, slot) || slot >= max_notes || !read_value(in, note.company_code) || !read_value(in, note.game_code) ||
                !in.read(reinterpret_cast<char*>(note.game_name.data()), note.game_name.size()) ||
                !in.read(reinterpret_cast<char*>(note.ext_name.data()), note.ext_name.size()) ||
                !read_value(in, size) || size > data_pages * page_size) {
                fprintf(stderr, "[robotron] Controller Pak file %s is truncated; starting with an empty pak\n", pak_path().string().c_str());
                return;
            }
            note.data.resize(size);
            if (!in.read(reinterpret_cast<char*>(note.data.data()), size)) {
                fprintf(stderr, "[robotron] Controller Pak file %s is truncated; starting with an empty pak\n", pak_path().string().c_str());
                return;
            }
            loaded_notes[slot] = std::move(note);
        }
        notes = std::move(loaded_notes);
    }

    void save_pak() {
        std::filesystem::path path = pak_path();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        {
            std::ofstream out = recomp::open_output_file_with_backup(path, std::ios_base::binary);
            if (!out.good()) {
                fprintf(stderr, "[robotron] Could not write Controller Pak file %s\n", path.string().c_str());
                return;
            }
            u32 count = 0;
            for (const Note& note : notes) {
                count += note.used ? 1 : 0;
            }
            out.write(file_magic, sizeof(file_magic));
            write_value(out, count);
            for (u32 slot = 0; slot < max_notes; slot++) {
                const Note& note = notes[slot];
                if (!note.used) {
                    continue;
                }
                write_value(out, slot);
                write_value(out, note.company_code);
                write_value(out, note.game_code);
                out.write(reinterpret_cast<const char*>(note.game_name.data()), note.game_name.size());
                out.write(reinterpret_cast<const char*>(note.ext_name.data()), note.ext_name.size());
                write_value(out, static_cast<u32>(note.data.size()));
                out.write(reinterpret_cast<const char*>(note.data.data()), note.data.size());
            }
        }
        if (!recomp::finalize_output_file_with_backup(path)) {
            fprintf(stderr, "[robotron] Could not finalize Controller Pak file %s\n", path.string().c_str());
        }
    }

    int used_pages() {
        int pages = 0;
        for (const Note& note : notes) {
            if (note.used) {
                pages += static_cast<int>(note.data.size()) / page_size;
            }
        }
        return pages;
    }

    // Reads `count` bytes from game memory at `addr` (RDRAM is stored byteswapped per word).
    template <size_t N>
    std::array<u8, N> read_bytes(uint8_t* rdram, gpr addr) {
        std::array<u8, N> out{};
        for (size_t i = 0; i < N; i++) {
            out[i] = MEM_B(i, addr);
        }
        return out;
    }

    int find_note(u16 company_code, u32 game_code, const std::array<u8, 16>& game_name, const std::array<u8, 4>& ext_name) {
        for (int i = 0; i < max_notes; i++) {
            const Note& note = notes[i];
            if (note.used && note.company_code == company_code && note.game_code == game_code &&
                note.game_name == game_name && note.ext_name == ext_name) {
                return i;
            }
        }
        return -1;
    }

    gpr arg_addr(recomp_context* ctx, int index) {
        return (&ctx->r4)[index];
    }

    gpr stack_arg_addr(uint8_t* rdram, recomp_context* ctx, int index) {
        return (gpr)(int32_t)MEM_W(index * 4, ctx->r29);
    }

    // Every call goes through here: the pak only exists on channel 0 (OSPfs.channel at offset 8).
    bool pak_ready(uint8_t* rdram, gpr pfs) {
        if (MEM_W(8, pfs) != 0) {
            return false;
        }
        if (!loaded) {
            load_pak();
        }
        return true;
    }
}

// osPfsIsPlug(OSMesgQueue*, u8* pattern): a pak is inserted in controller 1 only.
extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    gpr pattern = arg_addr(ctx, 1);
    MEM_B(0, pattern) = 0x01;
    _return<s32>(ctx, 0);
}

// osPfsInitPak(OSMesgQueue*, OSPfs*, int channel)
extern "C" void osPfsInitPak_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr mq = arg_addr(ctx, 0);
    gpr pfs = arg_addr(ctx, 1);
    s32 channel = _arg<2, s32>(rdram, ctx);
    if (channel != 0) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    MEM_W(0x0, pfs) = PFS_INITIALIZED;
    MEM_W(0x4, pfs) = (int32_t)mq;
    MEM_W(0x8, pfs) = channel;
    if (!loaded) {
        load_pak();
    }
    _return<s32>(ctx, 0);
}

// __osGetId(OSPfs*): called directly by the game's legacy pak menu (func_8004C0B0). The pak ID is
// always valid here.
extern "C" void __osGetId_recomp(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, 0);
}

extern "C" void osPfsRepairId_recomp(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, 0);
}

extern "C" void osPfsChecker_recomp(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, 0);
}

// osPfsFindFile(OSPfs*, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, s32* file_no)
extern "C" void osPfsFindFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    auto game_name = read_bytes<16>(rdram, arg_addr(ctx, 3));
    auto ext_name = read_bytes<4>(rdram, stack_arg_addr(rdram, ctx, 4));
    gpr file_no = stack_arg_addr(rdram, ctx, 5);

    int slot = find_note(company_code, game_code, game_name, ext_name);
    if (slot < 0) {
        _return<s32>(ctx, PFS_ERR_INVALID);
        return;
    }
    MEM_W(0, file_no) = slot;
    _return<s32>(ctx, 0);
}

// osPfsAllocateFile(OSPfs*, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, int nbytes, s32* file_no)
extern "C" void osPfsAllocateFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    auto game_name = read_bytes<16>(rdram, arg_addr(ctx, 3));
    auto ext_name = read_bytes<4>(rdram, stack_arg_addr(rdram, ctx, 4));
    s32 nbytes = _arg<5, s32>(rdram, ctx);
    gpr file_no = stack_arg_addr(rdram, ctx, 6);

    if (nbytes <= 0) {
        _return<s32>(ctx, PFS_ERR_INVALID);
        return;
    }
    if (find_note(company_code, game_code, game_name, ext_name) >= 0) {
        _return<s32>(ctx, PFS_ERR_EXIST);
        return;
    }
    int pages = (nbytes + page_size - 1) / page_size;
    if (pages > data_pages - used_pages()) {
        _return<s32>(ctx, PFS_DATA_FULL);
        return;
    }
    int slot = 0;
    while (slot < max_notes && notes[slot].used) {
        slot++;
    }
    if (slot == max_notes) {
        _return<s32>(ctx, PFS_DIR_FULL);
        return;
    }

    Note& note = notes[slot];
    note.used = true;
    note.company_code = company_code;
    note.game_code = game_code;
    note.game_name = game_name;
    note.ext_name = ext_name;
    note.data.assign(pages * page_size, 0);
    save_pak();

    MEM_W(0, file_no) = slot;
    _return<s32>(ctx, 0);
}

// osPfsDeleteFile(OSPfs*, u16 company_code, u32 game_code, u8* game_name, u8* ext_name)
extern "C" void osPfsDeleteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    auto game_name = read_bytes<16>(rdram, arg_addr(ctx, 3));
    auto ext_name = read_bytes<4>(rdram, stack_arg_addr(rdram, ctx, 4));

    int slot = find_note(company_code, game_code, game_name, ext_name);
    if (slot < 0) {
        _return<s32>(ctx, PFS_ERR_INVALID);
        return;
    }
    notes[slot] = Note{};
    save_pak();
    _return<s32>(ctx, 0);
}

// osPfsFileState(OSPfs*, s32 file_no, OSPfsState*). OSPfsState: u32 file_size (0x0),
// u32 game_code (0x4), u16 company_code (0x8), char ext_name[4] (0xA), char game_name[16] (0xE).
extern "C" void osPfsFileState_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    s32 slot = _arg<1, s32>(rdram, ctx);
    gpr state = arg_addr(ctx, 2);
    if (slot < 0 || slot >= max_notes || !notes[slot].used) {
        _return<s32>(ctx, PFS_ERR_INVALID);
        return;
    }
    const Note& note = notes[slot];
    MEM_W(0x0, state) = static_cast<int32_t>(note.data.size());
    MEM_W(0x4, state) = static_cast<int32_t>(note.game_code);
    MEM_H(0x8, state) = static_cast<int16_t>(note.company_code);
    for (size_t i = 0; i < note.ext_name.size(); i++) {
        MEM_B(0xA + i, state) = note.ext_name[i];
    }
    for (size_t i = 0; i < note.game_name.size(); i++) {
        MEM_B(0xE + i, state) = note.game_name[i];
    }
    _return<s32>(ctx, 0);
}

// osPfsFreeBlocks(OSPfs*, s32* bytes_not_used)
extern "C" void osPfsFreeBlocks_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    MEM_W(0, arg_addr(ctx, 1)) = (data_pages - used_pages()) * page_size;
    _return<s32>(ctx, 0);
}

// osPfsNumFiles(OSPfs*, s32* max_files, s32* files_used)
extern "C" void osPfsNumFiles_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    s32 files_used = 0;
    for (const Note& note : notes) {
        files_used += note.used ? 1 : 0;
    }
    MEM_W(0, arg_addr(ctx, 1)) = max_notes;
    MEM_W(0, arg_addr(ctx, 2)) = files_used;
    _return<s32>(ctx, 0);
}

// osPfsReadWriteFile(OSPfs*, s32 file_no, u8 flag, int offset, int nbytes, u8* data_buffer)
extern "C" void osPfsReadWriteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = arg_addr(ctx, 0);
    if (!pak_ready(rdram, pfs)) {
        _return<s32>(ctx, PFS_ERR_NOPACK);
        return;
    }
    s32 slot = _arg<1, s32>(rdram, ctx);
    u8 flag = _arg<2, u8>(rdram, ctx);
    s32 offset = _arg<3, s32>(rdram, ctx);
    s32 nbytes = _arg<4, s32>(rdram, ctx);
    gpr buffer = stack_arg_addr(rdram, ctx, 5);

    if (slot < 0 || slot >= max_notes || !notes[slot].used || offset < 0 || nbytes < 0 ||
        static_cast<size_t>(offset) + static_cast<size_t>(nbytes) > notes[slot].data.size()) {
        _return<s32>(ctx, PFS_ERR_INVALID);
        return;
    }
    std::vector<u8>& data = notes[slot].data;
    if (flag == PFS_WRITE) {
        for (s32 i = 0; i < nbytes; i++) {
            data[offset + i] = MEM_B(i, buffer);
        }
        save_pak();
    }
    else {
        for (s32 i = 0; i < nbytes; i++) {
            MEM_B(i, buffer) = data[offset + i];
        }
    }
    _return<s32>(ctx, 0);
}
