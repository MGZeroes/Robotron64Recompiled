package io.github.mgzeroes.robotron64;

import android.content.res.AssetManager;
import android.hardware.input.InputManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

// Android test build (ADR 007). Unpacks the UI assets into the app's files folder and points the game at it
// before SDL starts its native main. The ROM is copied there with adb as robotron64.us.z64.
public class MainActivity extends SDLActivity implements InputManager.InputDeviceListener {
    private static final String TAG = "Robotron64";

    // What the game needs; the root of the APK's assets also lists Android's own files.
    private static final String[] GAME_ASSETS = { "assets", "recompcontrollerdb.txt" };

    private TouchOverlay touchOverlay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File appFolder = getExternalFilesDir(null);
        try {
            Os.setenv("APP_FOLDER_PATH", appFolder.getAbsolutePath(), true);
            // RT64 looks up its data path from HOME on Linux-like systems.
            Os.setenv("HOME", appFolder.getAbsolutePath(), true);
            for (String asset : GAME_ASSETS) {
                copyAssets(asset, new File(appFolder, asset));
            }
        } catch (ErrnoException | IOException e) {
            Log.e(TAG, "Failed to set up the app folder", e);
        }
        super.onCreate(savedInstanceState);

        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(attributes);
        hideSystemBars();

        if (!mBrokenLibraries) {
            touchOverlay = new TouchOverlay(this);
            ((ViewGroup) getContentView()).addView(touchOverlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            InputManager inputManager = (InputManager) getSystemService(INPUT_SERVICE);
            inputManager.registerInputDeviceListener(this, new Handler(Looper.getMainLooper()));
            updateTouchOverlay();
        }
    }

    @Override
    protected void onDestroy() {
        if (touchOverlay != null) {
            ((InputManager) getSystemService(INPUT_SERVICE)).unregisterInputDeviceListener(this);
        }
        super.onDestroy();
    }

    @Override
    protected void onPause() {
        if (touchOverlay != null) {
            touchOverlay.releaseAll();
        }
        super.onPause();
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        updateTouchOverlay();
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        updateTouchOverlay();
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        updateTouchOverlay();
    }

    // The on-screen controls are for when no controller is connected.
    private void updateTouchOverlay() {
        boolean controller = hasController();
        if (controller) {
            touchOverlay.releaseAll();
        }
        touchOverlay.setVisibility(controller ? android.view.View.GONE : android.view.View.VISIBLE);
        TouchOverlay.nativeSetEnabled(!controller);
    }

    // Requires a real analog stick: phones have button devices that also call themselves gamepads.
    private boolean hasController() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device != null && !device.isVirtual()
                    && (device.getSources() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                    && device.getMotionRange(MotionEvent.AXIS_X, InputDevice.SOURCE_JOYSTICK) != null) {
                return true;
            }
        }
        return false;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
                | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "Robotron64" };
    }

    private void copyAssets(String path, File target) throws IOException {
        AssetManager assets = getAssets();
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            copyFile(assets, path, target);
            return;
        }
        target.mkdirs();
        for (String child : children) {
            copyAssets(path + "/" + child, new File(target, child));
        }
    }

    private void copyFile(AssetManager assets, String path, File target) throws IOException {
        try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(target)) {
            byte[] buffer = new byte[65536];
            int count;
            while ((count = in.read(buffer)) > 0) {
                out.write(buffer, 0, count);
            }
        }
    }
}
