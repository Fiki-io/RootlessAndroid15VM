package com.vm.engine;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * VMSurfaceView: Komponen tampilan layar virtual yang meneruskan render Surface
 * dan sentuhan pengguna langsung ke Engine C++.
 */
public class VMSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

    private final VMEngine mEngine;

    public VMSurfaceView(Context context) {
        this(context, null);
    }

    public VMSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mEngine = VMEngine.getInstance();
        getHolder().addCallback(this);
        setFocusable(true);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mEngine.setSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mEngine.setSurface(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mEngine.setSurface(null);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        float x = event.getX();
        float y = event.getY();

        // Teruskan koordinat sentuhan ke C++
        mEngine.sendTouchEvent(action, x, y);
        return true;
    }
}
