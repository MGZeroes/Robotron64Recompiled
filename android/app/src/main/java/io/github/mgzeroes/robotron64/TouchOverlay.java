package io.github.mgzeroes.robotron64;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

// On-screen controls (ADR 008): a stick for each thumb and a few buttons, drawn over the game. The state goes to
// android_touch.cpp, which feeds a virtual SDL gamepad. Numbers below match SDL_GameControllerButton.
public class TouchOverlay extends View {
    private static final int BUTTON_A = 0;
    private static final int BUTTON_B = 1;
    private static final int BUTTON_START = 6;
    private static final int BUTTON_L = 9;

    private static final float STICK_RADIUS_DP = 70;
    private static final float STICK_MARGIN_DP = 130;
    private static final float BUTTON_RADIUS_DP = 30;
    private static final float DEAD_ZONE = 0.12f;
    private static final int NO_POINTER = -1;

    private static native void nativeSetState(int buttons, float leftX, float leftY, float rightX, float rightY);
    static native void nativeSetEnabled(boolean enabled);
    private static native boolean nativeIsMenuOpen();

    private static final class Stick {
        int pointer = NO_POINTER;
        float centerX, centerY, knobX, knobY;
        float valueX, valueY;
    }

    private static final class Button {
        final int id;
        final String label;
        float x, y, radius;
        int pointer = NO_POINTER;

        Button(int id, String label) {
            this.id = id;
            this.label = label;
        }
    }

    private final Stick moveStick = new Stick();
    private final Stick aimStick = new Stick();
    private final Button[] buttons = {
        new Button(BUTTON_L, "L"), new Button(BUTTON_START, "START"), new Button(BUTTON_B, "B"), new Button(BUTTON_A, "A"),
    };
    private final float density;
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint outline = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);

    public TouchOverlay(Context context) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        fill.setColor(0x40FFFFFF);
        outline.setColor(0x80FFFFFF);
        outline.setStyle(Paint.Style.STROKE);
        outline.setStrokeWidth(2 * density);
        text.setColor(0xB0FFFFFF);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(14 * density);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        float margin = STICK_MARGIN_DP * density;
        restStick(moveStick, margin, height - margin);
        restStick(aimStick, width - margin, height - margin);

        float radius = BUTTON_RADIUS_DP * density;
        float top = 2 * radius;
        placeButton(BUTTON_L, 3 * radius, top, radius);
        placeButton(BUTTON_START, width / 2f, top, radius);
        placeButton(BUTTON_B, width - 6 * radius, top, radius);
        placeButton(BUTTON_A, width - 3 * radius, top, radius);
    }

    private void placeButton(int id, float x, float y, float radius) {
        for (Button button : buttons) {
            if (button.id == id) {
                button.x = x;
                button.y = y;
                button.radius = radius;
            }
        }
    }

    private void restStick(Stick stick, float x, float y) {
        stick.centerX = stick.knobX = x;
        stick.centerY = stick.knobY = y;
        stick.valueX = stick.valueY = 0;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        drawStick(canvas, moveStick);
        drawStick(canvas, aimStick);
        for (Button button : buttons) {
            canvas.drawCircle(button.x, button.y, button.radius, button.pointer != NO_POINTER ? outline : fill);
            canvas.drawCircle(button.x, button.y, button.radius, outline);
            canvas.drawText(button.label, button.x, button.y + text.getTextSize() / 3, text);
        }
    }

    private void drawStick(Canvas canvas, Stick stick) {
        float radius = STICK_RADIUS_DP * density;
        canvas.drawCircle(stick.centerX, stick.centerY, radius, outline);
        canvas.drawCircle(stick.knobX, stick.knobY, radius * 0.4f, fill);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int index = event.getActionIndex();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                if (!pointerDown(event.getPointerId(index), event.getX(index), event.getY(index))) {
                    return false;
                }
                break;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    moveStickPointer(moveStick, event, i);
                    moveStickPointer(aimStick, event, i);
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                pointerUp(event.getPointerId(index));
                break;
            case MotionEvent.ACTION_CANCEL:
                releaseAll();
                return true;
            default:
                return true;
        }
        sendState();
        invalidate();
        return true;
    }

    // Returns false when the touch isn't for the overlay, so it reaches the menu underneath.
    private boolean pointerDown(int pointer, float x, float y) {
        for (Button button : buttons) {
            float dx = x - button.x;
            float dy = y - button.y;
            // A little larger than drawn: thumbs don't aim well without something to feel.
            float reach = button.radius * 1.3f;
            if (dx * dx + dy * dy <= reach * reach) {
                button.pointer = pointer;
                return true;
            }
        }
        if (nativeIsMenuOpen()) {
            return false;
        }
        Stick stick = x < getWidth() / 2f ? moveStick : aimStick;
        if (stick.pointer == NO_POINTER) {
            stick.pointer = pointer;
            stick.centerX = stick.knobX = x;
            stick.centerY = stick.knobY = y;
        }
        return true;
    }

    private void moveStickPointer(Stick stick, MotionEvent event, int index) {
        if (event.getPointerId(index) != stick.pointer) {
            return;
        }
        float radius = STICK_RADIUS_DP * density;
        float dx = (event.getX(index) - stick.centerX) / radius;
        float dy = (event.getY(index) - stick.centerY) / radius;
        float length = (float) Math.hypot(dx, dy);
        if (length > 1) {
            dx /= length;
            dy /= length;
        }
        stick.knobX = stick.centerX + dx * radius;
        stick.knobY = stick.centerY + dy * radius;
        boolean centered = length < DEAD_ZONE;
        stick.valueX = centered ? 0 : dx;
        stick.valueY = centered ? 0 : dy;
    }

    private void pointerUp(int pointer) {
        for (Button button : buttons) {
            if (button.pointer == pointer) {
                button.pointer = NO_POINTER;
            }
        }
        if (moveStick.pointer == pointer) {
            releaseStick(moveStick, true);
        }
        if (aimStick.pointer == pointer) {
            releaseStick(aimStick, false);
        }
    }

    private void releaseStick(Stick stick, boolean left) {
        float margin = STICK_MARGIN_DP * density;
        stick.pointer = NO_POINTER;
        restStick(stick, left ? margin : getWidth() - margin, getHeight() - margin);
    }

    // Also for pausing and for a controller taking over, so that no input stays held.
    void releaseAll() {
        for (Button button : buttons) {
            button.pointer = NO_POINTER;
        }
        releaseStick(moveStick, true);
        releaseStick(aimStick, false);
        sendState();
        invalidate();
    }

    private void sendState() {
        int mask = 0;
        for (Button button : buttons) {
            if (button.pointer != NO_POINTER) {
                mask |= 1 << button.id;
            }
        }
        nativeSetState(mask, moveStick.valueX, moveStick.valueY, aimStick.valueX, aimStick.valueY);
    }
}
