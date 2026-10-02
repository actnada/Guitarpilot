#include <jni.h>
#include <memory>
#include <utility>
#include "GuitarEngine.h"

static std::unique_ptr<GuitarEngine> gEngine;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_create(
    JNIEnv*, jobject) {

    // La instancia pertenece a la sesión/proceso, no a una Activity concreta.
    // Una rotación puede recrear MainActivity sin reiniciar el transporte.
    if (gEngine) return JNI_TRUE;

    auto engine = std::make_unique<GuitarEngine>();
    if (!engine->open()) return JNI_FALSE;
    gEngine = std::move(engine);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_isCreated(
    JNIEnv*, jobject) {
    return gEngine ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_destroy(
    JNIEnv*, jobject) {

    if (gEngine) {
        gEngine->close();
        gEngine.reset();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_loadSample(
    JNIEnv* env,
    jobject,
    jint index,
    jint stringNumber,
    jint fret,
    jint layer,
    jshortArray pcm16,
    jint sampleRate) {

    if (!gEngine || !pcm16) return;

    const jsize length =
        env->GetArrayLength(pcm16);

    jshort* data =
        env->GetShortArrayElements(
            pcm16,
            nullptr);

    gEngine->loadSample(
        index,
        stringNumber,
        fret,
        layer,
        reinterpret_cast<int16_t*>(data),
        length,
        sampleRate);

    env->ReleaseShortArrayElements(
        pcm16,
        data,
        JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_play(
    JNIEnv*, jobject) {

    if (gEngine) gEngine->play();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_pause(
    JNIEnv*, jobject) {

    if (gEngine) gEngine->pause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_stop(
    JNIEnv*, jobject) {

    if (gEngine) gEngine->stop();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_setChord(
    JNIEnv*, jobject, jint value) {

    if (gEngine) gEngine->setChord(value);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_setBpm(
    JNIEnv*, jobject, jint value) {

    if (gEngine) gEngine->setBpm(value);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_setStringDelayMs(
    JNIEnv*, jobject, jint value) {

    if (gEngine) gEngine->setStringDelayMs(value);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_setPattern(
    JNIEnv*, jobject, jint value) {

    if (gEngine) gEngine->setPattern(value);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_setMasterVolume(
    JNIEnv*, jobject, jfloat value) {

    if (gEngine) gEngine->setMasterVolume(value);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_manualStrumDown(
    JNIEnv*, jobject) {

    if (gEngine) gEngine->manualStrumDown();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_guitarpilot_NativeAudioEngine_manualStrumUp(
    JNIEnv*, jobject) {

    if (gEngine) gEngine->manualStrumUp();
}
