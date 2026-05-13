package com.voiceflux.app.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.voiceflux.app.AudioEngine
import com.voiceflux.app.data.DefaultPresets
import com.voiceflux.app.data.PresetRepository
import com.voiceflux.app.data.VoicePreset
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class MainViewModel(app: Application) : AndroidViewModel(app) {

    val engine     = AudioEngine()
    val repository = PresetRepository(app)

    private val _isRunning        = MutableLiveData(false)
    private val _selectedPreset   = MutableLiveData<VoicePreset>()
    private val _latencyMs        = MutableLiveData(0f)
    private val _waveformData     = MutableLiveData<FloatArray>()

    val isRunning:      LiveData<Boolean>     = _isRunning
    val selectedPreset: LiveData<VoicePreset> = _selectedPreset
    val latencyMs:      LiveData<Float>       = _latencyMs
    val waveformData:   LiveData<FloatArray>  = _waveformData

    // Slider state (driven by UI; sent to engine)
    var pitch:      Float = 0f;      private set
    var formant:    Float = 0f;      private set
    var reverb:     Float = 0f;      private set
    var distortion: Float = 0f;      private set
    var echo:       Float = 0f;      private set
    var gain:       Float = 1f;      private set

    private val waveformBuf = FloatArray(512)
    private var pollingJob: Job? = null

    init {
        engine.create()
        val lastId = repository.loadLastPresetId()
        val preset = DefaultPresets.all.firstOrNull { it.id == lastId }
            ?: DefaultPresets.all[0]
        selectPreset(preset)
    }

    fun toggleRunning() {
        if (_isRunning.value == true) pauseEngine() else startEngine()
    }

    fun startEngine() {
        engine.start()
        _isRunning.value = true
        startPolling()
    }

    fun pauseEngine() {
        engine.stop()
        _isRunning.value = false
        pollingJob?.cancel()
    }

    fun selectPreset(preset: VoicePreset) {
        _selectedPreset.value = preset
        repository.saveLastPresetId(preset.id)
        engine.setPreset(preset.id)
        // Mirror preset values into sliders
        setPitch(preset.pitch)
        setFormant(preset.formant)
        setReverb(preset.reverb)
        setDistortion(preset.distortion)
        setEcho(preset.echo)
        setGain(preset.gain)
    }

    fun setPitch(v: Float)      { pitch      = v; engine.setPitch(v) }
    fun setFormant(v: Float)    { formant    = v; engine.setFormant(v) }
    fun setReverb(v: Float)     { reverb     = v; engine.setReverb(v) }
    fun setDistortion(v: Float) { distortion = v; engine.setDistortion(v) }
    fun setEcho(v: Float)       { echo       = v; engine.setEcho(v) }
    fun setGain(v: Float)       { gain       = v; engine.setGain(v) }

    private fun startPolling() {
        pollingJob?.cancel()
        pollingJob = viewModelScope.launch(Dispatchers.Default) {
            while (isActive) {
                val latency = engine.getLatencyMs()
                val n       = engine.getWaveformData(waveformBuf)
                launch(Dispatchers.Main) {
                    _latencyMs.value    = latency
                    _waveformData.value = waveformBuf.copyOf(n)
                }
                delay(33) // ~30 fps
            }
        }
    }

    override fun onCleared() {
        pollingJob?.cancel()
        engine.stop()
        engine.delete()
        super.onCleared()
    }
}
