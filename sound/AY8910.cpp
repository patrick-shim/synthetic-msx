#include "AY8910.h"
#include "Sound.h"

// Static look-up tables for envelopes and volumes
static constexpr std::array<std::array<uint8_t, 32>, 16> ENVELOPE_SHAPES = {
    // ... (Same envelope data as before) ...
};

static constexpr std::array<int, 16> VOLUME_LEVELS = {
    0, 1, 2, 4, 6, 8, 11, 16, 23, 32, 45, 64, 90, 128, 180, 255
};

void AY8910Emulator::reset(int clockHz, int firstChannel) {
    static constexpr std::array<uint8_t, 16> INITIAL_REGISTERS = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFD,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00
    };

    registers_ = INITIAL_REGISTERS;
    envelopePhase_ = 0;
    clockFrequency_ = clockHz >> 4;
    firstChannel_ = firstChannel;
    syncMode_ = ASYNC_MODE;
    changedChannels_ = 0x00;
    envelopePeriod_ = -1;
    envelopeCounter_ = 0;
    registerLatch_ = 0x00;

    // Set sound types for each channel
    for (int i = 0; i < NUM_CHANNELS / 2; ++i) {
        SetSound(i + firstChannel, SND_MELODIC);
        SetSound(i + firstChannel + NUM_CHANNELS / 2, SND_NOISE);
    }

    // Configure noise generator
    SetNoise(0x10000, 16, 14);

    // Silence all channels
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        frequencies_[i] = volumes_[i] = 0;
        Sound(i + firstChannel, 0, 0);
    }
}

void AY8910Emulator::writeControlPort(uint8_t value) {
    registerLatch_ = value & 0x0F;
}

void AY8910Emulator::writeDataPort(uint8_t value) {
    writeRegister(registerLatch_, value);
}

uint8_t AY8910Emulator::readDataPort() {
    return registers_[registerLatch_];
}

void AY8910Emulator::writeRegister(uint8_t reg, uint8_t value) {
    switch (reg) {
        case 1:
        case 3:
        case 5:
            value &= 0x0F;
            // Fall through
        case 0:
        case 2:
        case 4:
            if (value != registers_[reg]) {
                changedChannels_ |= (1 << (reg >> 1)) & ~registers_[7];
                registers_[reg] = value;
            }
            break;

        case 6:
            value &= 0x1F;
            if (value != registers_[reg]) {
                changedChannels_ |= 0x38 & ~registers_[7];
                registers_[reg] = value;
            }
            break;

        case 7:
            changedChannels_ |= (value ^ registers_[reg]) & 0x3F;
            registers_[reg] = value;
            break;

        case 8:
        case 9:
        case 10:
            value &= 0x1F;
            if (value != registers_[reg]) {
                changedChannels_ |= (0x09 << (reg - 8)) & ~registers_[7];
                registers_[reg] = value;
            }
            break;

        case 11:
        case 12:
            if (value != registers_[reg]) {
                envelopePeriod_ = -1; // Recompute envelope period on next loop
                registers_[reg] = value;
            }
            return;

        case 13:
            registers_[reg] = value & 0x0F;
            envelopeCounter_ = 0;
            envelopePhase_ = 0;
            // Compute changed channels mask
            for (int i = 0; i < NUM_CHANNELS / 2; ++i) {
                if (registers_[i + 8] & 0x10) {
                    changedChannels_ |= ((0x09 << i) & ~registers_[7]);
                }
            }
            break;

        case 14:
        case 15:
            registers_[reg] = value;
            return;

        default:
            return; // Invalid register
    }

    // For asynchronous mode, make Sound() calls immediately
    if (!syncMode_ && changedChannels_) {
        sync(FLUSH_MODE);
    }
}

void AY8910Emulator::loop(int uSec) {
    if (envelopePeriod_ < 0) {
        int period = ((int)registers_[12] << 8) + registers_[11];
        envelopePeriod_ = (int)(1000000L * (period ? period : 0x10000) / clockFrequency_);
    }

    if (!envelopePeriod_) {
        return; // No envelope running
    }

    envelopeCounter_ += uSec;
    if (envelopeCounter_ < envelopePeriod_) {
        return;
    }

    int numSteps = envelopeCounter_ / envelopePeriod_;
    envelopeCounter_ -= numSteps * envelopePeriod_;

    envelopePhase_ += numSteps;
    if (envelopePhase_ > 31) {
        envelopePhase_ = (registers_[13] & 0x09) == 0x08 ? (envelopePhase_ & 0x1F) : 31;
    }

    // Compute changed channels mask
    for (int i = 0; i < NUM_CHANNELS / 2; ++i) {
        if (registers_[i + 8] & 0x10) {
            changedChannels_ |= ((0x09 << i) & ~registers_[7]);
        }
    }

    // For asynchronous mode, make Sound() calls immediately
    if (!syncMode_ && changedChannels_) {
        sync(FLUSH_MODE);
    }
}

void AY8910Emulator::sync(uint8_t syncMode) {
    int newSyncMode = syncMode & ~DRUMS_MODE;
    if (newSyncMode != FLUSH_MODE) {
        syncMode_ = newSyncMode;
    }

    int channelsToUpdate = changedChannels_ | (syncMode & DRUMS_MODE ? 0x38 : 0x00);

    int drumsVolume = 0;
    for (int channel = 0; channelsToUpdate && (channel < NUM_CHANNELS); ++channel, channelsToUpdate >>= 1) {
        if (channelsToUpdate & 1) {
            int freq, volume;
            if (registers_[7] & (1 << channel)) {
                freq = volume = 0;
            } else {
                if (channel < NUM_CHANNELS / 2) {
                    // Melodic channel
                    int coarseVolume = registers_[channel + 8] & 0x1F;
                    int volumeIndex = coarseVolume & 0x10 ? ENVELOPE_SHAPES[registers_[13] & 0x0F][envelopePhase_] : (coarseVolume & 0x0F);
                    volume = VOLUME_LEVELS[volumeIndex];

                    int period = ((int)(registers_[(channel << 1) + 1] & 0x0F) << 8) + registers_[channel << 1];
                    freq = period ? clockFrequency_ / period : 0;
                } else {
                    // Noise channel
                    int coarseVolume = registers_[channel + 5] & 0x1F;
                    int volumeIndex = coarseVolume & 0x10 ? ENVELOPE_SHAPES[registers_[13] & 0x0F][envelopePhase_] : (coarseVolume & 0x0F);
                    volume = (VOLUME_LEVELS[volumeIndex] + 1) >> 1;
                    drumsVolume += volume;

                    int noisePeriod = registers_[6] & 0x1F;
                    freq = clockFrequency_ / ((noisePeriod ? noisePeriod : 0x20) << 2);
                }
            }

            Sound(firstChannel_ + channel, freq, volume);
        }
    }

    if (drumsVolume && (syncMode & DRUMS_MODE)) {
        Drum(DRM_MIDI | 28, drumsVolume > 255 ? 255 : drumsVolume);
    }

    changedChannels_ = 0x00;
}