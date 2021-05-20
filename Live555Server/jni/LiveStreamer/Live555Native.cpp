#include "Live555Native.h"
#include "live_streamer.h"

extern "C"
{
JNIEXPORT jboolean JNICALL Java_com_samsung_link_stream_service_Live555Native_initialize
        (JNIEnv *, jobject, jint, jint);
JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_loopNative
        (JNIEnv *, jobject);
JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_stopNative
        (JNIEnv *, jobject);
JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_feedH264Data
        (JNIEnv *, jobject, jbyteArray);
JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_destroy
        (JNIEnv *, jobject);
JNIEXPORT jbyteArray JNICALL Java_com_samsung_link_stream_service_Live555Native_yuvToBuffer
        (JNIEnv *env, jobject, jobject, jobject, jobject, jint, jint, jint, jint, jint, jint, jint,
         jint);
}

LiveStreamer *getInstance(JNIEnv *env, jobject jthiz) {
    const char *fieldName = "NativeInstance";
    jclass cls = env->GetObjectClass(jthiz);
    jfieldID instanceFieldId = env->GetFieldID(cls, fieldName, "J");
    if (instanceFieldId == NULL) {
        LOGI("LiveStreamer_Native has no long field named with: NativeInstance");
        return NULL;
    }
    jlong instanceValue = env->GetLongField(jthiz, instanceFieldId);
    if (instanceValue == 0) {
        LOGI("instanceValue NULL ");
        return NULL;
    } else {
        LOGI("instanceValue NOT NULL ");
    }
    return (LiveStreamer *) instanceValue;
}

void storeInstance(JNIEnv *env, jobject jthiz, LiveStreamer *instance) {
    const char *fieldName = "NativeInstance";
    jclass cls = env->GetObjectClass(jthiz);
    jfieldID instanceFieldId = env->GetFieldID(cls, fieldName, "J");
    if (instanceFieldId == NULL) {
        LOGI("LiveStreamer_Native has no long field named with: NativeInstance");
        return;
    }
    jlong value = (instance == NULL) ? 0 : (jlong) instance;
    env->SetLongField(jthiz, instanceFieldId, value);
}


JNIEXPORT jboolean JNICALL Java_com_samsung_link_stream_service_Live555Native_initialize
        (JNIEnv *env, jobject jthiz, jint fps, jint port) {
    LiveStreamer *streamer = new LiveStreamer(fps, port);
    storeInstance(env, jthiz, streamer);
    return streamer->init();
}

JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_loopNative
        (JNIEnv *env, jobject jthiz) {
    LiveStreamer *streamer = getInstance(env, jthiz);
    if (streamer != NULL) streamer->loop();
}


JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_stopNative
        (JNIEnv *env, jobject jthiz) {
    LiveStreamer *streamer = getInstance(env, jthiz);
    if (streamer != NULL) streamer->stop();
}

JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_feedH264Data
        (JNIEnv *env, jobject jthiz, jbyteArray dataArray) {
    LiveStreamer *streamer = getInstance(env, jthiz);
    if (streamer == NULL) return;
    int len = env->GetArrayLength(dataArray);
    char *buf = new char[len];
    env->GetByteArrayRegion(dataArray, 0, len, reinterpret_cast<jbyte *>(buf));
    streamer->dataPushed(buf, len);
}

JNIEXPORT void JNICALL Java_com_samsung_link_stream_service_Live555Native_destroy
        (JNIEnv *env, jobject jthiz) {
    LiveStreamer *streamer = getInstance(env, jthiz);
    if (streamer != NULL) {
        delete streamer;
        streamer = 0;
        storeInstance(env, jthiz, NULL);
    }
}

bool SPtoI420(const uint8_t *src, uint8_t *dst, int width, int height, bool isNV21) {
    if (!src || !dst) {
        return false;
    }

    unsigned int YSize = width * height;
    unsigned int UVSize = (YSize >> 1);

    // NV21: Y..Y + VUV...U
    const uint8_t *pSrcY = src;
    const uint8_t *pSrcUV = src + YSize;

    // I420: Y..Y + U.U + V.V
    uint8_t *pDstY = dst;
    uint8_t *pDstU = dst + YSize;
    uint8_t *pDstV = dst + YSize + (UVSize >> 1);

    // copy Y
    memcpy(pDstY, pSrcY, YSize);

    // copy U and V
    for (int k = 0; k < (UVSize >> 1); k++) {
        if (isNV21) {
            pDstV[k] = pSrcUV[k * 2];     // copy V
            pDstU[k] = pSrcUV[k * 2 + 1];   // copy U
        } else {
            pDstU[k] = pSrcUV[k * 2];     // copy V
            pDstV[k] = pSrcUV[k * 2 + 1];   // copy U
        }
    }

    return true;
}

