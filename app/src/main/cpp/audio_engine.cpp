#include "audio_engine.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "VoiceFlux"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace voiceflux {

// FIFO capacity: 2× the expected max output burst (e.g. 2048 frames × 2 = 4096)
static constexpr int kFifoCapacity = 8192;

AudioEngine::AudioEngine()
    : inputFifo_(kFifoCapacity) {
    waveform_.assign(kWaveformSize, 0.f);
    inputScratch_ .assign(2048, 0.f);
    processScratch_.assign(2048, 0.f);
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::initialize() {
    return openStreams();
}

bool AudioEngine::openStreams() {
    oboe::AudioStreamBuilder inBuilder, outBuilder;

    // ---- Output stream ----
    // Oboe setters return AudioStreamBuilder* so after the first dot call we chain with ->
    oboe::Result result = outBuilder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Mono)
        ->setSampleRate(sampleRate_)
        ->setDataCallback(this)
        ->setErrorCallback(this)
        ->openManagedStream(outputStream_);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        return false;
    }

    // Use actual sample rate the stream was granted
    sampleRate_ = outputStream_->getSampleRate();
    LOGI("Output stream: sr=%d bufSize=%d", sampleRate_,
         outputStream_->getBufferSizeInFrames());

    // ---- Input stream — match output sample rate ----
    // Try EXCLUSIVE + VoiceCommunication first (best for real devices).
    // Fall back to SHARED + default preset (needed for emulators and some devices).
    result = inBuilder.setDirection(oboe::Direction::Input)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Mono)
        ->setSampleRate(sampleRate_)
        ->setInputPreset(oboe::InputPreset::VoiceCommunication)
        ->openManagedStream(inputStream_);

    if (result != oboe::Result::OK) {
        LOGI("Exclusive input failed (%s), retrying with Shared mode",
             oboe::convertToText(result));
        oboe::AudioStreamBuilder fallbackIn;
        result = fallbackIn.setDirection(oboe::Direction::Input)
            ->setPerformanceMode(oboe::PerformanceMode::None)
            ->setSharingMode(oboe::SharingMode::Shared)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(oboe::ChannelCount::Mono)
            ->setSampleRate(sampleRate_)
            ->openManagedStream(inputStream_);
    }

    if (result != oboe::Result::OK) {
        LOGE("Failed to open input stream: %s", oboe::convertToText(result));
        outputStream_->close();
        return false;
    }

    // Set output buffer size to 2× input burst for smooth full-duplex
    const int inputBurst = inputStream_->getFramesPerBurst();
    outputStream_->setBufferSizeInFrames(inputBurst * 2);

    LOGI("Input stream:  sr=%d bufSize=%d", sampleRate_,
         inputStream_->getBufferSizeInFrames());
    return true;
}

void AudioEngine::closeStreams() {
    if (outputStream_) outputStream_->close();
    if (inputStream_)  inputStream_->close();
}

void AudioEngine::start() {
    if (running_.load()) return;
    running_.store(true);
    processor_.reset();
    inputFifo_.reset();

    if (inputStream_)  inputStream_->requestStart();
    if (outputStream_) outputStream_->requestStart();
}

void AudioEngine::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (outputStream_) outputStream_->requestStop();
    if (inputStream_)  inputStream_->requestStop();
}

oboe::DataCallbackResult AudioEngine::onAudioReady(
    oboe::AudioStream* /*stream*/,
    void* audioData,
    int32_t numFrames) {

    float* const out = static_cast<float*>(audioData);

    if (!running_.load()) {
        std::fill(out, out + numFrames, 0.f);
        return oboe::DataCallbackResult::Continue;
    }

    // ---- Drain input stream into FIFO ----
    if (inputStream_ && inputStream_->getState() == oboe::StreamState::Started) {
        if (static_cast<int>(inputScratch_.size()) < numFrames)
            inputScratch_.resize(numFrames * 2);

        int32_t framesRead = 0;
        const oboe::ResultWithValue<int32_t> res =
            inputStream_->read(inputScratch_.data(), numFrames, 0 /*nowait*/);
        if (res.value() > 0) {
            framesRead = res.value();
            inputFifo_.write(inputScratch_.data(), framesRead);
        }
    }

    // ---- Fill processing scratch (test tone OR microphone) ----
    if (static_cast<int>(processScratch_.size()) < numFrames)
        processScratch_.resize(numFrames * 2);

    if (testToneEnabled_.load()) {
        // Bandlimited sawtooth at 150 Hz — sounds voice-like through the DSP chain
        static constexpr float kFundamental = 150.f;
        const float phaseInc = kFundamental / static_cast<float>(sampleRate_);
        for (int i = 0; i < numFrames; ++i) {
            float s = 0.f;
            for (int h = 1; h <= 6; ++h) {
                s += std::sin(2.f * static_cast<float>(M_PI) * testTonePhase_ * h)
                     / static_cast<float>(h);
            }
            processScratch_[i] = s * 0.25f;
            testTonePhase_ += phaseInc;
            if (testTonePhase_ >= 1.f) testTonePhase_ -= 1.f;
        }
        processor_.process(processScratch_.data(), out, numFrames);
    } else {
        const int available = inputFifo_.available();
        if (available >= numFrames) {
            inputFifo_.read(processScratch_.data(), numFrames);
            processor_.process(processScratch_.data(), out, numFrames);
        } else {
            // Not enough mic input yet — output silence to avoid underrun
            std::fill(out, out + numFrames, 0.f);
        }
    }

    // ---- Update waveform snapshot for visualiser ----
    for (int i = 0; i < numFrames; ++i) {
        const int pos = waveformPos_.load(std::memory_order_relaxed);
        waveform_[pos] = out[i];
        waveformPos_.store((pos + 1) % kWaveformSize, std::memory_order_relaxed);
    }

    // ---- Estimate latency ----
    auto latRes = outputStream_->calculateLatencyMillis();
    if (latRes) latencyMs_.store(static_cast<float>(latRes.value()));

    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream* /*stream*/, oboe::Result error) {
    LOGE("Audio stream error: %s — restarting", oboe::convertToText(error));
    stop();
    if (openStreams()) start();
}

int AudioEngine::getWaveformData(float* buf, int maxSamples) {
    const int n = std::min(maxSamples, kWaveformSize);
    const int head = waveformPos_.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        buf[i] = waveform_[(head + i) % kWaveformSize];
    return n;
}

} // namespace voiceflux
