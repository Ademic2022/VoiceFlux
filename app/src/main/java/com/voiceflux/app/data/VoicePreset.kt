package com.voiceflux.app.data

data class VoicePreset(
    val id: Int,
    val name: String,
    val emoji: String,
    val pitch: Float,        // semitones -12..+12
    val formant: Float,      // semitones -12..+12
    val reverb: Float,       // 0..1
    val distortion: Float,   // 0..1
    val echo: Float,         // 0..1
    val gain: Float,         // 0..2
    val isCustom: Boolean = false
)

object DefaultPresets {
    val all: List<VoicePreset> = listOf(
        VoicePreset(0, "Original",  "🎤",  0f,   0f,  0.00f, 0.00f, 0.00f, 1.0f),
        VoicePreset(1, "Child",     "👦",  8f,   5f,  0.10f, 0.00f, 0.05f, 1.1f),
        VoicePreset(2, "Deep Male", "🦁", -5f,  -4f,  0.20f, 0.00f, 0.00f, 1.2f),
        VoicePreset(3, "Female",    "👩",  5f,   4f,  0.15f, 0.00f, 0.00f, 1.0f),
        VoicePreset(4, "Robot",     "🤖",  0f,   0f,  0.25f, 0.20f, 0.15f, 1.0f),
        VoicePreset(5, "Monster",   "👹", -8f,  -6f,  0.40f, 0.45f, 0.30f, 1.4f),
        VoicePreset(6, "Grandma",   "👵",  3f,   2f,  0.30f, 0.08f, 0.20f, 0.9f),
        VoicePreset(7, "Radio",     "📻",  0f,   0f,  0.05f, 0.35f, 0.08f, 1.1f),
    )
}
