#include "voice_processor.h"
#include <algorithm>
#include <cmath>

namespace voiceflux {

// Echo delay: ~250 ms
static constexpr float kEchoDelaySeconds = 0.25f;

VoiceProcessor::VoiceProcessor(int sampleRate)
    : sampleRate_(sampleRate),
      reverb_(sampleRate),
      eq_(sampleRate),
      robot_(sampleRate) {

    echoDelaySamples_ = static_cast<int>(kEchoDelaySeconds * sampleRate);
    echoBuffer_.assign(echoDelaySamples_ + 1, 0.f);

    // Worst-case frame size from pitch/formant shifters
    stageBuf_.assign(4096, 0.f);
}

void VoiceProcessor::applyPreset(PresetId id) {
    switch (id) {
        case PresetId::PASSTHROUGH:
            params_.pitchSemitones   = 0.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.f;
            params_.distortionAmount = 0.f;
            params_.echoAmount       = 0.f;
            params_.gain             = 1.f;
            params_.effectMode       = 0;
            params_.eqLow            = 0.f;
            params_.eqMid            = 0.f;
            params_.eqHigh           = 0.f;
            break;
        case PresetId::CHILD:
            // Phase vocoder shifts formants proportionally with pitch — no extra formant stage needed.
            params_.pitchSemitones   = 7.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.08f;
            params_.distortionAmount = 0.f;
            params_.echoAmount       = 0.f;
            params_.gain             = 1.1f;
            params_.effectMode       = 0;
            params_.eqLow            = -3.f;
            params_.eqMid            = 2.f;
            params_.eqHigh           = 4.f;
            break;
        case PresetId::DEEP_MALE:
            params_.pitchSemitones   = -5.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.18f;
            params_.distortionAmount = 0.f;
            params_.echoAmount       = 0.f;
            params_.gain             = 1.2f;
            params_.effectMode       = 0;
            params_.eqLow            = 4.f;
            params_.eqMid            = -1.f;
            params_.eqHigh           = -3.f;
            break;
        case PresetId::FEMALE:
            params_.pitchSemitones   = 5.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.12f;
            params_.distortionAmount = 0.f;
            params_.echoAmount       = 0.f;
            params_.gain             = 1.0f;
            params_.effectMode       = 0;
            params_.eqLow            = -2.f;
            params_.eqMid            = 1.f;
            params_.eqHigh           = 3.f;
            break;
        case PresetId::ROBOT:
            params_.pitchSemitones   = 0.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.25f;
            params_.distortionAmount = 0.2f;
            params_.echoAmount       = 0.15f;
            params_.gain             = 1.0f;
            params_.effectMode       = 1;
            params_.eqLow            = 0.f;
            params_.eqMid            = 0.f;
            params_.eqHigh           = 0.f;
            break;
        case PresetId::MONSTER:
            params_.pitchSemitones   = -7.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.35f;
            params_.distortionAmount = 0.3f;
            params_.echoAmount       = 0.2f;
            params_.gain             = 1.3f;
            params_.effectMode       = 0;
            params_.eqLow            = 6.f;
            params_.eqMid            = -2.f;
            params_.eqHigh           = -4.f;
            break;
        case PresetId::GRANDMA:
            params_.pitchSemitones   = 3.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.25f;
            params_.distortionAmount = 0.06f;
            params_.echoAmount       = 0.15f;
            params_.gain             = 0.9f;
            params_.effectMode       = 0;
            params_.eqLow            = -2.f;
            params_.eqMid            = 3.f;
            params_.eqHigh           = -2.f;
            break;
        case PresetId::RADIO:
            params_.pitchSemitones   = 0.f;
            params_.formantSemitones = 0.f;
            params_.reverbMix        = 0.f;
            params_.distortionAmount = 0.3f;
            params_.echoAmount       = 0.f;
            params_.gain             = 1.1f;
            params_.effectMode       = 2;
            params_.eqLow            = 0.f;
            params_.eqMid            = 0.f;
            params_.eqHigh           = 0.f;
            break;
    }
}

void VoiceProcessor::updateEffectParams() {
    const float pitch   = params_.pitchSemitones.load();
    const float formant = params_.formantSemitones.load();

    if (pitch != lastPitch_) {
        pitchShifter_.setPitchFactor(PitchShifter::semitoneToFactor(pitch));
        lastPitch_ = pitch;
    }
    if (formant != lastFormant_) {
        formantShifter_.setFormantFactor(FormantShifter::semitoneToFactor(formant));
        lastFormant_ = formant;
    }

    reverb_.setMix(params_.reverbMix.load());
    distortion_.setAmount(params_.distortionAmount.load());

    const int mode = params_.effectMode.load();
    if (mode == 1) {
        robot_.setCarrierFreq(50.f);
        robot_.setDepth(0.8f);
        eq_.setLowGain(0.f);
        eq_.setMidGain(0.f);
        eq_.setHighGain(0.f);
    } else if (mode == 2) {
        // Radio: telephone bandpass (hard low/high cut + slight mid boost)
        eq_.setLowGain(-14.f);
        eq_.setMidGain(3.f);
        eq_.setHighGain(-12.f);
    } else {
        // Use per-preset EQ for natural voice coloring
        eq_.setLowGain(params_.eqLow.load());
        eq_.setMidGain(params_.eqMid.load());
        eq_.setHighGain(params_.eqHigh.load());
    }
}

void VoiceProcessor::process(const float* input, float* output, int numSamples) {
    updateEffectParams();

    if (static_cast<int>(stageBuf_.size()) < numSamples)
        stageBuf_.resize(numSamples * 2);

    const float* in = input;

    // 1. Pitch shift
    const float pitchSt = params_.pitchSemitones.load();
    if (std::abs(pitchSt) > 0.01f) {
        pitchShifter_.process(in, stageBuf_.data(), numSamples);
        in = stageBuf_.data();
    }

    // 2. Formant shift (copy to output as working buffer)
    const float formantSt = params_.formantSemitones.load();
    if (std::abs(formantSt) > 0.01f) {
        formantShifter_.process(in, output, numSamples);
        in = output;
    } else if (in != output) {
        std::copy(in, in + numSamples, output);
        in = output;
    }

    // 3. Robot / ring modulation
    if (params_.effectMode.load() == 1) {
        robot_.process(in, output, numSamples);
        in = output;
    }

    // 4. Distortion
    if (params_.distortionAmount.load() > 0.01f) {
        distortion_.process(in, output, numSamples);
        in = output;
    }

    // 5. EQ (radio bandpass or normal)
    eq_.process(in, output, numSamples);
    in = output;

    // 6. Reverb
    if (params_.reverbMix.load() > 0.01f) {
        reverb_.process(in, output, numSamples);
        in = output;
    }

    // 7. Echo / delay
    const float echoAmt = params_.echoAmount.load();
    if (echoAmt > 0.01f) {
        for (int i = 0; i < numSamples; ++i) {
            const float delayed  = echoBuffer_[echoHead_];
            output[i]            = in[i] + delayed * echoAmt;
            echoBuffer_[echoHead_] = in[i];
            echoHead_            = (echoHead_ + 1) % echoDelaySamples_;
        }
        in = output;
    } else if (in != output) {
        std::copy(in, in + numSamples, output);
    }

    // 8. Gain + soft-limit to prevent clipping
    const float gain = params_.gain.load();
    for (int i = 0; i < numSamples; ++i) {
        const float s = output[i] * gain;
        // Soft limiter at ±1
        output[i] = std::tanh(s);
    }
}

void VoiceProcessor::reset() {
    pitchShifter_.reset();
    formantShifter_.reset();
    reverb_.reset();
    robot_.reset();
    eq_.reset();
    std::fill(echoBuffer_.begin(), echoBuffer_.end(), 0.f);
    echoHead_ = 0;
}

} // namespace voiceflux
