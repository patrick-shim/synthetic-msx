#include "Z80Registers.hpp"
#include <iostream>
#include <algorithm>

Z80Registers::Z80Registers() :
    AF(0), BC(0), DE(0), HL(0),
    AF_(0), BC_(0), DE_(0), HL_(0),
    SP(0), PC(0), IXH(0), IXL(0), IYH(0), IYL(0),
    I(0), R(0),
    IFF1(false), IFF2(false), interruptMode(0)
{
    // Initialize flags to 0 in both main and alternate flag registers
    flags.C = flags.N = flags.PV = flags.H = flags.Z = flags.S = flags.U1 = flags.U2 = 0;
    flags_.C = flags_.N = flags_.PV = flags_.H = flags_.Z = flags_.S = flags_.U1 = flags_.U2 = 0;
}

void Z80Registers::reset() {
    // Reset all registers to 0, including 16-bit and alternate registers
    AF = BC = DE = HL = 0;
    AF_ = BC_ = DE_ = HL_ = 0;
    SP = PC = IX = IY = 0;
    I = R = 0;

    // Reset interrupt flip-flops and mode
    IFF1 = IFF2 = false;
    interruptMode = 0;

    // Reset flags in both main and alternate flag registers
    flags.C = flags.N = flags.PV = flags.H = flags.Z = flags.S = flags.U1 = flags.U2 = 0;
    flags_.C = flags_.N = flags_.PV = flags_.H = flags_.Z = flags_.S = flags_.U1 = flags_.U2 = 0;
}

void Z80Registers::exchangeAF() {
    std::swap(AF, AF_);
}

void Z80Registers::exchangeMainRegisters() {
    std::swap(BC, BC_);
    std::swap(DE, DE_);
    std::swap(HL, HL_);
}

void Z80Registers::setFlag(Flag flag, bool value) {
    switch (flag) {
        case Carry:          flags.C = value;  break;
        case Subtract:       flags.N = value;  break;
        case ParityOverflow: flags.PV = value; break;
        case HalfCarry:      flags.H = value;  break;
        case Zero:           flags.Z = value;  break;
        case Sign:           flags.S = value;  break;
        default:             std::cerr << "Invalid flag in setFlag." << std::endl; return; // Added error handling
    }
    updateFlagsRegister(); // Update F register after setting a flag
}

bool Z80Registers::getFlag(Flag flag) const {
    switch (flag) {
        case Carry:          return flags.C;
        case Subtract:       return flags.N;
        case ParityOverflow: return flags.PV;
        case HalfCarry:      return flags.H;
        case Zero:           return flags.Z;
        case Sign:           return flags.S;
        default: std::cerr << "Invalid flag in getFlag." << std::endl; return false; // Added error handling
    }
}

void Z80Registers::setAlternateFlag(Flag flag, bool value) {
    switch (flag) {
        case Carry:          flags_.C = value;  break;
        case Subtract:       flags_.N = value;  break;
        case ParityOverflow: flags_.PV = value; break;
        case HalfCarry:      flags_.H = value;  break;
        case Zero:           flags_.Z = value;  break;
        case Sign:           flags_.S = value;  break;
        default:             std::cerr << "Invalid flag in setAlternateFlag." << std::endl; return; // Added error handling
    }
    // Update F' register after setting a flag in the alternate set
    F_ = (flags_.S << 7) | (flags_.Z << 6) | (flags_.U1 << 5) |
         (flags_.H << 4) | (flags_.U2 << 3) | (flags_.PV << 2) |
         (flags_.N << 1) | flags_.C;
}

bool Z80Registers::getAlternateFlag(Flag flag) const {
    switch (flag) {
        case Carry:          return flags_.C;
        case Subtract:       return flags_.N;
        case ParityOverflow: return flags_.PV;
        case HalfCarry:      return flags_.H;
        case Zero:           return flags_.Z;
        case Sign:           return flags_.S;
        default: std::cerr << "Invalid flag in getAlternateFlag." << std::endl; return false; // Added error handling
    }
}

void Z80Registers::updateFlagsRegister() {
    F = (flags.S << 7) |
        (flags.Z << 6) |
        (flags.U1 << 5) |
        (flags.H << 4) |
        (flags.U2 << 3) |
        (flags.PV << 2) |
        (flags.N << 1) |
        flags.C;
}

void Z80Registers::updateFromFlagsRegister() {
    flags.S = (F >> 7) & 1;
    flags.Z = (F >> 6) & 1;
    flags.U1 = (F >> 5) & 1;
    flags.H = (F >> 4) & 1;
    flags.U2 = (F >> 3) & 1;
    flags.PV = (F >> 2) & 1;
    flags.N = (F >> 1) & 1;
    flags.C = F & 1;
}

bool Z80Registers::getUndocumentedFlagU1() const {
    return flags.U1;
}

bool Z80Registers::getUndocumentedFlagU2() const {
    return flags.U2;
}

void Z80Registers::setUndocumentedFlagU1(bool value) {
    flags.U1 = value;
    updateFlagsRegister();
}

void Z80Registers::setUndocumentedFlagU2(bool value) {
    flags.U2 = value;
    updateFlagsRegister();
}

bool Z80Registers::getUndocumentedFlagU1_() const {
    return flags_.U1;
}

bool Z80Registers::getUndocumentedFlagU2_() const {
    return flags_.U2;
}

void Z80Registers::setUndocumentedFlagU1_(bool value) {
    flags_.U1 = value;
    // Update F' register after setting a flag in the alternate set
    F_ = (flags_.S << 7) | (flags_.Z << 6) | (flags_.U1 << 5) |
         (flags_.H << 4) | (flags_.U2 << 3) | (flags_.PV << 2) |
         (flags_.N << 1) | flags_.C;
}

void Z80Registers::setUndocumentedFlagU2_(bool value) {
    flags_.U2 = value;
    // Update F' register after setting a flag in the alternate set
    F_ = (flags_.S << 7) | (flags_.Z << 6) | (flags_.U1 << 5) |
         (flags_.H << 4) | (flags_.U2 << 3) | (flags_.PV << 2) |
         (flags_.N << 1) | flags_.C;
}

void Z80Registers::setFlags(uint8_t flagsValue) {
    flags.C = (flagsValue >> 0) & 1;
    flags.N = (flagsValue >> 1) & 1;
    flags.PV = (flagsValue >> 2) & 1;
    flags.U2 = (flagsValue >> 3) & 1;
    flags.H = (flagsValue >> 4) & 1;
    flags.U1 = (flagsValue >> 5) & 1;
    flags.Z = (flagsValue >> 6) & 1;
    flags.S = (flagsValue >> 7) & 1;
    updateFlagsRegister(); // Update F register after setting flags
}

void Z80Registers::setFlags_(uint8_t flagsValue) {
    flags_.C = (flagsValue >> 0) & 1;
    flags_.N = (flagsValue >> 1) & 1;
    flags_.PV = (flagsValue >> 2) & 1;
    flags_.U2 = (flagsValue >> 3) & 1;
    flags_.H = (flagsValue >> 4) & 1;
    flags_.U1 = (flagsValue >> 5) & 1;
    flags_.Z = (flagsValue >> 6) & 1;
    flags_.S = (flagsValue >> 7) & 1;
    // Update F' register after setting flags in the alternate set
    F_ = (flags_.S << 7) | (flags_.Z << 6) | (flags_.U1 << 5) |
         (flags_.H << 4) | (flags_.U2 << 3) | (flags_.PV << 2) |
         (flags_.N << 1) | flags_.C;
}