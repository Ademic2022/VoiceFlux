package com.voiceflux.app

/**
 * JNI bridge to the native Oboe audio engine.
 * All native* methods map 1-to-1 to jni_bridge.cpp.
 */
class AudioEngine {

    fun create(): Boolean = nativeCreate()
    fun delete()          = nativeDelete()
    fun start()           = nativeStart()
    fun stop()            = nativeStop()
    fun isRunning(): Boolean = nativeIsRunning()

    fun setPreset(presetId: Int)        = nativeSetPreset(presetId)
    fun setPitch(semitones: Float)      = nativeSetPitch(semitones)
    fun setFormant(semitones: Float)    = nativeSetFormant(semitones)
    fun setReverb(amount: Float)        = nativeSetReverb(amount)
    fun setDistortion(amount: Float)    = nativeSetDistortion(amount)
    fun setEcho(amount: Float)          = nativeSetEcho(amount)
    fun setGain(amount: Float)          = nativeSetGain(amount)
    fun setTestTone(enabled: Boolean)   = nativeSetTestTone(enabled)
    fun isTestTone(): Boolean           = nativeIsTestTone()
    fun getLatencyMs(): Float           = nativeGetLatencyMs()
    fun getWaveformData(buf: FloatArray): Int = nativeGetWaveformData(buf)

    // ---- native declarations ----
    private external fun nativeCreate(): Boolean
    private external fun nativeDelete()
    private external fun nativeStart()
    private external fun nativeStop()
    private external fun nativeIsRunning(): Boolean
    private external fun nativeSetPreset(presetId: Int)
    private external fun nativeSetPitch(semitones: Float)
    private external fun nativeSetFormant(semitones: Float)
    private external fun nativeSetReverb(amount: Float)
    private external fun nativeSetDistortion(amount: Float)
    private external fun nativeSetEcho(amount: Float)
    private external fun nativeSetGain(amount: Float)
    private external fun nativeSetTestTone(enabled: Boolean)
    private external fun nativeIsTestTone(): Boolean
    private external fun nativeGetLatencyMs(): Float
    private external fun nativeGetWaveformData(buf: FloatArray): Int

    companion object {
        init { System.loadLibrary("voiceflux") }
    }
}
