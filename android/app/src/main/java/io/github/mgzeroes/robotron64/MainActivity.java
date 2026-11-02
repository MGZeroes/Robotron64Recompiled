package io.github.mgzeroes.robotron64;

import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

// Android test build (ADR 007). Unpacks the UI assets into the app's files folder and points the game at it
// before SDL starts its native main. The ROM and mods come through the system file picker (ADR 009).
public class MainActivity extends SDLActivity implements InputManager.InputDeviceListener {
    private static final String TAG = "Robotron64";

    // What the game needs; the root of the APK's assets also lists Android's own files.
    private static final String[] GAME_ASSETS = { "assets", "recompcontrollerdb.txt" };

    private static final int REQUEST_PICK_FILES = 1;

    private static native void nativePickDone(String folder, String[] paths);

    private TouchOverlay touchOverlay;
    // The app starts in the launcher, so the overlay doesn't flash there at launch.
    private boolean menuOpen = true;

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

    // Called from android_touch.cpp.
    public void setMenuOpen(boolean open) {
        runOnUiThread(() -> {
            menuOpen = open;
            updateTouchOverlay();
        });
    }

    // The on-screen controls are for when no controller is connected, and not for menus, which work by tapping.
    private void updateTouchOverlay() {
        boolean controller = hasController();
        boolean show = !controller && !menuOpen;
        if (!show) {
            touchOverlay.releaseAll();
        }
        touchOverlay.setVisibility(show ? android.view.View.VISIBLE : android.view.View.GONE);
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

    // Called from android_files.cpp, which gets the result through nativePickDone.
    public void pickFiles(boolean multiple) {
        runOnUiThread(() -> {
            // ROMs and mods have no MIME type of their own, so anything can be picked. The game checks the content.
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, multiple);
            try {
                startActivityForResult(intent, REQUEST_PICK_FILES);
            } catch (ActivityNotFoundException e) {
                Log.e(TAG, "No file picker on this device", e);
                Toast.makeText(this, "No file picker on this device", Toast.LENGTH_LONG).show();
                nativePickDone(null, null);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_PICK_FILES) {
            return;
        }
        List<Uri> uris = new ArrayList<>();
        if (resultCode == RESULT_OK && data != null) {
            ClipData clip = data.getClipData();
            if (clip != null) {
                for (int i = 0; i < clip.getItemCount(); i++) {
                    uris.add(clip.getItemAt(i).getUri());
                }
            } else if (data.getData() != null) {
                uris.add(data.getData());
            }
        }
        if (uris.isEmpty()) {
            nativePickDone(null, null);
            return;
        }
        new Thread(() -> copyPickedFiles(uris), "PickFiles").start();
    }

    // The picker gives content:// Uris and the game reads paths, so the files go through the cache. Android may serve
    // them from the network, hence not on the UI thread.
    private void copyPickedFiles(List<Uri> uris) {
        File folder = new File(getCacheDir(), "picked");
        deleteRecursively(folder);
        try {
            String[] paths = new String[uris.size()];
            for (int i = 0; i < paths.length; i++) {
                Uri uri = uris.get(i);
                // A folder per file, since two picked files can have the same name.
                File target = new File(new File(folder, Integer.toString(i)), displayName(uri));
                target.getParentFile().mkdirs();
                try (InputStream in = getContentResolver().openInputStream(uri); OutputStream out = new FileOutputStream(target)) {
                    if (in == null) {
                        throw new IOException("No stream for " + uri);
                    }
                    copyStream(in, out);
                }
                paths[i] = target.getAbsolutePath();
            }
            nativePickDone(folder.getAbsolutePath(), paths);
        } catch (IOException | SecurityException e) {
            Log.e(TAG, "Failed to copy the picked files", e);
            deleteRecursively(folder);
            runOnUiThread(() -> Toast.makeText(this, "Could not read the selected file", Toast.LENGTH_LONG).show());
            nativePickDone(null, null);
        }
    }

    // The mod installer needs the real name: it checks the extension and installs under that name.
    private String displayName(Uri uri) {
        String name = null;
        String[] columns = { OpenableColumns.DISPLAY_NAME };
        try (Cursor cursor = getContentResolver().query(uri, columns, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                name = cursor.getString(0);
            }
        }
        if (name == null) {
            name = uri.getLastPathSegment();
        }
        // The name comes from another app, so it must not lead out of the folder.
        name = name == null ? "" : new File(name).getName();
        return name.isEmpty() || name.equals(".") || name.equals("..") ? "picked" : name;
    }

    private void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        file.delete();
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
            copyStream(in, out);
        }
    }

    private void copyStream(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[65536];
        int count;
        while ((count = in.read(buffer)) > 0) {
            out.write(buffer, 0, count);
        }
    }
}
