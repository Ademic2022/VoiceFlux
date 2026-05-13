#pragma once
#include <oboe/Oboe.h>
#include "voice_processor.h"
#include <atomic>
#include <vector>
#include <mutex>

namespace voiceflux {

// Lock-free single-producer / single-consumer ring buffer.
// Input callback (producer) writes; output callback (consumer) reads.
class AudioFifo {
public:
    explicit AudioFifo(int capacity)
        : buf_(capacity, 0.f), capacity_(capacity) {}

    int write(const float* data, int n) {
        int written = 0;
        while (written < n) {
            const int w = writePos_.load(std::memory_order_relaxed);
            const int r = readPos_.load(std::memory_order_acquire);
            const int avail = capacity_ - ((w - r + capacity_) % capacity_) - 1;
            if (avail <= 0) break;
            buf_[w % capacity_] = data[written++];
            writePos_.store((w + 1) % capacity_, std::memory_order_release);
        }
        return written;
    }

    int read(float* data, int n) {
        int read = 0;
        while (read < n) {
            const int r = readPos_.load(std::memory_order_relaxed);
            const int w = writePos_.load(std::memory_order_acquire);
            if (r == w) break;
            data[read++] = buf_[r];
            readPos_.store((r + 1) % capacity_, std::memory_order_release);
        }
        return read;
    }

    int available() const {
        const int w = writePos_.load(std::memory_order_acquire);
        const int r = readPos_.load(std::memory_order_acquire);
        return (w - r + capacity_) % capacity_;
    }

    void reset() { readPos_.store(0); writePos_.store(0); }

private:
    std::vector<float>   buf_;
    int                  capacity_;
    std::atomic<int>     readPos_{0};
    std::atomic<int>     writePos_{0};
};

// Full-duplex Oboe engine.
// Input stream feeds AudioFifo; output callback consumes FIFO and runs DSP.
class AudioEngine
    : public oboe::AudioStreamDataCallback,
      public oboe::AudioStreamErrorCallback {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    VoiceProcessor& processor() { return processor_; }

    float getLatencyMs() const { return latencyMs_.load(); }

    // Copy the latest waveform snapshot (up to maxSamples).
    int  getWaveformData(float* buf, int maxSamples);

private:
    // Output stream callback — this IS the hot path.
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* stream,
        void* audioData,
        int32_t numFrames) override;

    void onErrorAfterClose(oboe::AudioStream* stream,
                           oboe::Result error) override;

    bool openStreams();
    void closeStreams();

    oboe::ManagedStream outputStream_;
    oboe::ManagedStream inputStream_;

    VoiceProcessor processor_;
    AudioFifo      inputFifo_;

    std::atomic<bool>  running_{false};
    std::atomic<float> latencyMs_{0.f};

    // Waveform ring buffer for the visualiser
    static constexpr int kWaveformSize = 1024;
    std::vector<float>   waveform_;
    std::atomic<int>     waveformPos_{0};

    int sampleRate_{48000};

    // Per-callback scratch buffer (avoids allocation in hot path)
    std::vector<float> inputScratch_;
    std::vector<float> processScratch_;
};

} // namespace voiceflux
