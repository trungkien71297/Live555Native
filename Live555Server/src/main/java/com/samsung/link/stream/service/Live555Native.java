package com.samsung.link.stream.service;

import java.nio.ByteBuffer;

public class Live555Native implements Runnable {
    private long NativeInstance = 0;

    static{
        System.loadLibrary("Live555");
    }
    private Thread thread = null;

    public Live555Native() {
        thread = new Thread(this);
        thread.setName("Native-Thread");
    }
    public native boolean initialize(int fps,int port);
    private native void loopNative();
    public native void stopNative();
    public native byte[] yuvToBuffer(ByteBuffer yPlane, ByteBuffer uPlane, ByteBuffer vPlane, int p1, int r1, int p2, int r2, int p3, int r3, int width, int height);
    public native void feedH264Data(byte[] data);
    public native void destroy();

    public void run() {
        loopNative();
    }

    public void doLoop(){
        thread.start();
    }

    public void stop(){
        stopNative();
    }
}
