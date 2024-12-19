#include "Z80.hpp"
#include <iostream>

Z80::Z80(Memory& memory) : registers(), memory(memory), halted(false), cycles(0) {
    reset();
}

void Z80::reset() {
    registers.reset();
    halted = false;
    cycles = 0;
}

void Z80::loadProgram(const std::vector<uint8_t>& program, uint16_t startAddress) {
    for (size_t i = 0; i < program.size(); ++i) {
        memory.writeByte(startAddress + i, program[i]);
    }
    cycles += program.size() * 3; // Basic memory write timing
}

void Z80::executeInstruction() {
    if (!halted) {
        uint8_t opcode = fetchOpcode();
        cycles += decodeAndExecuteMainInstruction(opcode);
    } else {
        cycles += 4; // Halt instruction takes 4 cycles
    }
    handleInterrupts();
}

uint8_t Z80::fetchOpcode() {
    uint8_t opcode = memory.readByte(registers.PC);
    registers.PC++;
    registers.R = (registers.R + 1) & 0x7F; // Increment refresh register (lower 7 bits)
    cycles += 4; // Basic memory read timing
    return opcode;
}

uint8_t Z80::fetchByte() {
    uint8_t byte = memory.readByte(registers.PC);
    registers.PC++;
    cycles += 3; // Memory read timing
    return byte;
}

uint16_t Z80::fetchWord() {
    uint16_t word = memory.readWord(registers.PC);
    registers.PC += 2;
    cycles += 6; // Double memory read timing
    return word;
}

void Z80::pushByte(uint8_t value) {
    registers.SP--;
    memory.writeByte(registers.SP, value);
    cycles += 3; // Memory write timing
}

uint8_t Z80::popByte() {
    uint8_t value = memory.readByte(registers.SP);
    registers.SP++;
    cycles += 3; // Memory read timing
    return value;
}

void Z80::pushWord(uint16_t value) {
    pushByte((value >> 8) & 0xFF); // Push high byte first
    pushByte(value & 0xFF);        // Then push low byte
    cycles += 1; // Additional overhead for word operation
}

uint16_t Z80::popWord() {
    uint8_t lowByte = popByte();
    uint8_t highByte = popByte();
    cycles += 1; // Additional overhead for word operation
    return (highByte << 8) | lowByte;
}

int8_t Z80::signExtend(uint8_t value) {
    return (int8_t)value;
}

void Z80::calculateParity(uint8_t value) {
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        if ((value >> i) & 0x01) {
            count++;
        }
    }
    registers.setFlag(Z80Registers::ParityOverflow, (count % 2) == 0);
    cycles += 4; // Flag calculation overhead
}

void Z80::updateFlags_SZP(uint8_t value) {
    registers.setFlag(Z80Registers::Sign, (value & 0x80) != 0);
    registers.setFlag(Z80Registers::Zero, value == 0);
    calculateParity(value);
    cycles += 2; // Flag update overhead
}

void Z80::updateFlagsArithmetic(uint8_t result, bool isSubtraction, bool carry, bool halfCarry, bool overflow) {
    registers.setFlag(Z80Registers::Sign, (result & 0x80) != 0);
    registers.setFlag(Z80Registers::Zero, result == 0);
    registers.setFlag(Z80Registers::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::ParityOverflow, overflow);
    registers.setFlag(Z80Registers::Subtract, isSubtraction);
    registers.setFlag(Z80Registers::Carry, carry);
    registers.updateFlagsRegister();
    cycles += 2; // Flag update overhead
}

void Z80::updateFlagsLogical(uint8_t result, bool carry, bool halfCarry) {
    registers.setFlag(Z80Registers::Sign, (result & 0x80) != 0);
    registers.setFlag(Z80Registers::Zero, result == 0);
    registers.setFlag(Z80Registers::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::ParityOverflow, __builtin_parity(result) == 0);
    registers.setFlag(Z80Registers::Subtract, false);
    registers.setFlag(Z80Registers::Carry, carry);
    registers.updateFlagsRegister();
    cycles += 2; // Flag update overhead
}

void Z80::checkBit(uint8_t value, uint8_t bit) {
    bool isSet = (value >> bit) & 0x01;
    registers.setFlag(Z80Registers::Zero, !isSet);
    registers.setFlag(Z80Registers::HalfCarry, true);
    registers.setFlag(Z80Registers::Subtract, false);
    registers.setFlag(Z80Registers::Sign, bit == 7 && isSet);
    registers.setFlag(Z80Registers::ParityOverflow, !isSet);
    registers.updateFlagsRegister();
    cycles += 4; // Bit test overhead
}

void Z80::setInterruptLine(bool high) {
    interrupt_pending = high;
    cycles += 2; // Interrupt line setting overhead
}

