#ifndef AY8910_H
#define AY8910_H

#include <array>
#include <cstdint>

class AY8910Emulator {
public:
    static constexpr int NUM_CHANNELS = 6;       // 3 melodic + 3 noise channels
    static constexpr uint8_t ASYNC_MODE = 0;     // Asynchronous emulation
    static constexpr uint8_t SYNC_MODE = 1;      // Synchronous emulation
    static constexpr uint8_t FLUSH_MODE = 2;     // Flush buffers only
    static constexpr uint8_t DRUMS_MODE = 0x80;  // Hit drums for noise channels

    AY8910Emulator() = default;

    // Placement new operator (explained in previous response)
    void* operator new(size_t size, void *ptr) {
        return ptr;
    }

    /** reset() *************************************************/
    /** Reset the sound chip and use sound channels from the    **/
    /** one given in firstChannel.                             **/
    /*************************************************************/
    void reset(int clockHz, int firstChannel);

    /** writeRegister() *****************************************/
    /** Write a value to a specific PSG register.              **/
    /*************************************************************/
    void writeRegister(uint8_t reg, uint8_t value);

    /** writeControlPort() **************************************/
    /** Write a value to the PSG Control Port.                **/
    /*************************************************************/
    void writeControlPort(uint8_t value);

    /** writeDataPort() *****************************************/
    /** Write a value to the PSG Data Port.                   **/
    /*************************************************************/
    void writeDataPort(uint8_t value);

    /** readDataPort() ******************************************/
    /** Read a value from the PSG Data Port.                    **/
    /*************************************************************/
    uint8_t readDataPort();

    /** sync() **************************************************/
    /** Flush all accumulated changes by issuing Sound() calls  **/
    /** and set the synchronization on/off. The syncMode       **/
    /** argument should be SYNC_MODE/ASYNC_MODE to set/reset   **/
    /** sync, or FLUSH_MODE to leave sync mode as it is. To    **/
    /** emulate noise channels with MIDI drums, OR syncMode     **/
    /** with DRUMS_MODE.                                       **/
    /*************************************************************/
    void sync(uint8_t syncMode);

    /** loop() **************************************************/
    /** Call this function periodically to update volume        **/
    /** envelopes. Use uSec to pass the time since the last     **/
    /** call of loop() in microseconds.                        **/
    /*************************************************************/
    void loop(int uSec);

private:
    std::array<uint8_t, 16> registers_{};          // PSG registers contents

    // THESE VALUES ARE NOT USED BUT KEPT FOR BACKWARD COMPATIBILITY
    std::array<int, NUM_CHANNELS> frequencies_{};  // Frequencies (0 for off)
    std::array<int, NUM_CHANNELS> volumes_{};      // Volumes (0..255)

    int clockFrequency_{};                          // Base clock rate (Fin/16)
    int firstChannel_{};                            // First used Sound() channel
    uint8_t changedChannels_{};                     // Bitmap of changed channels
    uint8_t syncMode_{};                            // ASYNC_MODE/SYNC_MODE
    uint8_t registerLatch_{};                       // Latch for the register num
    int envelopePeriod_{};                          // Envelope step in microsecs
    int envelopeCounter_{};                         // Envelope step counter
    int envelopePhase_{};                           // Envelope phase
};

#endif /* AY8910_H */