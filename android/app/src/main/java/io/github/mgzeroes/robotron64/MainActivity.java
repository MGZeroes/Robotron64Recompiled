package io.github.mgzeroes.robotron64;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

// Android test build (ADR 007). Unpacks the UI assets into the app's files folder and points the game at it
// before SDL starts its native main. The ROM is copied there with adb as robotron64.us.z64.
public class MainActivity extends SDLActivity {
    private static final String TAG = "Robotron64";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File appFolder = getExternalFilesDir(null);
        try {
            Os.setenv("APP_FOLDER_PATH", appFolder.getAbsolutePath(), true);
            // RT64 looks up its data path from HOME on Linux-like systems.
            Os.setenv("HOME", appFolder.getAbsolutePath(), true);
            copyAssets("", appFolder);
        } catch (ErrnoException | IOException e) {
            Log.e(TAG, "Failed to set up the app folder", e);
        }
        super.onCreate(savedInstanceState);
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
            copyAssets(path.isEmpty() ? child : path + "/" + child, new File(target, child));
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