int Z80::decodeAndExecuteMainInstruction(uint8_t opcode) {
    switch (opcode) {
        case 0x00: NOP(); return 4;
        case 0x01: LD_BC_d16(); return 10;
        case 0x02: LD_iBC_A(); return 7;
        case 0x03: INC_nn(registers.BC); return 6;
        case 0x04: INC_r(registers.B); return 4;
        case 0x05: DEC_r(registers.B); return 4;
        case 0x06: LD_r_n(registers.B, fetchOpcode()); return 7;
        case 0x07: RLCA(); return 4;
        case 0x08: EX_AF_AF(); return 4;
        case 0x09: ADD_HL_ss(registers.BC); return 11;
        case 0x0A: LD_A_iBC(); return 7;
        case 0x0B: DEC_nn(registers.BC); return 6;
        case 0x0C: INC_r(registers.C); return 4;
        case 0x0D: DEC_r(registers.C); return 4;
        case 0x0E: LD_r_n(registers.C, fetchOpcode()); return 7;
        case 0x0F: RRCA(); return 4;
        case 0x10: DJNZ_d8(); return (registers.B != 0) ? 13 : 8;
        case 0x11: LD_DE_d16(); return 10;
        case 0x12: LD_iDE_A(); return 7;
        case 0x13: INC_nn(registers.DE); return 6;
        case 0x14: INC_r(registers.D); return 4;
        case 0x15: DEC_r(registers.D); return 4;
        case 0x16: LD_r_n(registers.D, fetchOpcode()); return 7;
        case 0x17: RLA(); return 4;
        case 0x18: JR_d8(); return 12;
        case 0x19: ADD_HL_ss(registers.DE); return 11;
        case 0x1A: LD_A_iDE(); return 7;
        case 0x1B: DEC_nn(registers.DE); return 6;
        case 0x1C: INC_r(registers.E); return 4;
        case 0x1D: DEC_r(registers.E); return 4;
        case 0x1E: LD_r_n(registers.E, fetchOpcode()); return 7;
        case 0x1F: RRA(); return 4;
        case 0x20: JR_NZ_d8(); return (registers.getFlag(Z80Registers::Flag::Zero) == 0) ? 12 : 7;
        case 0x21: LD_HL_d16(); return 10;
        case 0x22: LD_inn_HL(); return 16;
        case 0x23: INC_nn(registers.HL); return 6;
        case 0x24: INC_r(registers.H); return 4;
        case 0x25: DEC_r(registers.H); return 4;
        case 0x26: LD_r_n(registers.H, fetchOpcode()); return 7;
        case 0x27: DAA(); return 4;
        case 0x28: JR_Z_d8(); return (registers.getFlag(Z80Registers::Flag::Zero) == 1) ? 12 : 7;
        case 0x29: ADD_HL_ss(registers.HL); return 11;
        case 0x2A: LD_HL_inn(); return 16;
        case 0x2B: DEC_nn(registers.HL); return 6;
        case 0x2C: INC_r(registers.L); return 4;
        case 0x2D: DEC_r(registers.L); return 4;
        case 0x2E: LD_r_n(registers.L, fetchOpcode()); return 7;
        case 0x2F: CPL(); return 4;
        case 0x30: JR_NC_d8(); return (registers.getFlag(Z80Registers::Flag::Carry) == 0) ? 12 : 7;
        case 0x31: LD_SP_d16(); return 10;
        case 0x32: LD_inn_A(); return 13;
        case 0x33: INC_nn(registers.SP); return 6;
        case 0x34: INC_iHL(); return 11;
        case 0x35: DEC_iHL(); return 11;
        case 0x36: LD_iHL_n(fetchOpcode()); return 10;
        case 0x37: SCF(); return 4;
        case 0x38: JR_C_d8(); return (registers.getFlag(Z80Registers::Flag::Carry) == 1) ? 12 : 7;
        case 0x39: ADD_HL_ss(registers.SP); return 11;
        case 0x3A: LD_A_inn(); return 13;
        case 0x3B: DEC_nn(registers.SP); return 6;
        case 0x3C: INC_r(registers.A); return 4;
        case 0x3D: DEC_r(registers.A); return 4;
        case 0x3E: LD_r_n(registers.A, fetchOpcode()); return 7;
        case 0x3F: CCF(); return 4;
        case 0x40: LD_r_r(registers.B, registers.B); return 4;
        case 0x41: LD_r_r(registers.B, registers.C); return 4;
        case 0x42: LD_r_r(registers.B, registers.D); return 4;
        case 0x43: LD_r_r(registers.B, registers.E); return 4;
        case 0x44: LD_r_r(registers.B, registers.H); return 4;
        case 0x45: LD_r_r(registers.B, registers.L); return 4;
        case 0x46: LD_r_iHL(registers.B); return 7;
        case 0x47: LD_r_r(registers.B, registers.A); return 4;
        case 0x48: LD_r_r(registers.C, registers.B); return 4;
        case 0x49: LD_r_r(registers.C, registers.C); return 4;
        case 0x4A: LD_r_r(registers.C, registers.D); return 4;
        case 0x4B: LD_r_r(registers.C, registers.E); return 4;
        case 0x4C: LD_r_r(registers.C, registers.H); return 4;
        case 0x4D: LD_r_r(registers.C, registers.L); return 4;
        case 0x4E: LD_r_iHL(registers.C); return 7;
        case 0x4F: LD_r_r(registers.C, registers.A); return 4;
        case 0x50: LD_r_r(registers.D, registers.B); return 4;
        case 0x51: LD_r_r(registers.D, registers.C); return 4;
        case 0x52: LD_r_r(registers.D, registers.D); return 4;
        case 0x53: LD_r_r(registers.D, registers.E); return 4;
        case 0x54: LD_r_r(registers.D, registers.H); return 4;
        case 0x55: LD_r_r(registers.D, registers.L); return 4;
        case 0x56: LD_r_iHL(registers.D); return 7;
        case 0x57: LD_r_r(registers.D, registers.A); return 4;
        case 0x58: LD_r_r(registers.E, registers.B); return 4;
        case 0x59: LD_r_r(registers.E, registers.C); return 4;
        case 0x5A: LD_r_r(registers.E, registers.D); return 4;
        case 0x5B: LD_r_r(registers.E, registers.E); return 4;
        case 0x5C: LD_r_r(registers.E, registers.H); return 4;
        case 0x5D: LD_r_r(registers.E, registers.L); return 4;
        case 0x5E: LD_r_iHL(registers.E); return 7;
        case 0x5F: LD_r_r(registers.E, registers.A); return 4;
        case 0x60: LD_r_r(registers.H, registers.B); return 4;
        case 0x61: LD_r_r(registers.H, registers.C); return 4;
        case 0x62: LD_r_r(registers.H, registers.D); return 4;
        case 0x63: LD_r_r(registers.H, registers.E); return 4;
        case 0x64: LD_r_r(registers.H, registers.H); return 4;
        case 0x65: LD_r_r(registers.H, registers.L); return 4;
        case 0x66: LD_r_iHL(registers.H); return 7;
        case 0x67: LD_r_r(registers.H, registers.A); return 4;
        case 0x68: LD_r_r(registers.L, registers.B); return 4;
        case 0x69: LD_r_r(registers.L, registers.C); return 4;
        case 0x6A: LD_r_r(registers.L, registers.D); return 4;
        case 0x6B: LD_r_r(registers.L, registers.E); return 4;
        case 0x6C: LD_r_r(registers.L, registers.H); return 4;
        case 0x6D: LD_r_r(registers.L, registers.L); return 4;
        case 0x6E: LD_r_iHL(registers.L); return 7;
        case 0x6F: LD_r_r(registers.L, registers.A); return 4;
        case 0x70: LD_iHL_r(registers.B); return 7;
        case 0x71: LD_iHL_r(registers.C); return 7;
        case 0x72: LD_iHL_r(registers.D); return 7;
        case 0x73: LD_iHL_r(registers.E); return 7;
        case 0x74: LD_iHL_r(registers.H); return 7;
        case 0x75: LD_iHL_r(registers.L); return 7;
        case 0x76: HALT(); return 4;
        case 0x77: LD_iHL_r(registers.A); return 7;
        case 0x78: LD_r_r(registers.A, registers.B); return 4;
        case 0x79: LD_r_r(registers.A, registers.C); return 4;
        case 0x7A: LD_r_r(registers.A, registers.D); return 4;
        case 0x7B: LD_r_r(registers.A, registers.E); return 4;
        case 0x7C: LD_r_r(registers.A, registers.H); return 4;
        case 0x7D: LD_r_r(registers.A, registers.L); return 4;
        case 0x7E: LD_r_iHL(registers.A); return 7;
        case 0x7F: LD_r_r(registers.A, registers.A); return 4;
        case 0x80: ADD_A_r(registers.B); return 4;
        case 0x81: ADD_A_r(registers.C); return 4;
        case 0x82: ADD_A_r(registers.D); return 4;
        case 0x83: ADD_A_r(registers.E); return 4;
        case 0x84: ADD_A_r(registers.H); return 4;
        case 0x85: ADD_A_r(registers.L); return 4;
        case 0x86: ADD_A_iHL(); return 7;
        case 0x87: ADD_A_r(registers.A); return 4;
        case 0x88: ADC_A_r(registers.B); return 4;
        case 0x89: ADC_A_r(registers.C); return 4;
        case 0x8A: ADC_A_r(registers.D); return 4;
        case 0x8B: ADC_A_r(registers.E); return 4;
        case 0x8C: ADC_A_r(registers.H); return 4;
        case 0x8D: ADC_A_r(registers.L); return 4;
        case 0x8E: ADC_A_iHL(); return 7;
        case 0x8F: ADC_A_r(registers.A); return 4;
        case 0x90: SUB_r(registers.B); return 4;
        case 0x91: SUB_r(registers.C); return 4;
        case 0x92: SUB_r(registers.D); return 4;
        case 0x93: SUB_r(registers.E); return 4;
        case 0x94: SUB_r(registers.H); return 4;
        case 0x95: SUB_r(registers.L); return 4;
        case 0x96: SUB_iHL(); return 7;
        case 0x97: SUB_r(registers.A); return 4;
        case 0x98: SBC_A_r(registers.B); return 4;
        case 0x99: SBC_A_r(registers.C); return 4;
        case 0x9A: SBC_A_r(registers.D); return 4;
        case 0x9B: SBC_A_r(registers.E); return 4;
        case 0x9C: SBC_A_r(registers.H); return 4;
        case 0x9D: SBC_A_r(registers.L); return 4;
        case 0x9E: SBC_A_iHL(); return 7;
        case 0x9F: SBC_A_r(registers.A); return 4;
        case 0xA0: AND_r(registers.B); return 4;
        case 0xA1: AND_r(registers.C); return 4;
        case 0xA2: AND_r(registers.D); return 4;
        case 0xA3: AND_r(registers.E); return 4;
        case 0xA4: AND_r(registers.H); return 4;
        case 0xA5: AND_r(registers.L); return 4;
        case 0xA6: AND_iHL(); return 7;
        case 0xA7: AND_r(registers.A); return 4;
        case 0xA8: XOR_r(registers.B); return 4;
        case 0xA9: XOR_r(registers.C); return 4;
        case 0xAA: XOR_r(registers.D); return 4;
        case 0xAB: XOR_r(registers.E); return 4;
        case 0xAC: XOR_r(registers.H); return 4;
        case 0xAD: XOR_r(registers.L); return 4;
        case 0xAE: XOR_iHL(); return 7;
        case 0xAF: XOR_r(registers.A); return 4;
        case 0xB0: OR_r(registers.B); return 4;
        case 0xB1: OR_r(registers.C); return 4;
        case 0xB2: OR_r(registers.D); return 4;
        case 0xB3: OR_r(registers.E); return 4;
        case 0xB4: OR_r(registers.H); return 4;
        case 0xB5: OR_r(registers.L); return 4;
        case 0xB6: OR_iHL(); return 7;
        case 0xB7: OR_r(registers.A); return 4;
        case 0xB8: CP_r(registers.B); return 4;
        case 0xB9: CP_r(registers.C); return 4;
        case 0xBA: CP_r(registers.D); return 4;
        case 0xBB: CP_r(registers.E); return 4;
        case 0xBC: CP_r(registers.H); return 4;
        case 0xBD: CP_r(registers.L); return 4;
        case 0xBE: CP_iHL(); return 7;
        case 0xBF: CP_r(registers.A); return 4;
        case 0xC0: RET_cc(registers.getFlag(Z80Registers::Flag::Zero) == 0); return (registers.getFlag(Z80Registers::Flag::Zero) == 0) ? 11 : 5;
        case 0xC1: POP_qq(registers.BC); return 10;
        case 0xC2: JP_cc_nn(registers.getFlag(Z80Registers::Flag::Zero) == 0); return 10;
        case 0xC3: JP_nn(); return 10;
        case 0xC4: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::Zero) == 0); return (registers.getFlag(Z80Registers::Flag::Zero) == 0) ? 17 : 10;
        case 0xC5: PUSH_qq(registers.BC); return 11;
        case 0xC6: ADD_A_n(fetchByte()); return 7;
        case 0xC7: RST_p(0x00); return 11;
        case 0xC8: RET_cc(registers.getFlag(Z80Registers::Flag::Zero) == 1); return (registers.getFlag(Z80Registers::Flag::Zero) == 1) ? 11 : 5;
        case 0xC9: RET(); return 10;
        case 0xCA: JP_cc_nn(registers.getFlag(Z80Registers::Flag::Zero) == 1); return 10;
        case 0xCB: return decodeAndExecuteCBInstruction(fetchOpcode());
        case 0xCC: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::Zero) == 1); return (registers.getFlag(Z80Registers::Flag::Zero) == 1) ? 17 : 10;
        case 0xCD: CALL_nn(); return 17;
        case 0xCE: ADC_A_n(fetchByte()); return 7;
        case 0xCF: RST_p(0x08); return 11;
        case 0xD0: RET_cc(registers.getFlag(Z80Registers::Flag::Carry) == 0); return (registers.getFlag(Z80Registers::Flag::Carry) == 0) ? 11 : 5;
        case 0xD1: POP_qq(registers.DE); return 10;
        case 0xD2: JP_cc_nn(registers.getFlag(Z80Registers::Flag::Carry) == 0); return 10;
        case 0xD3: OUT_in_A(); return 11;
        case 0xD4: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::Carry) == 0); return (registers.getFlag(Z80Registers::Flag::Carry) == 0) ? 17 : 10;
        case 0xD5: PUSH_qq(registers.DE); return 11;
        case 0xD6: SUB_n(fetchByte()); return 7;
        case 0xD7: RST_p(0x10); return 11;
        case 0xD8: RET_cc(registers.getFlag(Z80Registers::Flag::Carry) == 1); return (registers.getFlag(Z80Registers::Flag::Carry) == 1) ? 11 : 5;
        case 0xD9: EXX(); return 4;
        case 0xDA: JP_cc_nn(registers.getFlag(Z80Registers::Flag::Carry) == 1); return 10;
        case 0xDB: IN_A_in(); return 11;
        case 0xDC: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::Carry) == 1); return (registers.getFlag(Z80Registers::Flag::Carry) == 1) ? 17 : 10;
        case 0xDD: return decodeAndExecuteDDInstruction(fetchOpcode());
        case 0xDE: SBC_A_n(fetchByte()); return 7;
        case 0xDF: RST_p(0x18); return 11;
        case 0xE0: RET_cc(registers.getFlag(Z80Registers::Flag::ParityOverflow) == 0); return (registers.getFlag(Z80Registers::Flag::ParityOverflow) == 0) ? 11 : 5;
        case 0xE1: POP_qq(registers.HL); return 10;
        case 0xE2: JP_cc_nn(registers.getFlag(Z80Registers::Flag::ParityOverflow) == 0); return 10;
        case 0xE3: EX_iSP_HL(); return 19;
        case 0xE4: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::ParityOverflow) == 0); return (registers.getFlag(Z80Registers::Flag::ParityOverflow) == 0) ? 17 : 10;
        case 0xE5: PUSH_qq(registers.HL); return 11;
        case 0xE6: AND_n(fetchByte()); return 7;
        case 0xE7: RST_p(0x20); return 11;
        case 0xE8: RET_cc(registers.getFlag(Z80Registers::Flag::ParityOverflow) == 1); return (registers.getFlag(Z80Registers::Flag::ParityOverflow) == 1) ? 11 : 5;
        case 0xE9: JP_iHL(); return 4;
        case 0xEA: JP_cc_nn(registers.getFlag(Z80Registers::Flag::ParityOverflow) == 1); return 10;
        case 0xEB: EX_DE_HL(); return 4;
        case 0xEC: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::ParityOverflow) == 1); return (registers.getFlag(Z80Registers::Flag::ParityOverflow) == 1) ? 17 : 10;
        case 0xED: return decodeAndExecuteEDInstruction(fetchOpcode());
        case 0xEE: XOR_n(fetchByte()); return 7;
        case 0xEF: RST_p(0x28); return 11;
        case 0xF0: RET_cc(registers.getFlag(Z80Registers::Flag::Sign) == 0); return (registers.getFlag(Z80Registers::Flag::Sign) == 0) ? 11 : 5;
        case 0xF1: POP_qq(registers.AF); return 10;
        case 0xF2: JP_cc_nn(registers.getFlag(Z80Registers::Flag::Sign) == 0); return 10;
        case 0xF3: DI(); return 4;
        case 0xF4: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::Sign) == 0); return (registers.getFlag(Z80Registers::Flag::Sign) == 0) ? 17 : 10;
        case 0xF5: PUSH_qq(registers.AF); return 11;
        case 0xF6: OR_n(fetchByte()); return 7;
        case 0xF7: RST_p(0x30); return 11;
        case 0xF8: RET_cc(registers.getFlag(Z80Registers::Flag::Sign) == 1); return (registers.getFlag(Z80Registers::Flag::Sign) == 1) ? 11 : 5;
        case 0xF9: LD_iSP_HL(); return 6;
        case 0xFA: JP_cc_nn(registers.getFlag(Z80Registers::Flag::Sign) == 1); return 10;
        case 0xFB: EI(); return 4;
        case 0xFC: CALL_cc_nn(registers.getFlag(Z80Registers::Flag::Sign) == 1); return (registers.getFlag(Z80Registers::Flag::Sign) == 1) ? 17 : 10;
        case 0xFD: return decodeAndExecuteFDInstruction(fetchOpcode());
        case 0xFE: CP_n(fetchByte()); return 7;
        case 0xFF: RST_p(0x38); return 11;

        default:
            std::cerr << "Unknown opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

