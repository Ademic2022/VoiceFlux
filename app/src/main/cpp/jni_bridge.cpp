#include <jni.h>
#include <android/log.h>
#include "audio_engine.h"

// Singleton engine owned by the JNI layer.
static voiceflux::AudioEngine* g_engine = nullptr;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_voiceflux_app_AudioEngine_nativeCreate(JNIEnv*, jobject) {
    if (g_engine) return JNI_TRUE;
    g_engine = new voiceflux::AudioEngine();
    return g_engine->initialize() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeDelete(JNIEnv*, jobject) {
    delete g_engine;
    g_engine = nullptr;
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeStart(JNIEnv*, jobject) {
    if (g_engine) g_engine->start();
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeStop(JNIEnv*, jobject) {
    if (g_engine) g_engine->stop();
}

JNIEXPORT jboolean JNICALL
Java_com_voiceflux_app_AudioEngine_nativeIsRunning(JNIEnv*, jobject) {
    return (g_engine && g_engine->isRunning()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetPreset(JNIEnv*, jobject, jint presetId) {
    if (g_engine)
        g_engine->processor().applyPreset(
            static_cast<voiceflux::PresetId>(presetId));
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetPitch(JNIEnv*, jobject, jfloat semitones) {
    if (g_engine) g_engine->processor().params().pitchSemitones.store(semitones);
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetFormant(JNIEnv*, jobject, jfloat semitones) {
    if (g_engine) g_engine->processor().params().formantSemitones.store(semitones);
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetReverb(JNIEnv*, jobject, jfloat amount) {
    if (g_engine) g_engine->processor().params().reverbMix.store(amount);
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetDistortion(JNIEnv*, jobject, jfloat amount) {
    if (g_engine) g_engine->processor().params().distortionAmount.store(amount);
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetEcho(JNIEnv*, jobject, jfloat amount) {
    if (g_engine) g_engine->processor().params().echoAmount.store(amount);
}

JNIEXPORT void JNICALL
Java_com_voiceflux_app_AudioEngine_nativeSetGain(JNIEnv*, jobject, jfloat gain) {
    if (g_engine) g_engine->processor().params().gain.store(gain);
}

JNIEXPORT jfloat JNICALL
Java_com_voiceflux_app_AudioEngine_nativeGetLatencyMs(JNIEnv*, jobject) {
    return g_engine ? g_engine->getLatencyMs() : 0.f;
}

JNIEXPORT jint JNICALL
Java_com_voiceflux_app_AudioEngine_nativeGetWaveformData(
        JNIEnv* env, jobject, jfloatArray jBuf) {
    if (!g_engine) return 0;
    const jsize len  = env->GetArrayLength(jBuf);
    jfloat* raw = env->GetFloatArrayElements(jBuf, nullptr);
    const int n = g_engine->getWaveformData(raw, static_cast<int>(len));
    env->ReleaseFloatArrayElements(jBuf, raw, 0);
    return n;
}

} // extern "C"
