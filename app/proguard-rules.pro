# JNI method names must not be obfuscated
-keep class com.voiceflux.app.AudioEngine { *; }

# Gson data models
-keep class com.voiceflux.app.data.VoicePreset { *; }
-keepattributes Signature
-keepattributes *Annotation*