int Z80::decodeAndExecuteCBInstruction(uint8_t opcode) {
    switch (opcode) {
        case 0x00: RLC_r(registers.B); return 8;
        case 0x01: RLC_r(registers.C); return 8;
        case 0x02: RLC_r(registers.D); return 8;
        case 0x03: RLC_r(registers.E); return 8;
        case 0x04: RLC_r(registers.H); return 8;
        case 0x05: RLC_r(registers.L); return 8;
        case 0x06: RLC_iHL(); return 15;
        case 0x07: RLC_r(registers.A); return 8;
        case 0x08: RRC_r(registers.B); return 8;
        case 0x09: RRC_r(registers.C); return 8;
        case 0x0A: RRC_r(registers.D); return 8;
        case 0x0B: RRC_r(registers.E); return 8;
        case 0x0C: RRC_r(registers.H); return 8;
        case 0x0D: RRC_r(registers.L); return 8;
        case 0x0E: RRC_iHL(); return 15;
        case 0x0F: RRC_r(registers.A); return 8;
        case 0x10: RL_r(registers.B); return 8;
        case 0x11: RL_r(registers.C); return 8;
        case 0x12: RL_r(registers.D); return 8;
        case 0x13: RL_r(registers.E); return 8;
        case 0x14: RL_r(registers.H); return 8;
        case 0x15: RL_r(registers.L); return 8;
        case 0x16: RL_iHL(); return 15;
        case 0x17: RL_r(registers.A); return 8;
        case 0x18: RR_r(registers.B); return 8;
        case 0x19: RR_r(registers.C); return 8;
        case 0x1A: RR_r(registers.D); return 8;
        case 0x1B: RR_r(registers.E); return 8;
        case 0x1C: RR_r(registers.H); return 8;
        case 0x1D: RR_r(registers.L); return 8;
        case 0x1E: RR_iHL(); return 15;
        case 0x1F: RR_r(registers.A); return 8;
        case 0x20: SLA_r(registers.B); return 8;
        case 0x21: SLA_r(registers.C); return 8;
        case 0x22: SLA_r(registers.D); return 8;
        case 0x23: SLA_r(registers.E); return 8;
        case 0x24: SLA_r(registers.H); return 8;
        case 0x25: SLA_r(registers.L); return 8;
        case 0x26: SLA_iHL(); return 15;
        case 0x27: SLA_r(registers.A); return 8;
        case 0x28: SRA_r(registers.B); return 8;
        case 0x29: SRA_r(registers.C); return 8;
        case 0x2A: SRA_r(registers.D); return 8;
        case 0x2B: SRA_r(registers.E); return 8;
        case 0x2C: SRA_r(registers.H); return 8;
        case 0x2D: SRA_r(registers.L); return 8;
        case 0x2E: SRA_iHL(); return 15;
        case 0x2F: SRA_r(registers.A); return 8;
        case 0x30: SLL_r(registers.B); return 8;
        case 0x31: SLL_r(registers.C); return 8;
        case 0x32: SLL_r(registers.D); return 8;
        case 0x33: SLL_r(registers.E); return 8;
        case 0x34: SLL_r(registers.H); return 8;
        case 0x35: SLL_r(registers.L); return 8;
        case 0x36: SLL_iHL(); return 15;
        case 0x37: SLL_r(registers.A); return 8;
        case 0x38: SRL_r(registers.B); return 8;
        case 0x39: SRL_r(registers.C); return 8;
        case 0x3A: SRL_r(registers.D); return 8;
        case 0x3B: SRL_r(registers.E); return 8;
        case 0x3C: SRL_r(registers.H); return 8;
        case 0x3D: SRL_r(registers.L); return 8;
        case 0x3E: SRL_iHL(); return 15;
        case 0x3F: SRL_r(registers.A); return 8;
        case 0x40: BIT_b_r(0, registers.B); return 8;
        case 0x41: BIT_b_r(0, registers.C); return 8;
        case 0x42: BIT_b_r(0, registers.D); return 8;
        case 0x43: BIT_b_r(0, registers.E); return 8;
        case 0x44: BIT_b_r(0, registers.H); return 8;
        case 0x45: BIT_b_r(0, registers.L); return 8;
        case 0x46: BIT_b_iHL(0); return 12;
        case 0x47: BIT_b_r(0, registers.A); return 8;
        case 0x48: BIT_b_r(1, registers.B); return 8;
        case 0x49: BIT_b_r(1, registers.C); return 8;
        case 0x4A: BIT_b_r(1, registers.D); return 8;
        case 0x4B: BIT_b_r(1, registers.E); return 8;
        case 0x4C: BIT_b_r(1, registers.H); return 8;
        case 0x4D: BIT_b_r(1, registers.L); return 8;
        case 0x4E: BIT_b_iHL(1); return 12;
        case 0x4F: BIT_b_r(1, registers.A); return 8;
        case 0x50: BIT_b_r(2, registers.B); return 8;
        case 0x51: BIT_b_r(2, registers.C); return 8;
        case 0x52: BIT_b_r(2, registers.D); return 8;
        case 0x53: BIT_b_r(2, registers.E); return 8;
        case 0x54: BIT_b_r(2, registers.H); return 8;
        case 0x55: BIT_b_r(2, registers.L); return 8;
        case 0x56: BIT_b_iHL(2); return 12;
        case 0x57: BIT_b_r(2, registers.A); return 8;
        case 0x58: BIT_b_r(3, registers.B); return 8;
        case 0x59: BIT_b_r(3, registers.C); return 8;
        case 0x5A: BIT_b_r(3, registers.D); return 8;
        case 0x5B: BIT_b_r(3, registers.E); return 8;
        case 0x5C: BIT_b_r(3, registers.H); return 8;
        case 0x5D: BIT_b_r(3, registers.L); return 8;
        case 0x5E: BIT_b_iHL(3); return 12;
        case 0x5F: BIT_b_r(3, registers.A); return 8;
        case 0x60: BIT_b_r(4, registers.B); return 8;
        case 0x61: BIT_b_r(4, registers.C); return 8;
        case 0x62: BIT_b_r(4, registers.D); return 8;
        case 0x63: BIT_b_r(4, registers.E); return 8;
        case 0x64: BIT_b_r(4, registers.H); return 8;
        case 0x65: BIT_b_r(4, registers.L); return 8;
        case 0x66: BIT_b_iHL(4); return 12;
        case 0x67: BIT_b_r(4, registers.A); return 8;
        case 0x68: BIT_b_r(5, registers.B); return 8;
        case 0x69: BIT_b_r(5, registers.C); return 8;
        case 0x6A: BIT_b_r(5, registers.D); return 8;
        case 0x6B: BIT_b_r(5, registers.E); return 8;
        case 0x6C: BIT_b_r(5, registers.H); return 8;
        case 0x6D: BIT_b_r(5, registers.L); return 8;
        case 0x6E: BIT_b_iHL(5); return 12;
        case 0x6F: BIT_b_r(5, registers.A); return 8;
        case 0x70: BIT_b_r(6, registers.B); return 8;
        case 0x71: BIT_b_r(6, registers.C); return 8;
        case 0x72: BIT_b_r(6, registers.D); return 8;
        case 0x73: BIT_b_r(6, registers.E); return 8;
        case 0x74: BIT_b_r(6, registers.H); return 8;
        case 0x75: BIT_b_r(6, registers.L); return 8;
        case 0x76: BIT_b_iHL(6); return 12;
        case 0x77: BIT_b_r(6, registers.A); return 8;
        case 0x78: BIT_b_r(7, registers.B); return 8;
        case 0x79: BIT_b_r(7, registers.C); return 8;
        case 0x7A: BIT_b_r(7, registers.D); return 8;
        case 0x7B: BIT_b_r(7, registers.E); return 8;
        case 0x7C: BIT_b_r(7, registers.H); return 8;
        case 0x7D: BIT_b_r(7, registers.L); return 8;
        case 0x7E: BIT_b_iHL(7); return 12;
        case 0x7F: BIT_b_r(7, registers.A); return 8;
        case 0x80: RES_b_r(0, registers.B); return 8;
        case 0x81: RES_b_r(0, registers.C); return 8;
        case 0x82: RES_b_r(0, registers.D); return 8;
        case 0x83: RES_b_r(0, registers.E); return 8;
        case 0x84: RES_b_r(0, registers.H); return 8;
        case 0x85: RES_b_r(0, registers.L); return 8;
        case 0x86: RES_b_iHL(0); return 15;
        case 0x87: RES_b_r(0, registers.A); return 8;
        case 0x88: RES_b_r(1, registers.B); return 8;
        case 0x89: RES_b_r(1, registers.C); return 8;
        case 0x8A: RES_b_r(1, registers.D); return 8;
        case 0x8B: RES_b_r(1, registers.E); return 8;
        case 0x8C: RES_b_r(1, registers.H); return 8;
        case 0x8D: RES_b_r(1, registers.L); return 8;
        case 0x8E: RES_b_iHL(1); return 15;
        case 0x8F: RES_b_r(1, registers.A); return 8;
        case 0x90: RES_b_r(2, registers.B); return 8;
        case 0x91: RES_b_r(2, registers.C); return 8;
        case 0x92: RES_b_r(2, registers.D); return 8;
        case 0x93: RES_b_r(2, registers.E); return 8;
        case 0x94: RES_b_r(2, registers.H); return 8;
        case 0x95: RES_b_r(2, registers.L); return 8;
        case 0x96: RES_b_iHL(2); return 15;
        case 0x97: RES_b_r(2, registers.A); return 8;
        case 0x98: RES_b_r(3, registers.B); return 8;
        case 0x99: RES_b_r(3, registers.C); return 8;
        case 0x9A: RES_b_r(3, registers.D); return 8;
        case 0x9B: RES_b_r(3, registers.E); return 8;
        case 0x9C: RES_b_r(3, registers.H); return 8;
        case 0x9D: RES_b_r(3, registers.L); return 8;
        case 0x9E: RES_b_iHL(3); return 15;
        case 0x9F: RES_b_r(3, registers.A); return 8;
        case 0xA0: RES_b_r(4, registers.B); return 8;
        case 0xA1: RES_b_r(4, registers.C); return 8;
        case 0xA2: RES_b_r(4, registers.D); return 8;
        case 0xA3: RES_b_r(4, registers.E); return 8;
        case 0xA4: RES_b_r(4, registers.H); return 8;
        case 0xA5: RES_b_r(4, registers.L); return 8;
        case 0xA6: RES_b_iHL(4); return 15;
        case 0xA7: RES_b_r(4, registers.A); return 8;
        case 0xA8: RES_b_r(5, registers.B); return 8;
        case 0xA9: RES_b_r(5, registers.C); return 8;
        case 0xAA: RES_b_r(5, registers.D); return 8;
        case 0xAB: RES_b_r(5, registers.E); return 8;
        case 0xAC: RES_b_r(5, registers.H); return 8;
        case 0xAD: RES_b_r(5, registers.L); return 8;
        case 0xAE: RES_b_iHL(5); return 15;
        case 0xAF: RES_b_r(5, registers.A); return 8;
        case 0xB0: RES_b_r(6, registers.B); return 8;
        case 0xB1: RES_b_r(6, registers.C); return 8;
        case 0xB2: RES_b_r(6, registers.D); return 8;
        case 0xB3: RES_b_r(6, registers.E); return 8;
        case 0xB4: RES_b_r(6, registers.H); return 8;
        case 0xB5: RES_b_r(6, registers.L); return 8;
        case 0xB6: RES_b_iHL(6); return 15;
        case 0xB7: RES_b_r(6, registers.A); return 8;
        case 0xB8: RES_b_r(7, registers.B); return 8;
        case 0xB9: RES_b_r(7, registers.C); return 8;
        case 0xBA: RES_b_r(7, registers.D); return 8;
        case 0xBB: RES_b_r(7, registers.E); return 8;
        case 0xBC: RES_b_r(7, registers.H); return 8;
        case 0xBD: RES_b_r(7, registers.L); return 8;
        case 0xBE: RES_b_iHL(7); return 15;
        case 0xBF: RES_b_r(7, registers.A); return 8;
        case 0xC0: SET_b_r(0, registers.B); return 8;
        case 0xC1: SET_b_r(0, registers.C); return 8;
        case 0xC2: SET_b_r(0, registers.D); return 8;
        case 0xC3: SET_b_r(0, registers.E); return 8;
        case 0xC4: SET_b_r(0, registers.H); return 8;
        case 0xC5: SET_b_r(0, registers.L); return 8;
        case 0xC6: SET_b_iHL(0); return 15;
        case 0xC7: SET_b_r(0, registers.A); return 8;
        case 0xC8: SET_b_r(1, registers.B); return 8;
        case 0xC9: SET_b_r(1, registers.C); return 8;
        case 0xCA: SET_b_r(1, registers.D); return 8;
        case 0xCB: SET_b_r(1, registers.E); return 8;
        case 0xCC: SET_b_r(1, registers.H); return 8;
        case 0xCD: SET_b_r(1, registers.L); return 8;
        case 0xCE: SET_b_iHL(1); return 15;
        case 0xCF: SET_b_r(1, registers.A); return 8;
        case 0xD0: SET_b_r(2, registers.B); return 8;
        case 0xD1: SET_b_r(2, registers.C); return 8;
        case 0xD2: SET_b_r(2, registers.D); return 8;
        case 0xD3: SET_b_r(2, registers.E); return 8;
        case 0xD4: SET_b_r(2, registers.H); return 8;
        case 0xD5: SET_b_r(2, registers.L); return 8;
        case 0xD6: SET_b_iHL(2); return 15;
        case 0xD7: SET_b_r(2, registers.A); return 8;
        case 0xD8: SET_b_r(3, registers.B); return 8;
        case 0xD9: SET_b_r(3, registers.C); return 8;
        case 0xDA: SET_b_r(3, registers.D); return 8;
        case 0xDB: SET_b_r(3, registers.E); return 8;
        case 0xDC: SET_b_r(3, registers.H); return 8;
        case 0xDD: SET_b_r(3, registers.L); return 8;
        case 0xDE: SET_b_iHL(3); return 15;
        case 0xDF: SET_b_r(3, registers.A); return 8;
        case 0xE0: SET_b_r(4, registers.B); return 8;
        case 0xE1: SET_b_r(4, registers.C); return 8;
        case 0xE2: SET_b_r(4, registers.D); return 8;
        case 0xE3: SET_b_r(4, registers.E); return 8;
        case 0xE4: SET_b_r(4, registers.H); return 8;
        case 0xE5: SET_b_r(4, registers.L); return 8;
        case 0xE6: SET_b_iHL(4); return 15;
        case 0xE7: SET_b_r(4, registers.A); return 8;
        case 0xE8: SET_b_r(5, registers.B); return 8;
        case 0xE9: SET_b_r(5, registers.C); return 8;
        case 0xEA: SET_b_r(5, registers.D); return 8;
        case 0xEB: SET_b_r(5, registers.E); return 8;
        case 0xEC: SET_b_r(5, registers.H); return 8;
        case 0xED: SET_b_r(5, registers.L); return 8;
        case 0xEE: SET_b_iHL(5); return 15;
        case 0xEF: SET_b_r(5, registers.A); return 8;
        case 0xF0: SET_b_r(6, registers.B); return 8;
        case 0xF1: SET_b_r(6, registers.C); return 8;
        case 0xF2: SET_b_r(6, registers.D); return 8;
        case 0xF3: SET_b_r(6, registers.E); return 8;
        case 0xF4: SET_b_r(6, registers.H); return 8;
        case 0xF5: SET_b_r(6, registers.L); return 8;
        case 0xF6: SET_b_iHL(6); return 15;
        case 0xF7: SET_b_r(6, registers.A); return 8;
        case 0xF8: SET_b_r(7, registers.B); return 8;
        case 0xF9: SET_b_r(7, registers.C); return 8;
        case 0xFA: SET_b_r(7, registers.D); return 8;
        case 0xFB: SET_b_r(7, registers.E); return 8;
        case 0xFC: SET_b_r(7, registers.H); return 8;
        case 0xFD: SET_b_r(7, registers.L); return 8;
        case 0xFE: SET_b_iHL(7); return 15;
        case 0xFF: SET_b_r(7, registers.A); return 8;
        default:
            std::cerr << "Unknown CB opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 0;
    }
}

