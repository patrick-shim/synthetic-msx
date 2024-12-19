#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>
#include <cstddef>

class Memory {
public:
    static const size_t MEMORY_SIZE = 65536; // Full 16-bit address space
    static const size_t BANK_SIZE = 16384;    // 16KB per bank
    static const size_t NUM_BANKS = 4;        // 4 banks for 64KB

    Memory(); // Constructor

    // Read and write methods (renamed for clarity)
    uint8_t readByte(uint16_t address) const;
    void writeByte(uint16_t address, uint8_t value);
    uint16_t readWord(uint16_t address) const;
    void writeWord(uint16_t address, uint16_t value);

    // Load data into memory
    void loadData(const std::vector<uint8_t>& data, uint16_t startAddress = 0);

    // Dump memory for debugging (improved formatting)
    void dumpMemory(uint16_t start = 0, size_t length = 256) const;

    // Memory-mapped I/O
    void setIOHandler(uint16_t start, uint16_t end,
                     std::function<uint8_t(uint16_t)> readHandler,
                     std::function<void(uint16_t, uint8_t)> writeHandler);
                     
    // Banked memory methods
    void loadBank(const std::vector<uint8_t>& data, uint8_t bank);
    void selectBank(uint8_t bank);

    // Save and restore (fixed)
    void saveState(std::ostream& os) const;
    void loadState(std::istream& is);

private:
    std::array<uint8_t, MEMORY_SIZE> memory_;
    std::array<std::array<uint8_t, BANK_SIZE>, NUM_BANKS> banks_;
    uint8_t activeBank_{};

    struct IOHandler {
        uint16_t start, end;
        std::function<uint8_t(uint16_t)> read;
        std::function<void(uint16_t, uint8_t)> write;
    };

    std::vector<IOHandler> ioHandlers_;

    void validateAddress(uint16_t address) const;
    bool isIOAddress(uint16_t address, IOHandler& handler) const;
};

#endif // MEMORY_HPP