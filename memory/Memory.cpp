#include "Memory.hpp"
#include <algorithm>
#include <iomanip>

Memory::Memory() : activeBank_(0) {
    memory_.fill(0);
    for (auto& bank : banks_) {
        bank.fill(0);
    }
}

void Memory::writeByte(uint16_t address, uint8_t value) {
    IOHandler handler;

    if (isIOAddress(address, handler)) {
        handler.write(address, value);
        return;
    }

    // For addresses within bank size, write to active bank
    if (address < BANK_SIZE) {
        if (activeBank_ >= NUM_BANKS) {
            throw std::out_of_range("Invalid active bank.");
        }
        banks_[activeBank_][address] = value;
    } else if (address < MEMORY_SIZE) {
        memory_[address] = value;
    } else {
        throw std::out_of_range("Address out of bounds.");
    }
}

uint8_t Memory::readByte(uint16_t address) const {
    IOHandler handler;

    if (isIOAddress(address, handler)) {
        return handler.read(address);
    }

    if (address < BANK_SIZE) {
        if (activeBank_ >= NUM_BANKS) {
            throw std::out_of_range("Invalid active bank.");
        }
        return banks_[activeBank_][address];
    } else if (address < MEMORY_SIZE) {
        return memory_[address];
    } else {
        throw std::out_of_range("Address out of bounds.");
    }
}

uint16_t Memory::readWord(uint16_t address) const {
    return readByte(address) | (readByte(address + 1) << 8);
}

void Memory::writeWord(uint16_t address, uint16_t value) {
    writeByte(address, value & 0xFF);
    writeByte(address + 1, (value >> 8) & 0xFF);
}

void Memory::loadData(const std::vector<uint8_t>& data, uint16_t startAddress) {
    if (startAddress + data.size() > MEMORY_SIZE) {
        throw std::out_of_range("Data size exceeds memory bounds.");
    }
    for (size_t i = 0; i < data.size(); ++i) {
        memory_[startAddress + i] = data[i];
    }
}

void Memory::dumpMemory(uint16_t start, size_t length) const {
    std::cout << std::hex << std::uppercase; // Set output to hex and uppercase

    for (size_t i = 0; i < length; i += 16) {
        if (start + i >= MEMORY_SIZE)
            break;

        // Address
        std::cout << std::setfill('0') << std::setw(4) << (start + i) << ": ";

        // Hex values
        std::string ascii;
        for (size_t j = 0; j < 16 && (i + j) < length; ++j) {
            size_t addr = start + i + j;
            if (addr >= MEMORY_SIZE) {
                std::cout << "?? ";
                ascii += '?';
                continue;
            }
            uint8_t byte = memory_[addr];
            std::cout << std::setw(2) << static_cast<int>(byte) << " ";
            ascii += (byte >= 32 && byte <= 126) ? static_cast<char>(byte) : '.';
        }

        // Pad if we didn't fill all 16 columns
        if ((i + 16) > length) {
            for (size_t j = 0; j < 16 - (length - i); ++j) {
                std::cout << "   ";
            }
        }

        // ASCII representation
        std::cout << " |" << ascii << "|\n";
    }
    std::cout << std::dec; // Reset output to decimal
    std::cout << std::endl;
}

void Memory::setIOHandler(uint16_t start, uint16_t end,
                         std::function<uint8_t(uint16_t)> readHandler,
                         std::function<void(uint16_t, uint8_t)> writeHandler) {
    if (start > end) {
        throw std::invalid_argument("Invalid I/O range");
    }
    ioHandlers_.push_back({start, end, readHandler, writeHandler});
}

void Memory::validateAddress(uint16_t address) const {
    if (address >= MEMORY_SIZE) {
        throw std::out_of_range("Address out of bounds.");
    }
}

bool Memory::isIOAddress(uint16_t address, IOHandler& handler) const {
    for (const auto& ioHandler : ioHandlers_) {
        if (address >= ioHandler.start && address <= ioHandler.end) {
            handler = ioHandler;
            return true;
        }
    }
    return false;
}

// Banked memory
void Memory::loadBank(const std::vector<uint8_t>& data, uint8_t bank) {
    if (bank >= NUM_BANKS) {
        throw std::out_of_range("Invalid bank index");
    }
    if (data.size() > BANK_SIZE) {
        throw std::invalid_argument("Bank data size exceeds 16KB");
    }
    std::copy(data.begin(), data.end(), banks_[bank].begin());
}

void Memory::selectBank(uint8_t bank) {
    if (bank >= NUM_BANKS) {
        throw std::out_of_range("Invalid bank selection");
    }
    activeBank_ = bank;
}

void Memory::saveState(std::ostream& os) const {
    // Write a marker to ensure proper restoration
    const uint32_t MARKER = 0x52414D53; // "RAMS"
    os.write(reinterpret_cast<const char*>(&MARKER), sizeof(MARKER));

    // Save main memory
    os.write(reinterpret_cast<const char*>(memory_.data()), memory_.size());

    // Save all banks
    for (const auto& bank : banks_) {
        os.write(reinterpret_cast<const char*>(bank.data()), bank.size());
    }

    // Save active bank
    os.write(reinterpret_cast<const char*>(&activeBank_), sizeof(activeBank_));

    if (!os) {
        throw std::runtime_error("Failed to write to output stream.");
    }
}

void Memory::loadState(std::istream& is) {
    // Verify marker
    uint32_t marker;
    is.read(reinterpret_cast<char*>(&marker), sizeof(marker));
    if (marker != 0x52414D53) {
        throw std::runtime_error("Invalid state data.");
    }

    // Load main memory
    is.read(reinterpret_cast<char*>(memory_.data()), memory_.size());

    // Load all banks
    for (auto& bank : banks_) {
        is.read(reinterpret_cast<char*>(bank.data()), bank.size());
    }

    // Load active bank
    is.read(reinterpret_cast<char*>(&activeBank_), sizeof(activeBank_));

    if (!is) {
        throw std::runtime_error("Failed to read from input stream.");
    }

    // Ensure the active bank is within valid range
    if (activeBank_ >= NUM_BANKS) {
        throw std::runtime_error("Invalid active bank in state data.");
    }
}