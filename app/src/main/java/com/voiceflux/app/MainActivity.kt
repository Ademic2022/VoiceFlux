package com.voiceflux.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.voiceflux.app.data.DefaultPresets
import com.voiceflux.app.databinding.ActivityMainBinding
import com.voiceflux.app.ui.MainViewModel
import com.voiceflux.app.ui.PresetAdapter

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val vm: MainViewModel by viewModels()
    private lateinit var presetAdapter: PresetAdapter
    private lateinit var bottomSheet: BottomSheetBehavior<LinearLayout>

    private val requestPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) vm.startEngine()
            else Toast.makeText(this, R.string.permission_denied, Toast.LENGTH_LONG).show()
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupToolbar()
        setupPresetList()
        setupBottomSheet()
        setupSliders()
        setupFab()
        observeViewModel()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
    }

    private fun setupPresetList() {
        presetAdapter = PresetAdapter { preset ->
            vm.selectPreset(preset)
            presetAdapter.setSelected(preset.id)
            updateSlidersFromVm()
        }
        binding.rvPresets.apply {
            adapter      = presetAdapter
            layoutManager = GridLayoutManager(this@MainActivity, 2)
            setHasFixedSize(true)
        }
        presetAdapter.submitList(DefaultPresets.all)
    }

    private fun setupBottomSheet() {
        bottomSheet = BottomSheetBehavior.from(binding.bottomSheet)
        bottomSheet.state = BottomSheetBehavior.STATE_COLLAPSED
        bottomSheet.peekHeight = 80.dpToPx()

        binding.btnExpandSheet.setOnClickListener {
            bottomSheet.state =
                if (bottomSheet.state == BottomSheetBehavior.STATE_COLLAPSED)
                    BottomSheetBehavior.STATE_EXPANDED
                else
                    BottomSheetBehavior.STATE_COLLAPSED
        }
    }

    private fun setupSliders() {
        fun SeekBar.onChanged(block: (Float) -> Unit) {
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, p: Int, fromUser: Boolean) {
                    if (fromUser) block(p.toFloat() / max.toFloat())
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?)  {}
            })
        }

        val s = binding.slidersLayout

        // Pitch: -12..+12 semitones, seekbar 0..240
        s.sliderPitch.max = 240
        s.sliderPitch.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, p: Int, fromUser: Boolean) {
                if (fromUser) vm.setPitch((p - 120).toFloat() / 10f)
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?)  {}
        })

        // Formant: same range
        s.sliderFormant.max = 240
        s.sliderFormant.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, p: Int, fromUser: Boolean) {
                if (fromUser) vm.setFormant((p - 120).toFloat() / 10f)
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?)  {}
        })

        s.sliderReverb    .onChanged { vm.setReverb(it) }
        s.sliderDistortion.onChanged { vm.setDistortion(it) }
        s.sliderEcho      .onChanged { vm.setEcho(it) }

        // Gain: 0..2
        s.sliderGain.max = 200
        s.sliderGain.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, p: Int, fromUser: Boolean) {
                if (fromUser) vm.setGain(p.toFloat() / 100f)
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?)  {}
        })
    }

    private fun setupFab() {
        binding.fab.setOnClickListener {
            if (hasAudioPermission()) vm.toggleRunning()
            else requestPermission.launch(Manifest.permission.RECORD_AUDIO)
        }

        binding.chipPreview.setOnCheckedChangeListener { _, checked ->
            if (checked && !hasAudioPermission()) {
                requestPermission.launch(Manifest.permission.RECORD_AUDIO)
                binding.chipPreview.isChecked = false
                return@setOnCheckedChangeListener
            }
            vm.toggleTestTone()
        }
    }

    private fun observeViewModel() {
        vm.isRunning.observe(this) { running ->
            binding.fab.text = getString(if (running) R.string.stop else R.string.start)
            binding.fab.setIconResource(
                if (running) android.R.drawable.ic_media_pause
                else         android.R.drawable.ic_btn_speak_now
            )
            binding.waveformView.isActive = running

            if (running) startService(Intent(this, AudioService::class.java))
            else         stopService(Intent(this, AudioService::class.java))
        }

        vm.isTestTone.observe(this) { active ->
            binding.chipPreview.isChecked = active
            // Highlight chip with accent colour when active
            binding.chipPreview.chipBackgroundColor =
                android.content.res.ColorStateList.valueOf(
                    if (active) getColor(R.color.accent_purple)
                    else        getColor(R.color.bg_card_elevated)
                )
        }

        vm.selectedPreset.observe(this) { preset ->
            presetAdapter.setSelected(preset.id)
        }

        vm.latencyMs.observe(this) { ms ->
            binding.tvLatency.text = getString(R.string.latency_fmt, ms)
        }

        vm.waveformData.observe(this) { data ->
            binding.waveformView.update(data)
        }
    }

    private fun updateSlidersFromVm() {
        val s = binding.slidersLayout
        s.sliderPitch    .progress = ((vm.pitch    * 10f) + 120).toInt().coerceIn(0, 240)
        s.sliderFormant  .progress = ((vm.formant  * 10f) + 120).toInt().coerceIn(0, 240)
        s.sliderReverb   .progress = (vm.reverb     * 100).toInt().coerceIn(0, 100)
        s.sliderDistortion.progress = (vm.distortion * 100).toInt().coerceIn(0, 100)
        s.sliderEcho     .progress = (vm.echo       * 100).toInt().coerceIn(0, 100)
        s.sliderGain     .progress = (vm.gain       * 100).toInt().coerceIn(0, 200)
    }

    private fun hasAudioPermission() =
        ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
            PackageManager.PERMISSION_GRANTED

    private fun Int.dpToPx(): Int =
        (this * resources.displayMetrics.density).toInt()

    override fun onStop() {
        super.onStop()
        // Engine keeps running in AudioService; ViewModel polling pauses here
        // and resumes in onStart via lifecycle observer.
    }
}
