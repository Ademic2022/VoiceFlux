package com.voiceflux.app.data

import android.content.Context
import android.content.SharedPreferences
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken

class PresetRepository(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("voiceflux_presets", Context.MODE_PRIVATE)
    private val gson = Gson()

    fun loadCustomPresets(): List<VoicePreset> {
        val json = prefs.getString(KEY_CUSTOM, null) ?: return emptyList()
        return try {
            val type = object : TypeToken<List<VoicePreset>>() {}.type
            gson.fromJson(json, type) ?: emptyList()
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun saveCustomPresets(presets: List<VoicePreset>) {
        prefs.edit().putString(KEY_CUSTOM, gson.toJson(presets)).apply()
    }

    fun saveLastPresetId(id: Int) {
        prefs.edit().putInt(KEY_LAST_PRESET, id).apply()
    }

    fun loadLastPresetId(): Int = prefs.getInt(KEY_LAST_PRESET, 0)

    companion object {
        private const val KEY_CUSTOM      = "custom_presets"
        private const val KEY_LAST_PRESET = "last_preset_id"
    }
}