int Z80::decodeAndExecuteEDInstruction(uint8_t opcode) {
    switch (opcode) {
        case 0x40: IN_r_iC(registers.B); return 12;
        case 0x41: OUT_iC_r(registers.B); return 12;
        case 0x42: SBC_HL_ss(registers.BC); return 15;
        case 0x43: LD_inn_dd(registers.BC); return 20;
        case 0x44: NEG(); return 8;
        case 0x45: RETN(); return 14;
        case 0x46: IM_x(0); return 8;
        case 0x47: LD_I_A(); return 9;
        case 0x48: IN_r_iC(registers.C); return 12;
        case 0x49: OUT_iC_r(registers.C); return 12;
        case 0x4A: ADC_HL_ss(registers.BC); return 15;
        case 0x4B: LD_dd_inn(registers.BC); return 20;
        case 0x4C: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x4D: RETI(); return 14;
        case 0x4E: IM_x(0); return 8; // Duplicate opcode, same as 0x46
        case 0x4F: LD_R_A(); return 9;
        case 0x50: IN_r_iC(registers.D); return 12;
        case 0x51: OUT_iC_r(registers.D); return 12;
        case 0x52: SBC_HL_ss(registers.DE); return 15;
        case 0x53: LD_inn_dd(registers.DE); return 20;
        case 0x54: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x55: RETN(); return 14; // Duplicate opcode, same as 0x45
        case 0x56: IM_x(1); return 8;
        case 0x57: LD_A_I(); return 9;
        case 0x58: IN_r_iC(registers.E); return 12;
        case 0x59: OUT_iC_r(registers.E); return 12;
        case 0x5A: ADC_HL_ss(registers.DE); return 15;
        case 0x5B: LD_dd_inn(registers.DE); return 20;
        case 0x5C: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x5D: RETN(); return 14; // Duplicate opcode, same as 0x45
        case 0x5E: IM_x(2); return 8; // Duplicate opcode, same as 0x5E
        case 0x5F: LD_A_R(); return 9;
        case 0x60: IN_r_iC(registers.H); return 12;
        case 0x61: OUT_iC_r(registers.H); return 12;
        case 0x62: SBC_HL_ss(registers.HL); return 15;
        case 0x63: LD_inn_dd(registers.HL); return 20;
        case 0x64: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x65: RETN(); return 14; // Duplicate opcode, same as 0x45
        case 0x66: IM_x(0); return 8; // Duplicate opcode, same as 0x46
        case 0x67: RRD(); return 18;
        case 0x68: IN_r_iC(registers.L); return 12;
        case 0x69: OUT_iC_r(registers.L); return 12;
        case 0x6A: ADC_HL_ss(registers.HL); return 15;
        case 0x6B: LD_dd_inn(registers.HL); return 20;
        case 0x6C: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x6D: RETN(); return 14; // Duplicate opcode, same as 0x45
        case 0x6E: IM_x(0); return 8; // Duplicate opcode, same as 0x46
        case 0x6F: RLD(); return 18;
        case 0x70: IN_f_iC(); return 12;
        case 0x71: OUT_iC_0(); return 12;
        case 0x72: SBC_HL_ss(registers.SP); return 15;
        case 0x73: LD_inn_dd(registers.SP); return 20;
        case 0x74: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x75: RETN(); return 14; // Duplicate opcode, same as 0x45
        case 0x76: IM_x(1); return 8; // Duplicate opcode, same as 0x56
        case 0x77: NOP(); return 4; // Undocumented opcode, same as NOP
        case 0x78: IN_r_iC(registers.A); return 12;
        case 0x79: OUT_iC_r(registers.A); return 12;
        case 0x7A: ADC_HL_ss(registers.SP); return 15;
        case 0x7B: LD_dd_inn(registers.SP); return 20;
        case 0x7C: NEG(); return 8; // Duplicate opcode, same as 0x44
        case 0x7D: RETN(); return 14; // Duplicate opcode, same as 0x45
        case 0x7E: IM_x(2); return 8; // Duplicate opcode, same as 0x5E
        case 0x7F: NOP(); return 4; // Undocumented opcode, same as NOP
        case 0xA0: LDI(); return 16;
        case 0xA1: CPI(); return 16;
        case 0xA2: INI(); return 16;
        case 0xA3: OUTI(); return 16;
        case 0xA8: LDD(); return 16;
        case 0xA9: CPD(); return 16;
        case 0xAA: IND(); return 16;
        case 0xAB: OUTD(); return 16;
        case 0xB0: LDIR(); return (registers.BC != 0) ? 21 : 16;
        case 0xB1: CPIR(); return (registers.BC != 0 && !registers.getFlag(Z80Registers::Flag::Zero)) ? 21 : 16;
        case 0xB2: INIR(); return (registers.B != 0) ? 21 : 16;
        case 0xB3: OTIR(); return (registers.B != 0) ? 21 : 16;
        case 0xB8: LDDR(); return (registers.BC != 0) ? 21 : 16;
        case 0xB9: CPDR(); return (registers.BC != 0 && !registers.getFlag(Z80Registers::Flag::Zero)) ? 21 : 16;
        case 0xBA: INDR(); return (registers.B != 0) ? 21 : 16;
        case 0xBB: OTDR(); return (registers.B != 0) ? 21 : 16;
        default:
            std::cerr << "Unknown ED opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

int Z80::decodeAndExecuteDDInstruction(uint8_t opcode) {
    switch (opcode)
    {
        case 0x09: ADD_IX_pp(registers.BC); return 15;
        case 0x19: ADD_IX_pp(registers.DE); return 15;
        case 0x21: LD_IXIY_nn(registers.IX); return 14;
        case 0x22: LD_inn_IXIY(registers.IX); return 20;
        case 0x23: INC_IXIY(registers.IX); return 10;
        case 0x24: INC_IXIYH(registers.IXH); return 8;
        case 0x25: DEC_IXIYH(registers.IXH); return 8;
        case 0x26: LD_IXIYH_n(registers.IXH, fetchByte()); return 11;
        case 0x29: ADD_IX_pp(registers.IX); return 15;
        case 0x2A: LD_IXIY_inn(registers.IX); return 20;
        case 0x2B: DEC_IXIY(registers.IX); return 10;
        case 0x2C: INC_IXIYL(registers.IXL); return 8;
        case 0x2D: DEC_IXIYL(registers.IXL); return 8;
        case 0x2E: LD_IXIYL_n(registers.IXL, fetchByte()); return 11;
        case 0x34: INC_IXIYd(registers.IX); return 23;
        case 0x35: DEC_IXIYd(registers.IX); return 23;
        case 0x36: LD_IXIYd_n(registers.IX, fetchByte()); return 19;
        case 0x39: ADD_IX_pp(registers.SP); return 15;
        case 0x44: LD_r_IXIYd(registers.B, registers.IX); return 19;
        case 0x45: LD_r_IXIYd(registers.B, registers.IX); return 19;
        case 0x46: LD_r_IXIYd(registers.B, registers.IX); return 19;
        case 0x4C: LD_r_IXIYd(registers.C, registers.IX); return 19;
        case 0x4D: LD_r_IXIYd(registers.C, registers.IX); return 19;
        case 0x4E: LD_r_IXIYd(registers.C, registers.IX); return 19;
        case 0x54: LD_r_IXIYd(registers.D, registers.IX); return 19;
        case 0x55: LD_r_IXIYd(registers.D, registers.IX); return 19;
       case 0x56: LD_r_IXIYd(registers.D, registers.IX); return 19;
        case 0x5C: LD_r_IXIYd(registers.E, registers.IX); return 19;
        case 0x5D: LD_r_IXIYd(registers.E, registers.IX); return 19;
        case 0x5E: LD_r_IXIYd(registers.E, registers.IX); return 19;
        case 0x60: LD_r_r(registers.IXH, registers.B); return 8;
        case 0x61: LD_r_r(registers.IXH, registers.C); return 8;
        case 0x62: LD_r_r(registers.IXH, registers.D); return 8;
        case 0x63: LD_r_r(registers.IXH, registers.E); return 8;
        case 0x64: /* LD IXH,IXH */ return 8; // Effectively a NOP
        case 0x65: LD_r_r(registers.IXH, registers.IXL); return 8;
        case 0x66: LD_r_IXIYd(registers.H, registers.IX); return 19;
        case 0x67: LD_r_r(registers.IXH, registers.A); return 8;
        case 0x68: LD_r_r(registers.IXL, registers.B); return 8;
        case 0x69: LD_r_r(registers.IXL, registers.C); return 8;
        case 0x6A: LD_r_r(registers.IXL, registers.D); return 8;
        case 0x6B: LD_r_r(registers.IXL, registers.E); return 8;
        case 0x6C: LD_r_r(registers.IXL, registers.IXH); return 8;
        case 0x6D: /* LD IXL,IXL */ return 8; // Effectively a NOP
        case 0x6E: LD_r_IXIYd(registers.L, registers.IX); return 19;
        case 0x6F: LD_r_r(registers.IXL, registers.A); return 8;
        case 0x70: LD_IXIYd_r(registers.IX, registers.B); return 19;
        case 0x71: LD_IXIYd_r(registers.IX, registers.C); return 19;
        case 0x72: LD_IXIYd_r(registers.IX, registers.D); return 19;
        case 0x73: LD_IXIYd_r(registers.IX, registers.E); return 19;
        case 0x74: LD_IXIYd_r(registers.IX, registers.H); return 19;
        case 0x75: LD_IXIYd_r(registers.IX, registers.L); return 19;
        case 0x77: LD_IXIYd_r(registers.IX, registers.A); return 19;
        case 0x7C: LD_r_IXIYd(registers.A, registers.IX); return 19;
        case 0x7D: LD_r_IXIYd(registers.A, registers.IX); return 19;
        case 0x7E: LD_r_IXIYd(registers.A, registers.IX); return 19;
        case 0x84: ADD_A_IXIYd(registers.IX); return 19;
        case 0x85: ADD_A_IXIYd(registers.IX); return 19;
        case 0x86: ADD_A_IXIYd(registers.IX); return 19;
        case 0x8C: ADC_A_IXIYd(registers.IX); return 19;
        case 0x8D: ADC_A_IXIYd(registers.IX); return 19;
        case 0x8E: ADC_A_IXIYd(registers.IX); return 19;
        case 0x94: SUB_IXIYd(registers.IX); return 19;
        case 0x95: SUB_IXIYd(registers.IX); return 19;
        case 0x96: SUB_IXIYd(registers.IX); return 19;
        case 0x9C: SBC_A_IXIYd(registers.IX); return 19;
        case 0x9D: SBC_A_IXIYd(registers.IX); return 19;
        case 0x9E: SBC_A_IXIYd(registers.IX); return 19;
        case 0xA4: AND_IXIYd(registers.IX); return 19;
        case 0xA5: AND_IXIYd(registers.IX); return 19;
        case 0xA6: AND_IXIYd(registers.IX); return 19;
        case 0xAC: XOR_IXIYd(registers.IX); return 19;
        case 0xAD: XOR_IXIYd(registers.IX); return 19;
        case 0xAE: XOR_IXIYd(registers.IX); return 19;
        case 0xB4: OR_IXIYd(registers.IX); return 19;
        case 0xB5: OR_IXIYd(registers.IX); return 19;
        case 0xB6: OR_IXIYd(registers.IX); return 19;
        case 0xBC: CP_IXIYd(registers.IX); return 19;
        case 0xBD: CP_IXIYd(registers.IX); return 19;
        case 0xBE: CP_IXIYd(registers.IX); return 19;
        case 0xCB: return decodeAndExecuteDDFDCBInstruction(fetchOpcode(), registers.IX);
        case 0xE1: POP_qq(registers.IX); return 14;
        case 0xE3: EX_iSP_IXIY(registers.IX); return 23;
        case 0xE5: PUSH_qq(registers.IX); return 15;
        case 0xE9: JP_iIXIY(registers.IX); return 8;
        case 0xF9: LD_SP_IXIY(registers.IX); return 10;
        default:
            std::cerr << "Unknown DD opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

int Z80::decodeAndExecuteFDInstruction(uint8_t opcode) {
    switch (opcode) {
        case 0x09: ADD_IX_pp(registers.BC); return 15;
        case 0x19: ADD_IX_pp(registers.DE); return 15;
        case 0x21: LD_IXIY_nn(registers.IY); return 14;
        case 0x22: LD_inn_IXIY(registers.IY); return 20;
        case 0x23: INC_IXIY(registers.IY); return 10;
        case 0x24: INC_IXIYH(registers.IYH); return 8;
        case 0x25: DEC_IXIYH(registers.IYH); return 8;
        case 0x26: LD_IXIYH_n(registers.IYH, fetchByte()); return 11;
        case 0x29: ADD_IX_pp(registers.IY); return 15;
        case 0x2A: LD_IXIY_inn(registers.IY); return 20;
        case 0x2B: DEC_IXIY(registers.IY); return 10;
        case 0x2C: INC_IXIYL(registers.IYL); return 8;
        case 0x2D: DEC_IXIYL(registers.IYL); return 8;
        case 0x2E: LD_IXIYL_n(registers.IYL, fetchByte()); return 11;
        case 0x34: INC_IXIYd(registers.IY); return 23;
        case 0x35: DEC_IXIYd(registers.IY); return 23;
        case 0x36: LD_IXIYd_n(registers.IY, fetchByte()); return 19;
        case 0x39: ADD_IX_pp(registers.SP); return 15;
        case 0x44: LD_r_IXIYd(registers.B, registers.IY); return 19;
        case 0x45: LD_r_IXIYd(registers.B, registers.IY); return 19;
        case 0x46: LD_r_IXIYd(registers.B, registers.IY); return 19;
        case 0x4C: LD_r_IXIYd(registers.C, registers.IY); return 19;
        case 0x4D: LD_r_IXIYd(registers.C, registers.IY); return 19;
        case 0x4E: LD_r_IXIYd(registers.C, registers.IY); return 19;
        case 0x54: LD_r_IXIYd(registers.D, registers.IY); return 19;
        case 0x55: LD_r_IXIYd(registers.D, registers.IY); return 19;
        case 0x56: LD_r_IXIYd(registers.D, registers.IY); return 19;
        case 0x5C: LD_r_IXIYd(registers.E, registers.IY); return 19;
        case 0x5D: LD_r_IXIYd(registers.E, registers.IY); return 19;
        case 0x5E: LD_r_IXIYd(registers.E, registers.IY); return 19;
        case 0x60: LD_r_r(registers.IYH, registers.B); return 8;
        case 0x61: LD_r_r(registers.IYH, registers.C); return 8;
        case 0x62: LD_r_r(registers.IYH, registers.D); return 8;
        case 0x63: LD_r_r(registers.IYH, registers.E); return 8;
        case 0x64: /* LD IYH,IYH */ return 8; // Effectively a NOP
        case 0x65: LD_r_r(registers.IYH, registers.IYL); return 8;
        case 0x66: LD_r_IXIYd(registers.H, registers.IY); return 19;
        case 0x67: LD_r_r(registers.IYH, registers.A); return 8;
        case 0x68: LD_r_r(registers.IYL, registers.B); return 8;
        case 0x69: LD_r_r(registers.IYL, registers.C); return 8;
        case 0x6A: LD_r_r(registers.IYL, registers.D); return 8;
        case 0x6B: LD_r_r(registers.IYL, registers.E); return 8;
        case 0x6C: LD_r_r(registers.IYL, registers.IYH); return 8;
        case 0x6D: /* LD IYL,IYL */ return 8; // Effectively a NOP
        case 0x6E: LD_r_IXIYd(registers.L, registers.IY); return 19;
        case 0x6F: LD_r_r(registers.IYL, registers.A); return 8;
        case 0x70: LD_IXIYd_r(registers.IY, registers.B); return 19;
        case 0x71: LD_IXIYd_r(registers.IY, registers.C); return 19;
        case 0x72: LD_IXIYd_r(registers.IY, registers.D); return 19;
        case 0x73: LD_IXIYd_r(registers.IY, registers.E); return 19;
        case 0x74: LD_IXIYd_r(registers.IY, registers.H); return 19;
        case 0x75: LD_IXIYd_r(registers.IY, registers.L); return 19;
        case 0x77: LD_IXIYd_r(registers.IY, registers.A); return 19;
        case 0x7C: LD_r_IXIYd(registers.A, registers.IY); return 19;
        case 0x7D: LD_r_IXIYd(registers.A, registers.IY); return 19;
        case 0x7E: LD_r_IXIYd(registers.A, registers.IY); return 19;
        case 0x84: ADD_A_IXIYd(registers.IY); return 19;
        case 0x85: ADD_A_IXIYd(registers.IY); return 19;
        case 0x86: ADD_A_IXIYd(registers.IY); return 19;
        case 0x8C: ADC_A_IXIYd(registers.IY); return 19;
        case 0x8D: ADC_A_IXIYd(registers.IY); return 19;
        case 0x8E: ADC_A_IXIYd(registers.IY); return 19;
        case 0x94: SUB_IXIYd(registers.IY); return 19;
        case 0x95: SUB_IXIYd(registers.IY); return 19;
        case 0x96: SUB_IXIYd(registers.IY); return 19;
        case 0x9C: SBC_A_IXIYd(registers.IY); return 19;
        case 0x9D: SBC_A_IXIYd(registers.IY); return 19;
        case 0x9E: SBC_A_IXIYd(registers.IY); return 19;
        case 0xA4: AND_IXIYd(registers.IY); return 19;
        case 0xA5: AND_IXIYd(registers.IY); return 19;
        case 0xA6: AND_IXIYd(registers.IY); return 19;
        case 0xAC: XOR_IXIYd(registers.IY); return 19;
        case 0xAD: XOR_IXIYd(registers.IY); return 19;
        case 0xAE: XOR_IXIYd(registers.IY); return 19;
        case 0xB4: OR_IXIYd(registers.IY); return 19;
        case 0xB5: OR_IXIYd(registers.IY); return 19;
        case 0xB6: OR_IXIYd(registers.IY); return 19;
        case 0xBC: CP_IXIYd(registers.IY); return 19;
        case 0xBD: CP_IXIYd(registers.IY); return 19;
        case 0xBE: CP_IXIYd(registers.IY); return 19;
        case 0xCB: return decodeAndExecuteDDFDCBInstruction(fetchOpcode(), registers.IY);
        case 0xE1: POP_qq(registers.IY); return 14;
        case 0xE3: EX_iSP_IXIY(registers.IY); return 23;
        case 0xE5: PUSH_qq(registers.IY); return 15;
        case 0xE9: JP_iIXIY(registers.IY); return 8;
        case 0xF9: LD_SP_IXIY(registers.IY); return 10;
        default:
            std::cerr << "Unknown FD opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

int Z80::decodeAndExecuteDDFDInstruction(uint8_t opcode, uint16_t& indexRegister) {
    // The majority of these instructions are handled by the other decoding functions
    // This function is mainly for instructions that directly modify the index registers
    switch (opcode) {
        case 0x09: ADD_IXIY_ss(indexRegister, registers.BC); return 15;
        case 0x19: ADD_IXIY_ss(indexRegister, registers.DE); return 15;
        case 0x21: LD_IXIY_nn(indexRegister); return 14;
        case 0x22: LD_inn_IXIY(indexRegister); return 20;
        case 0x23: INC_IXIY(indexRegister); return 10;
        case 0x29: ADD_IXIY_ss(indexRegister, indexRegister); return 15;
        case 0x2A: LD_IXIY_inn(indexRegister); return 20;
        case 0x2B: DEC_IXIY(indexRegister); return 10;
        case 0x34: INC_IXIYd(indexRegister); return 23;
        case 0x35: DEC_IXIYd(indexRegister); return 23;
        case 0x36: LD_IXIYd_n(indexRegister, fetchByte()); return 19;
        case 0x39: ADD_IXIY_ss(indexRegister, registers.SP); return 15;
        case 0x86: ADD_A_IXIYd(indexRegister); return 19;
        case 0x8E: ADC_A_IXIYd(indexRegister); return 19;
        case 0x96: SUB_IXIYd(indexRegister); return 19;
        case 0x9E: SBC_A_IXIYd(indexRegister); return 19;
        case 0xA6: AND_IXIYd(indexRegister); return 19;
        case 0xAE: XOR_IXIYd(indexRegister); return 19;
        case 0xB6: OR_IXIYd(indexRegister); return 19;
        case 0xBE: CP_IXIYd(indexRegister); return 19;
        case 0xE1: POP_qq(indexRegister); return 14;
        case 0xE3: EX_iSP_IXIY(indexRegister); return 23;
        case 0xE5: PUSH_qq(indexRegister); return 15;
        case 0xE9: JP_iIXIY(indexRegister); return 8;
        case 0xF9: LD_SP_IXIY(indexRegister); return 10;
        default:
            std::cerr << "Unhandled DDFD opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

int Z80::decodeAndExecuteDDFDCBInstruction(uint8_t opcode, uint16_t& indexRegister) {
    int8_t displacement = signExtend(fetchByte());
    uint8_t nextOpcode = fetchOpcode();
    uint16_t address = indexRegister + displacement;
    uint8_t value = memory.readByte(address);

    switch (nextOpcode) {
        case 0x06: RLC_IXIYd(indexRegister); return 23;
        case 0x0E: RRC_IXIYd(indexRegister); return 23;
        case 0x16: RL_IXIYd(indexRegister); return 23;
        case 0x1E: RR_IXIYd(indexRegister); return 23;
        case 0x26: SLA_IXIYd(indexRegister); return 23;
        case 0x2E: SRA_IXIYd(indexRegister); return 23;
        case 0x36: SLL_IXIYd(indexRegister); return 23;
        case 0x3E: SRL_IXIYd(indexRegister); return 23;
        case 0x46: BIT_b_IXIYd(0, indexRegister); return 20;
        case 0x4E: BIT_b_IXIYd(1, indexRegister); return 20;
        case 0x56: BIT_b_IXIYd(2, indexRegister); return 20;
        case 0x5E: BIT_b_IXIYd(3, indexRegister); return 20;
        case 0x66: BIT_b_IXIYd(4, indexRegister); return 20;
        case 0x6E: BIT_b_IXIYd(5, indexRegister); return 20;
        case 0x76: BIT_b_IXIYd(6, indexRegister); return 20;
        case 0x7E: BIT_b_IXIYd(7, indexRegister); return 20;
        case 0x86: RES_b_IXIYd(0, indexRegister); return 23;
        case 0x8E: RES_b_IXIYd(1, indexRegister); return 23;
        case 0x96: RES_b_IXIYd(2, indexRegister); return 23;
        case 0x9E: RES_b_IXIYd(3, indexRegister); return 23;
        case 0xA6: RES_b_IXIYd(4, indexRegister); return 23;
        case 0xAE: RES_b_IXIYd(5, indexRegister); return 23;
        case 0xB6: RES_b_IXIYd(6, indexRegister); return 23;
        case 0xBE: RES_b_IXIYd(7, indexRegister); return 23;
        case 0xC6: SET_b_IXIYd(0, indexRegister); return 23;
        case 0xCE: SET_b_IXIYd(1, indexRegister); return 23;
        case 0xD6: SET_b_IXIYd(2, indexRegister); return 23;
        case 0xDE: SET_b_IXIYd(3, indexRegister); return 23;
        case 0xE6: SET_b_IXIYd(4, indexRegister); return 23;
        case 0xEE: SET_b_IXIYd(5, indexRegister); return 23;
        case 0xF6: SET_b_IXIYd(6, indexRegister); return 23;
        case 0xFE: SET_b_IXIYd(7, indexRegister); return 23;
        default:
            std::cerr << "Unknown DDFDCB opcode: 0x" << std::hex << (int)nextOpcode << std::endl;
            return 4;
    }
}

void Z80::handleInterrupts() {
    // Handle non-maskable interrupt (NMI) if pending
    if (nmi_line) {
        handleNMI();
        nmi_line = false;
        cycles += 11;  // NMI handling overhead
        return;
    }

    // Handle maskable interrupt if IFF1 is set and an interrupt is pending
    if (registers.IFF1 && interrupt_pending) {
        halted = false;
        registers.IFF1 = false;

        switch (registers.interruptMode) {
            case 0:
                RST_p(0x38);  // Simulate RST 38h
                cycles += 13;
                break;
            case 1:
                RST_p(0x38);
                cycles += 13;
                break;
            case 2:
                if (checkForInterrupts()) {
                    uint8_t interruptVector = 0;
                    uint16_t address = (registers.I << 8) | interruptVector;
                    generateInterrupt(address);
                    cycles += 19;
                }
                break;
            default:
                std::cerr << "Invalid interrupt mode: " << registers.interruptMode << std::endl;
                cycles += 4;  // Error handling overhead
                break;
        }
        interrupt_pending = false;
    }
}

void Z80::handleNMI() {
    halted = false;
    registers.IFF2 = registers.IFF1;
    registers.IFF1 = false;
    pushWord(registers.PC);
    registers.PC = 0x0066;
    cycles += 11;
}

bool Z80::checkForInterrupts() {
    cycles += 4;  // Interrupt check overhead
    return interrupt_pending;
}

void Z80::generateInterrupt(uint16_t address) {
    pushWord(registers.PC);
    registers.PC = address;
    cycles += 11;  // Interrupt generation overhead
}


// --- Instruction Implementations ---
// temp
void Z80::ADD_IX_pp(uint16_t& pp) {
    uint32_t result = registers.IX + pp;
    bool halfCarry = ((registers.IX & 0xFFF) + (pp & 0xFFF)) > 0xFFF;
    bool carry = result > 0xFFFF;
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.setFlag(Z80Registers::Flag::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::Flag::Carry, carry);

    registers.IX = static_cast<uint16_t>(result);
    cycles += 15;
}

void Z80::LD_A_iDE() {
        registers.A = memory.readByte(registers.DE);
        cycles += 7;
    }

    void Z80::LD_A_inn() {
        uint16_t address = fetchWord();
        registers.A = memory.readByte(address);
        cycles += 13;
    }

    void Z80::EX_iSP_HL() {
        uint16_t temp = memory.readWord(registers.SP);
        memory.writeWord(registers.SP, registers.HL);
        registers.HL = temp;
        cycles += 19;
    }

    void Z80::LD_HL_inn() {
        uint16_t address = fetchWord();
        registers.HL = memory.readWord(address);
        cycles += 16;
    }

//// end of temp

void Z80::NOP() {
    // No operation
    cycles += 4;
}

void Z80::LD_BC_d16() {
    registers.BC = fetchWord();
    cycles += 10;
}

void Z80::LD_iBC_A() {
    memory.writeByte(registers.BC, registers.A);
    cycles += 7;
}

void Z80::INC_nn(uint16_t& nn) {
    nn++;
    cycles += 6;
}

void Z80::INC_r(uint8_t& r) {
    bool halfCarry = ((r & 0x0F) == 0x0F);
    r++;
    updateFlagsArithmetic(r, false, false, halfCarry, r == 0x80);
    cycles += 4;
}

void Z80::DEC_r(uint8_t& r) {
    bool halfCarry = ((r & 0x0F) == 0x00);
    r--;
    updateFlagsArithmetic(r, true, false, halfCarry, r == 0x7F);
    cycles += 4;
}

void Z80::LD_r_n(uint8_t& r, uint8_t value) {
    r = value;
    cycles += 7;
}

void Z80::RLCA() {
    uint8_t bit7 = (registers.A >> 7) & 1;
    registers.A = (registers.A << 1) | bit7;
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::EX_AF_AF() {
    registers.exchangeAF();
    cycles += 4;
}

void Z80::ADD_HL_ss(uint16_t& ss) {
    uint32_t result = registers.HL + ss;
    bool halfCarry = ((registers.HL & 0xFFF) + (ss & 0xFFF)) > 0xFFF;
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.setFlag(Z80Registers::Flag::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::Flag::Carry, result > 0xFFFF);
    registers.HL = (uint16_t)result;
    registers.updateFlagsRegister();
    cycles += 11;
}

void Z80::LD_A_iBC() {
    registers.A = memory.readByte(registers.BC);
    cycles += 7;
}

void Z80::DEC_nn(uint16_t& nn) {
    nn--;
    cycles += 6;
}

void Z80::RRCA() {
    uint8_t bit0 = registers.A & 1;
    registers.A = (registers.A >> 1) | (bit0 << 7);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::DJNZ_d8() {
    registers.B--;
    if (registers.B != 0) {
        int8_t offset = signExtend(fetchByte());
        registers.PC += offset;
        cycles += 13;
    } else {
        registers.PC++; // Skip the offset byte
        cycles += 8;
    }
}

void Z80::LD_DE_d16() {
    registers.DE = fetchWord();
    cycles += 10;
}

void Z80::LD_iDE_A() {
    memory.writeByte(registers.DE, registers.A);
    cycles += 7;
}

void Z80::RLA() {
    uint8_t bit7 = (registers.A >> 7) & 1;
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    registers.A = (registers.A << 1) | carry;
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::JR_d8() {
    int8_t offset = signExtend(fetchByte());
    registers.PC += offset;
    cycles += 12;
}

void Z80::RRA() {
    uint8_t bit0 = registers.A & 1;
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    registers.A = (registers.A >> 1) | (carry << 7);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::JR_NZ_d8() {
    if (!registers.getFlag(Z80Registers::Flag::Zero)) {
        JR_d8();
        cycles += 12;
    } else {
        registers.PC++; // Skip offset
        cycles += 7;
    }
}

void Z80::LD_HL_d16() {
    registers.HL = fetchWord();
    cycles += 10;
}

void Z80::LD_inn_HL() {
    uint16_t address = fetchWord();
    memory.writeWord(address, registers.HL);
    cycles += 16;
}

void Z80::JR_Z_d8() {
    if (registers.getFlag(Z80Registers::Flag::Zero)) {
        JR_d8();
        cycles += 12;
    } else {
        registers.PC++; // Skip offset
        cycles += 7;
    }
}

void Z80::DAA() {
    uint8_t correction = 0;
    bool carry = registers.getFlag(Z80Registers::Flag::Carry);
    bool halfCarry = registers.getFlag(Z80Registers::Flag::HalfCarry);
    bool subtract = registers.getFlag(Z80Registers::Flag::Subtract);

    if (halfCarry || (!subtract && ((registers.A & 0x0F) > 0x09))) {
        correction |= 0x06;
    }

    if (carry || (!subtract && (registers.A > 0x99))) {
        correction |= 0x60;
        carry = true;
    }

    registers.A += (subtract ? -correction : correction);

    registers.setFlag(Z80Registers::Flag::Carry, carry);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    updateFlags_SZP(registers.A);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::CPL() {
    registers.A = ~registers.A;
    registers.setFlag(Z80Registers::Flag::HalfCarry, true);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::JR_NC_d8() {
    if (!registers.getFlag(Z80Registers::Flag::Carry)) {
        JR_d8();
        cycles += 12;
    } else {
        registers.PC++; // Skip offset
        cycles += 7;
    }
}

void Z80::LD_SP_d16() {
    registers.SP = fetchWord();
    cycles += 10;
}

void Z80::LD_inn_A() {
    uint16_t address = fetchWord();
    memory.writeByte(address, registers.A);
    cycles += 13;
}

void Z80::JR_C_d8() {
    if (registers.getFlag(Z80Registers::Flag::Carry)) {
        JR_d8();
        cycles += 12;
    } else {
        registers.PC++; // Skip offset
        cycles += 7;
    }
}

void Z80::SCF() {
    registers.setFlag(Z80Registers::Flag::Carry, true);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::CCF() {
    bool carry = registers.getFlag(Z80Registers::Flag::Carry);
    registers.setFlag(Z80Registers::Flag::Carry, !carry);
    registers.setFlag(Z80Registers::Flag::HalfCarry, carry);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::LD_r_r(uint8_t& dst, uint8_t& src) {
    dst = src;
    cycles += 4;
}

void Z80::HALT() {
    halted = true;
    cycles += 4;
}

void Z80::ADD_A_r(uint8_t& r) {
    uint16_t result = registers.A + r;
    bool halfCarry = ((registers.A & 0x0F) + (r & 0x0F)) > 0x0F;
    bool overflow = ((registers.A ^ r) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 4;
}

void Z80::ADC_A_r(uint8_t& r) {
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint16_t result = registers.A + r + carry;
    bool halfCarry = ((registers.A & 0x0F) + (r & 0x0F) + carry) > 0x0F;
    bool overflow = ((registers.A ^ r) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 4;
}

void Z80::SUB_r(uint8_t& r) {
    uint8_t result = registers.A - r;
    bool halfCarry = (registers.A & 0x0F) < (r & 0x0F);
    bool overflow = ((registers.A ^ r) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 4;
}

void Z80::SBC_A_r(uint8_t& r) {
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t result = registers.A - r - carry;
    bool halfCarry = (registers.A & 0x0F) < ((r & 0x0F) + carry);
    bool overflow = ((registers.A ^ r) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 4;
}

void Z80::AND_r(uint8_t& r) {
    registers.A &= r;
    updateFlagsLogical(registers.A, false, true);
    cycles += 4;
}

void Z80::XOR_r(uint8_t& r) {
    registers.A ^= r;
    updateFlagsLogical(registers.A, false, false);
    cycles += 4;
}

void Z80::OR_r(uint8_t& r) {
    registers.A |= r;
    updateFlagsLogical(registers.A, false, false);
    cycles += 4;
}

void Z80::CP_r(uint8_t& r) {
    uint8_t result = registers.A - r;
    bool halfCarry = (registers.A & 0x0F) < (r & 0x0F);
    bool overflow = ((registers.A ^ r) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    updateFlagsArithmetic(result, true, false, halfCarry, overflow);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 4;
}

void Z80::RET_cc(bool condition) {
    if (condition) {
        registers.PC = popWord();
        cycles += 11;
    } else {
        cycles += 5;
    }
}

void Z80::POP_qq(uint16_t& qq) {
    qq = popWord();
    cycles += 10;
}

void Z80::JP_cc_nn(bool condition) {
    uint16_t address = fetchWord();
    if (condition) {
        registers.PC = address;
    }
    cycles += 10;
}

void Z80::JP_nn() {
    registers.PC = fetchWord();
    cycles += 10;
}

void Z80::CALL_cc_nn(bool condition) {
    uint16_t address = fetchWord();
    if (condition) {
        pushWord(registers.PC);
        registers.PC = address;
        cycles += 17;
    } else {
        cycles += 10;
    }
}

void Z80::PUSH_qq(uint16_t& qq) {
    pushWord(qq);
    cycles += 11;
}

void Z80::CALL_nn() {
    pushWord(registers.PC + 2);
    registers.PC = fetchWord();
    cycles += 17;
}

void Z80::ADD_A_n(uint8_t value) {
    uint16_t result = registers.A + value;
    bool halfCarry = ((registers.A & 0x0F) + (value & 0x0F)) > 0x0F;
    bool overflow = ((registers.A ^ value) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 7;
}

void Z80::RST_p(uint8_t p) {
    pushWord(registers.PC);
    registers.PC = p;
    cycles += 11;
}

void Z80::ADC_A_n(uint8_t value) {
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint16_t result = registers.A + value + carry;
    bool halfCarry = ((registers.A & 0x0F) + (value & 0x0F) + carry) > 0x0F;
    bool overflow = ((registers.A ^ value) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 7;
}

void Z80::RET() {
    registers.PC = popWord();
    cycles += 10;
}

void Z80::EXX() {
    registers.exchangeMainRegisters();
    cycles += 4;
}

void Z80::IN_A_in() {
    uint8_t port = fetchByte();
    // In a real Z80, IN affects flags, but we'll skip that for simplicity here
    // TODO: Implement IN instruction
    cycles += 11;
}

void Z80::OUT_in_A() {
    uint8_t port = fetchByte();
    // TODO: Implement OUT instruction
    cycles += 11;
}

void Z80::SUB_n(uint8_t value) {
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 7;
}

void Z80::SBC_A_n(uint8_t value) {
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t result = registers.A - value - carry;
    bool halfCarry = (registers.A & 0x0F) < ((value & 0x0F) + carry);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 7;
}

void Z80::EX_DE_HL() {
    std::swap(registers.DE, registers.HL);
    cycles += 4;
}

void Z80::JP_iHL() {
    registers.PC = registers.HL;
    cycles += 4;
}

void Z80::LD_iSP_HL() {
    memory.writeWord(registers.SP, registers.HL);
    cycles += 6;
}

void Z80::DI() {
    registers.IFF1 = registers.IFF2 = false;
    cycles += 4;
}

void Z80::AND_n(uint8_t value) {
    registers.A &= value;
    updateFlagsLogical(registers.A, false, true);
    cycles += 7;
}

void Z80::EI() {
    registers.IFF1 = registers.IFF2 = true;
    cycles += 4;
}


void Z80::XOR_n(uint8_t value) {
    registers.A ^= value;
    updateFlagsLogical(registers.A, false, false);
    cycles += 7;
}

void Z80::OR_n(uint8_t value) {
    registers.A |= value;
    updateFlagsLogical(registers.A, false, false);
    cycles += 7;
}

void Z80::CP_n(uint8_t value) {
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    updateFlagsArithmetic(result, true, false, halfCarry, overflow);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 7;
}

void Z80::LD_r_iHL(uint8_t& r) {
    r = memory.readByte(registers.HL);
    cycles += 7;
}

void Z80::LD_iHL_r(uint8_t& r) {
    memory.writeByte(registers.HL, r);
    cycles += 7;
}

void Z80::LD_iHL_n(uint8_t n) {
    memory.writeByte(registers.HL, n);
    cycles += 10;
}

void Z80::INC_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    bool halfCarry = ((value & 0x0F) == 0x0F);
    value++;
    memory.writeByte(registers.HL, value);
    updateFlagsArithmetic(value, false, false, halfCarry, value == 0x80);
    cycles += 11;
}

void Z80::DEC_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    bool halfCarry = ((value & 0x0F) == 0x00);
    value--;
    memory.writeByte(registers.HL, value);
    updateFlagsArithmetic(value, true, false, halfCarry, value == 0x7F);
    cycles += 11;
}

void Z80::ADD_A_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    uint16_t result = registers.A + value;
    bool halfCarry = ((registers.A & 0x0F) + (value & 0x0F)) > 0x0F;
    bool overflow = ((registers.A ^ value) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 7;
}

void Z80::ADC_A_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint16_t result = registers.A + value + carry;
    bool halfCarry = ((registers.A & 0x0F) + (value & 0x0F) + carry) > 0x0F;
    bool overflow = ((registers.A ^ value) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 7;
}

void Z80::SUB_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 7;
}

void Z80::SBC_A_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t result = registers.A - value - carry;
    bool halfCarry = (registers.A & 0x0F) < ((value & 0x0F) + carry);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 7;
}

void Z80::AND_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    registers.A &= value;
    updateFlagsLogical(registers.A, false, true);
    cycles += 7;
}

void Z80::XOR_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    registers.A ^= value;
    updateFlagsLogical(registers.A, false, false);
    cycles += 7;
}

void Z80::OR_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    registers.A |= value;
    updateFlagsLogical(registers.A, false, false);
    cycles += 7;
}

void Z80::CP_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    updateFlagsArithmetic(result, true, false, halfCarry, overflow);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 7;
}

// 0xCB prefixed instructions
void Z80::BIT_b_r(uint8_t bit, uint8_t& reg) {
    checkBit(reg, bit);
    cycles += 8;
}

void Z80::RES_b_r(uint8_t bit, uint8_t& reg) {
    reg &= ~(1 << bit);
    cycles += 8;
}

void Z80::SET_b_r(uint8_t bit, uint8_t& reg) {
    reg |= (1 << bit);
    cycles += 8;
}

void Z80::RLC_r(uint8_t& reg) {
    uint8_t bit7 = (reg >> 7) & 1;
    reg = (reg << 1) | bit7;
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::RRC_r(uint8_t& reg) {
    uint8_t bit0 = reg & 1;
    reg = (reg >> 1) | (bit0 << 7);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::RL_r(uint8_t& reg) {
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t bit7 = (reg >> 7) & 1;
    reg = (reg << 1) | carry;
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::RR_r(uint8_t& reg) {
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t bit0 = reg & 1;
    reg = (reg >> 1) | (carry << 7);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::SLA_r(uint8_t& reg) {
    uint8_t bit7 = (reg >> 7) & 1;
    reg = reg << 1;
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::SRA_r(uint8_t& reg) {
    uint8_t bit0 = reg & 1;
    uint8_t bit7 = reg & 0x80;
    reg = (reg >> 1) | bit7;
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::SLL_r(uint8_t& reg) {
    uint8_t bit7 = (reg >> 7) & 1;
    reg = (reg << 1) | 1;
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::SRL_r(uint8_t& reg) {
    uint8_t bit0 = reg & 1;
    reg = reg >> 1;
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(reg);
    registers.updateFlagsRegister();
    cycles += 8;
}

void Z80::RLC_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    RLC_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::RRC_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    RRC_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::RL_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    RL_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::RR_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    RR_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::SLA_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    SLA_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::SRA_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    SRA_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::SLL_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    SLL_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::SRL_iHL() {
    uint8_t value = memory.readByte(registers.HL);
    SRL_r(value);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::BIT_b_iHL(uint8_t bit) {
    uint8_t value = memory.readByte(registers.HL);
    checkBit(value, bit);
    cycles += 12;
}

void Z80::RES_b_iHL(uint8_t bit) {
    uint8_t value = memory.readByte(registers.HL);
    value &= ~(1 << bit);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

void Z80::SET_b_iHL(uint8_t bit) {
    uint8_t value = memory.readByte(registers.HL);
    value |= (1 << bit);
    memory.writeByte(registers.HL, value);
    cycles += 15;
}

// ED prefixed instructions
void Z80::IN_r_iC(uint8_t& r) {
    // TODO: Implement IN instruction
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(r);
    registers.updateFlagsRegister();
    cycles += 12;
}

void Z80::OUT_iC_r(uint8_t& r) {
    // TODO: Implement OUT instruction
    cycles += 12;
}

void Z80::SBC_HL_ss(uint16_t& ss) {
    uint16_t result = registers.HL - ss - registers.getFlag(Z80Registers::Flag::Carry);
    bool halfCarry = (registers.HL & 0xFFF) < ((ss & 0xFFF) + registers.getFlag(Z80Registers::Flag::Carry));
    bool overflow = ((registers.HL ^ ss) & 0x8000) != 0 && ((registers.HL ^ result) & 0x8000) != 0;
    registers.HL = result;
    updateFlagsArithmetic(static_cast<uint8_t>(result >> 8), true, false, halfCarry, overflow);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.setFlag(Z80Registers::Flag::Carry, (result & 0x10000) != 0);
    registers.updateFlagsRegister();
    cycles += 15;
}

void Z80::LD_inn_dd(uint16_t& dd) {
    uint16_t address = fetchWord();
    memory.writeWord(address, dd);
    cycles += 20;
}

void Z80::NEG() {
    uint8_t result = 0 - registers.A;
    bool halfCarry = (registers.A & 0x0F) != 0;
    bool overflow = registers.A == 0x80;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 8;
}

void Z80::RETN() {
    registers.PC = popWord();
    registers.IFF1 = registers.IFF2;
    cycles += 14;
}

void Z80::IM_x(uint8_t x) {
    if (x == 0 || x == 1 || x == 2) {
        registers.interruptMode = x;
    } else {
        std::cerr << "Invalid interrupt mode: " << (int)x << std::endl;
    }
    cycles += 8;
}

void Z80::LD_I_A() {
    registers.I = registers.A;
    cycles += 9;
}

void Z80::IN_f_iC() {
    // TODO: Implement IN instruction
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.updateFlagsRegister();
    cycles += 12;
}

void Z80::OUT_iC_0() {
    // TODO: Implement OUT instruction
    cycles += 12;
}

void Z80::ADC_HL_ss(uint16_t& ss) {
    uint32_t result = registers.HL + ss + registers.getFlag(Z80Registers::Flag::Carry);
    bool halfCarry = ((registers.HL & 0xFFF) + (ss & 0xFFF) + registers.getFlag(Z80Registers::Flag::Carry)) > 0xFFF;
    bool overflow = ((registers.HL ^ ss) & 0x8000) == 0 && ((registers.HL ^ result) & 0x8000) != 0;
    registers.HL = (uint16_t)result;
    updateFlagsArithmetic(static_cast<uint8_t>(result >> 8), false, result > 0xFFFF, halfCarry, overflow);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 15;
}

void Z80::LD_dd_inn(uint16_t& dd) {
    uint16_t address = fetchWord();
    dd = memory.readWord(address);
    cycles += 20;
}

void Z80::RETI() {
    registers.PC = popWord();
    registers.IFF1 = registers.IFF2;
    cycles += 14;
}

void Z80::LD_R_A() {
    registers.R = registers.A;
    cycles += 9;
}

void Z80::RRD() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t lowNibbleA = registers.A & 0x0F;
    registers.A = (registers.A & 0xF0) | (value & 0x0F);
    value = (value >> 4) | (lowNibbleA << 4);
    memory.writeByte(registers.HL, value);
    updateFlags_SZP(registers.A);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 18;
}

void Z80::RLD() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t highNibbleA = (registers.A >> 4) & 0x0F;
    registers.A = (registers.A & 0xF0) | ((value >> 4) & 0x0F);
    value = (value << 4) | highNibbleA;
    memory.writeByte(registers.HL, value);
    updateFlags_SZP(registers.A);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 18;
}

void Z80::LDI() {
    uint8_t value = memory.readByte(registers.HL);
    memory.writeByte(registers.DE, value);
    registers.HL++;
    registers.DE++;
    registers.BC--;
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::ParityOverflow, registers.BC != 0);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::CPI() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    registers.HL++;
    registers.BC--;
    updateFlags_SZP(result);
    registers.setFlag(Z80Registers::Flag::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::Flag::ParityOverflow, registers.BC != 0);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::INI() {
    // TODO: Implement IN instruction
    // uint8_t value = io.read(registers.C);
    // memory.writeByte(registers.HL, value);
    registers.B--;
    registers.HL++;
    registers.setFlag(Z80Registers::Flag::Zero, registers.B == 0);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::OUTI() {
    // TODO: Implement OUT instruction
    uint8_t value = memory.readByte(registers.HL);
    // io.write(registers.C, value);
    registers.B--;
    registers.HL++;
    registers.setFlag(Z80Registers::Flag::Zero, registers.B == 0);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::LDD() {
    uint8_t value = memory.readByte(registers.HL);
    memory.writeByte(registers.DE, value);
    registers.HL--;
    registers.DE--;
    registers.BC--;
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::ParityOverflow, registers.BC != 0);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::CPD() {
    uint8_t value = memory.readByte(registers.HL);
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    registers.HL--;
    registers.BC--;
    updateFlags_SZP(result);
    registers.setFlag(Z80Registers::Flag::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::Flag::ParityOverflow, registers.BC != 0);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::IND() {
    // TODO: Implement IN instruction
    // uint8_t value = io.read(registers.C);
    // memory.writeByte(registers.HL, value);
    registers.B--;
    registers.HL--;
    registers.setFlag(Z80Registers::Flag::Zero, registers.B == 0);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::OUTD() {
    // TODO: Implement OUT instruction
    uint8_t value = memory.readByte(registers.HL);
    // io.write(registers.C, value);
    registers.B--;
    registers.HL--;
    registers.setFlag(Z80Registers::Flag::Zero, registers.B == 0);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 16;
}

void Z80::LDIR() {
    LDI();
    if (registers.BC != 0) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::CPIR() {
    CPI();
    if (registers.BC != 0 && !registers.getFlag(Z80Registers::Flag::Zero)) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::INIR() {
    INI();
    if (registers.B != 0) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::OTIR() {
    OUTI();
    if (registers.B != 0) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::LDDR() {
    LDD();
    if (registers.BC != 0) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::CPDR() {
    CPD();
    if (registers.BC != 0 && !registers.getFlag(Z80Registers::Flag::Zero)) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::INDR() {
    IND();
    if (registers.B != 0) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::OTDR() {
    OUTD();
    if (registers.B != 0) {
        registers.PC -= 2;
        cycles += 21;  // 5 extra cycles if operation repeats
    } else {
        cycles += 16;
    }
}

void Z80::LD_A_I() {
    registers.A = registers.I;
    registers.setFlag(Z80Registers::Flag::Sign, (registers.A & 0x80) != 0);
    registers.setFlag(Z80Registers::Flag::Zero, registers.A == 0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::ParityOverflow, registers.IFF2);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 9;
}

void Z80::LD_A_R() {
    registers.A = registers.R;
    registers.setFlag(Z80Registers::Flag::Sign, (registers.A & 0x80) != 0);
    registers.setFlag(Z80Registers::Flag::Zero, registers.A == 0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::ParityOverflow, registers.IFF2);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.updateFlagsRegister();
    cycles += 9;
}

// DD/FD prefixed instructions (IX/IY instructions)
void Z80::LD_IXIY_nn(uint16_t& indexReg) {
    indexReg = fetchWord();
    cycles += 14;
}

void Z80::LD_inn_IXIY(uint16_t& indexReg) {
    uint16_t address = fetchWord();
    memory.writeWord(address, indexReg);
    cycles += 20;
}

void Z80::INC_IXIY(uint16_t& indexReg) {
    indexReg++;
    cycles += 10;
}

void Z80::INC_IXIYH(uint8_t& reg) {
    bool halfCarry = ((reg & 0x0F) == 0x0F);
    reg++;
    updateFlagsArithmetic(reg, false, false, halfCarry, reg == 0x80);
    cycles += 8;
}

void Z80::DEC_IXIYH(uint8_t& reg) {
    bool halfCarry = ((reg & 0x0F) == 0x00);
    reg--;
    updateFlagsArithmetic(reg, true, false, halfCarry, reg == 0x7F);
    cycles += 8;
}

void Z80::LD_IXIYH_n(uint8_t& reg, uint8_t value) {
    reg = value;
    cycles += 11;
}

void Z80::ADD_IXIY_ss(uint16_t& indexReg, uint16_t& otherReg) {
    uint32_t result = indexReg + otherReg;
    bool halfCarry = ((indexReg & 0xFFF) + (otherReg & 0xFFF)) > 0xFFF;
    indexReg = (uint16_t)result;
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    registers.setFlag(Z80Registers::Flag::HalfCarry, halfCarry);
    registers.setFlag(Z80Registers::Flag::Carry, result > 0xFFFF);
    registers.updateFlagsRegister();
    cycles += 15;
}

void Z80::LD_IXIY_inn(uint16_t& indexReg) {
    uint16_t address = fetchWord();
    indexReg = memory.readWord(address);
    cycles += 20;
}

void Z80::DEC_IXIY(uint16_t& indexReg) {
    indexReg--;
    cycles += 10;
}

void Z80::INC_IXIYL(uint8_t& reg) {
    bool halfCarry = ((reg & 0x0F) == 0x0F);
    reg++;
    updateFlagsArithmetic(reg, false, false, halfCarry, reg == 0x80);
    cycles += 8;
}

void Z80::DEC_IXIYL(uint8_t& reg) {
    bool halfCarry = ((reg & 0x0F) == 0x00);
    reg--;
    updateFlagsArithmetic(reg, true, false, halfCarry, reg == 0x7F);
    cycles += 8;
}

void Z80::LD_IXIYL_n(uint8_t& reg, uint8_t value) {
    reg = value;
    cycles += 11;
}

void Z80::LD_r_IXIYd(uint8_t& reg, uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    reg = memory.readByte(address);
    cycles += 19;
}

void Z80::LD_IXIYd_r(uint16_t& indexReg, uint8_t& reg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    memory.writeByte(address, reg);
    cycles += 19;
}

void Z80::LD_IXIYd_n(uint16_t& indexReg, uint8_t value) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    memory.writeByte(address, value);
    cycles += 19;
}

void Z80::ADD_A_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint16_t result = registers.A + value;
    bool halfCarry = ((registers.A & 0x0F) + (value & 0x0F)) > 0x0F;
    bool overflow = ((registers.A ^ value) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 19;
}

void Z80::ADC_A_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint16_t result = registers.A + value + carry;
    bool halfCarry = ((registers.A & 0x0F) + (value & 0x0F) + carry) > 0x0F;
    bool overflow = ((registers.A ^ value) & 0x80) == 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = (uint8_t)result;
    updateFlagsArithmetic(registers.A, false, result > 0xFF, halfCarry, overflow);
    cycles += 19;
}

void Z80::SUB_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 19;
}

void Z80::SBC_A_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t result = registers.A - value - carry;
    bool halfCarry = (registers.A & 0x0F) < ((value & 0x0F) + carry);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    registers.A = result;
    updateFlagsArithmetic(registers.A, true, false, halfCarry, overflow);
    cycles += 19;
}

void Z80::AND_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    registers.A &= value;
    updateFlagsLogical(registers.A, false, true);
    cycles += 19;
}

void Z80::XOR_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    registers.A ^= value;
    updateFlagsLogical(registers.A, false, false);
    cycles += 19;
}

void Z80::OR_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    registers.A |= value;
    updateFlagsLogical(registers.A, false, false);
    cycles += 19;
}

void Z80::CP_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t result = registers.A - value;
    bool halfCarry = (registers.A & 0x0F) < (value & 0x0F);
    bool overflow = ((registers.A ^ value) & 0x80) != 0 && ((registers.A ^ result) & 0x80) != 0;
    updateFlagsArithmetic(result, true, false, halfCarry, overflow);
    registers.setFlag(Z80Registers::Flag::Subtract, true);
    registers.updateFlagsRegister();
    cycles += 19;
}

void Z80::INC_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    bool halfCarry = ((value & 0x0F) == 0x0F);
    value++;
    memory.writeByte(address, value);
    updateFlagsArithmetic(value, false, false, halfCarry, value == 0x80);
    cycles += 23;
}

void Z80::DEC_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    bool halfCarry = ((value & 0x0F) == 0x00);
    value--;
    memory.writeByte(address, value);
    updateFlagsArithmetic(value, true, false, halfCarry, value == 0x7F);
    cycles += 23;
}

void Z80::JP_iIXIY(uint16_t& indexReg) {
    registers.PC = indexReg;
    cycles += 8;
}

void Z80::LD_SP_IXIY(uint16_t& indexReg) {
    registers.SP = indexReg;
    cycles += 10;
}

void Z80::EX_iSP_IXIY(uint16_t& indexReg) {
    uint16_t temp = memory.readWord(registers.SP);
    memory.writeWord(registers.SP, indexReg);
    indexReg = temp;
    cycles += 23;
}

// CB prefixed IX/IY instructions
void Z80::RLC_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t bit7 = (value >> 7) & 1;
    value = (value << 1) | bit7;
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::RRC_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t bit0 = value & 1;
    value = (value >> 1) | (bit0 << 7);
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::RL_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t bit7 = (value >> 7) & 1;
    value = (value << 1) | carry;
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::RR_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t carry = registers.getFlag(Z80Registers::Flag::Carry);
    uint8_t bit0 = value & 1;
    value = (value >> 1) | (carry << 7);
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::SLA_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t bit7 = (value >> 7) & 1;
    value <<= 1;
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::SRA_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t bit0 = value & 1;
    uint8_t bit7 = value & 0x80;
    value = (value >> 1) | bit7;
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::SLL_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t bit7 = (value >> 7) & 1;
    value = (value << 1) | 1;
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit7);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::SRL_IXIYd(uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    uint8_t bit0 = value & 1;
    value >>= 1;
    memory.writeByte(address, value);
    registers.setFlag(Z80Registers::Flag::Carry, bit0);
    registers.setFlag(Z80Registers::Flag::HalfCarry, false);
    registers.setFlag(Z80Registers::Flag::Subtract, false);
    updateFlags_SZP(value);
    registers.updateFlagsRegister();
    cycles += 23;
}

void Z80::BIT_b_IXIYd(uint8_t bit, uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    checkBit(value, bit);
    cycles += 20;
}

void Z80::RES_b_IXIYd(uint8_t bit, uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    value &= ~(1 << bit);
    memory.writeByte(address, value);
    cycles += 23;
}

void Z80::SET_b_IXIYd(uint8_t bit, uint16_t& indexReg) {
    int8_t displacement = signExtend(fetchByte());
    uint16_t address = indexReg + displacement;
    uint8_t value = memory.readByte(address);
    value |= (1 << bit);
    memory.writeByte(address, value);
    cycles += 23;
}