JNIEXPORT jbyteArray JNICALL Java_com_samsung_link_stream_service_Live555Native_yuvToBuffer
        (JNIEnv *env, jobject instance, jobject yPlane, jobject uPlane, jobject vPlane,
         jint yPixelStride, jint yRowStride, jint uPixelStride, jint uRowStride, jint vPixelStride,
         jint vRowStride, jint imgWidth, jint imgHeight) {
    auto *bbuf_yIn = static_cast<uint8_t *>(env->GetDirectBufferAddress(yPlane));
    auto *bbuf_uIn = static_cast<uint8_t *>(env->GetDirectBufferAddress(uPlane));
    auto *bbuf_vIn = static_cast<uint8_t *>(env->GetDirectBufferAddress(vPlane));

    auto *buf = (uint8_t *) malloc(sizeof(uint8_t) * imgWidth * imgHeight +
                                   2 * (imgWidth + 1) / 2 * (imgHeight + 1) / 2);

    bool isNV21;
    if (yPixelStride == 1) {
        // All pixels in a row are contiguous; copy one line at a time.
        for (int y = 0; y < imgHeight; y++)
            memcpy(buf + y * imgWidth, bbuf_yIn + y * yRowStride,
                   static_cast<size_t>(imgWidth));
    } else {
        // Highly improbable, but not disallowed by the API. In this case
        // individual pixels aren't stored consecutively but sparsely with
        // other data inbetween each pixel.
        for (int y = 0; y < imgHeight; y++)
            for (int x = 0; x < imgWidth; x++)
                buf[y * imgWidth + x] = bbuf_yIn[y * yRowStride + x * yPixelStride];
    }

    uint8_t *chromaBuf = &buf[imgWidth * imgHeight];
    int chromaBufStride = 2 * ((imgWidth + 1) / 2);
    if (uPixelStride == 2 && vPixelStride == 2 &&
        uRowStride == vRowStride && bbuf_vIn == bbuf_uIn + 1) {
        isNV21 = true;
        // The actual cb/cr planes happened to be laid out in
        // exact NV21 form in memory; copy them as is
        for (int y = 0; y < (imgHeight + 1) / 2; y++)
            memcpy(chromaBuf + y * chromaBufStride, bbuf_vIn + y * vRowStride,
                   static_cast<size_t>(chromaBufStride));
    } else if (vPixelStride == 2 && uPixelStride == 2 &&
               uRowStride == vRowStride && bbuf_vIn == bbuf_uIn + 1) {
        isNV21 = false;
        // The cb/cr planes happened to be laid out in exact NV12 form
        // in memory; if the destination API can use NV12 in addition to
        // NV21 do something similar as above, but using cbPtr instead of crPtr.
        // If not, remove this clause and use the generic code below.
    } else {
        isNV21 = true;
        if (vPixelStride == 1 && uPixelStride == 1) {
            // Continuous cb/cr planes; the input data was I420/YV12 or similar;
            // copy it into NV21 form
            for (int y = 0; y < (imgHeight + 1) / 2; y++) {
                for (int x = 0; x < (imgWidth + 1) / 2; x++) {
                    chromaBuf[y * chromaBufStride + 2 * x + 0] = bbuf_vIn[y * vRowStride + x];
                    chromaBuf[y * chromaBufStride + 2 * x + 1] = bbuf_uIn[y * uRowStride + x];
                }
            }
        } else {
            // Generic data copying into NV21
            for (int y = 0; y < (imgHeight + 1) / 2; y++) {
                for (int x = 0; x < (imgWidth + 1) / 2; x++) {
                    chromaBuf[y * chromaBufStride + 2 * x + 0] = bbuf_vIn[y * vRowStride +
                                                                          x * uPixelStride];
                    chromaBuf[y * chromaBufStride + 2 * x + 1] = bbuf_uIn[y * uRowStride +
                                                                          x * vPixelStride];
                }
            }
        }
    }

    uint8_t *I420Buff = (uint8_t *) malloc(sizeof(uint8_t) * imgWidth * imgHeight +
                                           2 * (imgWidth + 1) / 2 * (imgHeight + 1) / 2);
    SPtoI420(buf, I420Buff, imgWidth, imgHeight, isNV21);

    jbyteArray ret = env->NewByteArray(imgWidth * imgHeight *
                                       3 / 2);
    env->SetByteArrayRegion(ret, 0, imgWidth * imgHeight *
                                    3 / 2, (jbyte *) I420Buff);
    free(buf);
    free(I420Buff);
    return ret;
}