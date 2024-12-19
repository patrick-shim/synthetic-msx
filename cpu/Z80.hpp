
#ifndef Z80_HPP
#define Z80_HPP

#include <cstdint>
#include <vector>
#include "Z80Registers.hpp"
#include "../memory/Memory.hpp" // Adjust path as needed

class Z80 {
public:
    Z80(Memory& memory);

    void reset();
    void executeInstruction();
    void loadProgram(const std::vector<uint8_t>& program, uint16_t startAddress);
    uint64_t getCycleCount() const { return cycles; } // Add a getter for cycle count
    // Optional: For debugging and inspection
    const Z80Registers& getRegisters() const { return registers; }
    void setInterruptLine(bool high); // Add a function to set the interrupt line state
    bool nmi_line; // Add this variable to the class. Non-maskable interrupt line

private:
    Z80Registers registers;
    Memory& memory;
    bool halted;
    uint64_t cycles;

    // --- Instruction Fetching and Decoding ---
    uint8_t fetchOpcode();
    int decodeAndExecuteMainInstruction(uint8_t opcode);
    int decodeAndExecuteCBInstruction(uint8_t opcode);
    int decodeAndExecuteEDInstruction(uint8_t opcode);
    int decodeAndExecuteDDInstruction(uint8_t opcode); // Instructions prefixed with 0xDD (IX-related)
    int decodeAndExecuteFDInstruction(uint8_t opcode); // Instructions prefixed with 0xFD (IY-related)
    int decodeAndExecuteDDFDInstruction(uint8_t opcode, uint16_t& indexRegister); // Instructions prefixed with 0xDD or 0xFD
    int decodeAndExecuteDDFDCBInstruction(uint8_t opcode, uint16_t& indexRegister); // Instructions prefixed with 0xDD or 0xFD

    // --- Helper functions ---
    uint8_t fetchByte();
    uint16_t fetchWord();
    int8_t signExtend(uint8_t value);
    void pushByte(uint8_t value);
    uint8_t popByte();
    void pushWord(uint16_t value);
    uint16_t popWord();
    void updateFlags(uint8_t result, bool isSubtraction = false, bool is16Bit = false);
    void updateFlags_SZP(uint8_t value);
    void updateFlagsArithmetic(uint8_t result, bool isSubtraction = false, bool carry = false, bool halfCarry = false, bool overflow = false);
    void updateFlagsLogical(uint8_t result, bool carry = false, bool halfCarry = false);
    void calculateParity(uint8_t value);
    void checkBit(uint8_t value, uint8_t bit);

    // --- Interrupt Handling ---
    void handleInterrupts();
    bool checkForInterrupts();
    void generateInterrupt(uint16_t address);
    bool interrupt_pending; // Add a flag to indicate if an interrupt is pending
    void handleNMI(); // Add this method to the class


    // temp
    void LD_A_iDE();
    void LD_HL_inn();
    void LD_A_inn();
    void EX_iSP_HL();
    void LD_A_I();
    void LD_A_R();
    void ADD_IX_pp(uint16_t& pp);
    
    // --- Instruction Implementations ---
    void NOP();                                     // 0x00
    void LD_BC_d16();                               // 0x01
    void LD_iBC_A();                                // 0x02
    void INC_nn(uint16_t& nn);                      // 0x03, 0x13, 0x23, 0x33
    void INC_r(uint8_t& r);                         // 0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C
    void DEC_r(uint8_t& r);                         // 0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D
    void LD_r_n(uint8_t& r, uint8_t value);         // 0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E
    void RLCA();                                    // 0x07
    void EX_AF_AF();                                // 0x08
    void ADD_HL_ss(uint16_t& ss);                   // 0x09, 0x19, 0x29, 0x39
    void LD_A_iBC();                                // 0x0A
    void DEC_nn(uint16_t& nn);                      // 0x0B, 0x1B, 0x2B, 0x3B
    void RRCA();                                    // 0x0F
    void DJNZ_d8();                                 // 0x10
    void LD_DE_d16();                               // 0x11
    void LD_iDE_A();                                // 0x12
    void RLA();                                     // 0x17
    void JR_d8();                                   // 0x18
    void RRA();                                     // 0x1F
    void JR_NZ_d8();                                // 0x20
    void LD_HL_d16();                               // 0x21
    void LD_inn_HL();                               // 0x22
    void JR_Z_d8();                                 // 0x28
    void DAA();                                     // 0x27
    void CPL();                                     // 0x2F
    void JR_NC_d8();                                // 0x30
    void LD_SP_d16();                               // 0x31
    void LD_inn_A();                                // 0x32
    void JR_C_d8();                                 // 0x38
    void SCF();                                     // 0x37
    void CCF();                                     // 0x3F
    void LD_r_r(uint8_t& dst, uint8_t& src);        // 0x40-0x7F (except 0x76)
    void HALT();                                    // 0x76
    void ADD_A_r(uint8_t& r);                       // 0x80-0x87
    void ADC_A_r(uint8_t& r);                       // 0x88-0x8F
    void SUB_r(uint8_t& r);                         // 0x90-0x97
    void SBC_A_r(uint8_t& r);                       // 0x98-0x9F
    void AND_r(uint8_t& r);                         // 0xA0-0xA7
    void XOR_r(uint8_t& r);                         // 0xA8-0xAF
    void OR_r(uint8_t& r);                          // 0xB0-0xB7
    void CP_r(uint8_t& r);                          // 0xB8-0xBF
    void RET_cc(bool condition);                    // 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8
    void POP_qq(uint16_t& qq);                      // 0xC1, 0xD1, 0xE1, 0xF1
    void JP_cc_nn(bool condition);                  // 0xC2, 0xCA, 0xD2, 0xDA, 0xE2, 0xEA, 0xF2, 0xFA
    void JP_nn();                                   // 0xC3
    void OUT_in_A();                                // 0xD3
    void CALL_cc_nn(bool condition);                // 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC
    void PUSH_qq(uint16_t& qq);                     // 0xC5, 0xD5, 0xE5, 0xF5
    void CALL_nn();                                 // 0xCD
    void ADD_A_n(uint8_t value);                    // 0xC6
    void RST_p(uint8_t p);                          // 0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF
    void ADC_A_n(uint8_t value);                    // 0xCE
    void RET();                                     // 0xC9
    void EXX();                                     // 0xD9
    void IN_A_in();                                 // 0xDB
    void SUB_n(uint8_t value);                      // 0xD6
    void SBC_A_n(uint8_t value);                    // 0xDE
    void EX_DE_HL();                                // 0xEB
    void JP_iHL();                                  // 0xE9
    void LD_iSP_HL();                               // 0xF9
    void DI();                                      // 0xF3
    void AND_n(uint8_t value);                       // 0xE6
    void EI();                                      // 0xFB
    void XOR_n(uint8_t value);                       // 0xEE
    void OR_n(uint8_t value);                        // 0xF6
    void CP_n(uint8_t value);                        // 0xFE
    void LD_r_iHL(uint8_t& r);                      // 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x7E
    void LD_iHL_r(uint8_t& r);                      // 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77
    void LD_iHL_n(uint8_t n);                       // 0x36
    void INC_iHL();                                 // 0x34
    void DEC_iHL();                                 // 0x35
    void ADD_A_iHL();                               // 0x86
    void ADC_A_iHL();                               // 0x8E
    void SUB_iHL();                                 // 0x96
    void SBC_A_iHL();                               // 0x9E
    void AND_iHL();                                 // 0xA6
    void XOR_iHL();                                 // 0xAE
    void OR_iHL();                                  // 0xB6
    void CP_iHL();                                  // 0xBE

    // 0xCB prefixed instructions
    void BIT_b_r(uint8_t bit, uint8_t& reg);         // 0x40 - 0x7F (various)
    void RES_b_r(uint8_t bit, uint8_t& reg);         // 0x80 - 0xBF (various)
    void SET_b_r(uint8_t bit, uint8_t& reg);         // 0xC0 - 0xFF (various)
    void RLC_r(uint8_t& reg);                       // 0x00 - 0x07
    void RRC_r(uint8_t& reg);                       // 0x08 - 0x0F
    void RL_r(uint8_t& reg);                        // 0x10 - 0x17
    void RR_r(uint8_t& reg);                        // 0x18 - 0x1F
    void SLA_r(uint8_t& reg);                       // 0x20 - 0x27
    void SRA_r(uint8_t& reg);                       // 0x28 - 0x2F
    void SLL_r(uint8_t& reg);                       // 0x30 - 0x37
    void SRL_r(uint8_t& reg);                       // 0x38 - 0x3F
    void RLC_iHL();                                 // 0x06
    void RRC_iHL();                                 // 0x0E
    void RL_iHL();                                  // 0x16
    void RR_iHL();                                  // 0x1E
    void SLA_iHL();                                 // 0x26
    void SRA_iHL();                                 // 0x2E
    void SLL_iHL();                                 // 0x36
    void SRL_iHL();                                 // 0x3E
    void BIT_b_iHL(uint8_t bit);                     // 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E
    void RES_b_iHL(uint8_t bit);                     // 0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE
    void SET_b_iHL(uint8_t bit);                     // 0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE

    // 0xED prefixed instructions
    void IN_r_iC(uint8_t& r);                       // 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x78
    void OUT_iC_r(uint8_t& r);                      // 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x79
    void SBC_HL_ss(uint16_t& ss);                  // 0x42, 0x52, 0x62, 0x72
    void LD_inn_dd(uint16_t& dd);                   // 0x43, 0x53, 0x63, 0x73
    void NEG();                                     // 0x44
    void RETN();                                    // 0x45
    void IM_x(uint8_t x);                           // 0x46, 0x56, 0x5E, 0x4E, 0x66, 0x6E, 0x76, 0x7E
    void LD_I_A();                                  // 0x47
    void IN_f_iC();                                 // 0x70
    void OUT_iC_0();                                // 0x71
    void ADC_HL_ss(uint16_t& ss);                  // 0x4A, 0x5A, 0x6A, 0x7A
    void LD_dd_inn(uint16_t& dd);                   // 0x4B, 0x5B, 0x6B, 0x7B
    void RETI();                                    // 0x4D
    void LD_R_A();                                  // 0x4F
    void RRD();                                     // 0x67
    void RLD();                                     // 0x6F
    void LDI();                                     // 0xA0
    void CPI();                                     // 0xA1
    void INI();                                     // 0xA2
    void OUTI();                                    // 0xA3
    void LDD();                                     // 0xA8
    void CPD();                                     // 0xA9
    void IND();                                     // 0xAA
    void OUTD();                                    // 0xAB
    void LDIR();                                    // 0xB0
    void CPIR();                                    // 0xB1
    void INIR();                                    // 0xB2
    void OTIR();                                    // 0xB3
    void LDDR();                                    // 0xB8
    void CPDR();                                    // 0xB9
    void INDR();                                    // 0xBA
    void OTDR();                                    // 0xBB

    // 0xDD and 0xFD prefixed instructions (IX and IY instructions)
    void LD_IXIY_nn(uint16_t& indexReg);             // 0x21
    void LD_inn_IXIY(uint16_t& indexReg);            // 0x22
    void INC_IXIY(uint16_t& indexReg);               // 0x23
    void INC_IXIYH(uint8_t& reg);                   // 0x24
    void DEC_IXIYH(uint8_t& reg);                   // 0x25
    void LD_IXIYH_n(uint8_t& reg, uint8_t value);   // 0x26
    void ADD_IXIY_ss(uint16_t& indexReg, uint16_t& otherReg); // 0x09, 0x19, 0x29, 0x39
    void LD_IXIY_inn(uint16_t& indexReg);            // 0x2A
    void DEC_IXIY(uint16_t& indexReg);               // 0x2B
    void INC_IXIYL(uint8_t& reg);                   // 0x2C
    void DEC_IXIYL(uint8_t& reg);                   // 0x2D
    void LD_IXIYL_n(uint8_t& reg, uint8_t value);   // 0x2E
    void LD_r_IXIYd(uint8_t& reg, uint16_t& indexReg); // 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x7E
    void LD_IXIYd_r(uint16_t& indexReg, uint8_t& reg); // 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77
    void LD_IXIYd_n(uint16_t& indexReg, uint8_t value); // 0x36
    void ADD_A_IXIYd(uint16_t& indexReg);           // 0x86
    void ADC_A_IXIYd(uint16_t& indexReg);           // 0x8E
    void SUB_IXIYd(uint16_t& indexReg);             // 0x96
    void SBC_A_IXIYd(uint16_t& indexReg);           // 0x9E
    void AND_IXIYd(uint16_t& indexReg);             // 0xA6
    void XOR_IXIYd(uint16_t& indexReg);             // 0xAE
    void OR_IXIYd(uint16_t& indexReg);              // 0xB6
    void CP_IXIYd(uint16_t& indexReg);              // 0xBE
    void INC_IXIYd(uint16_t& indexReg);             // 0x34
    void DEC_IXIYd(uint16_t& indexReg);             // 0x35
    void JP_iIXIY(uint16_t& indexReg);              // 0xE9
    void LD_SP_IXIY(uint16_t& indexReg);            // 0xF9
    void EX_iSP_IXIY(uint16_t& indexReg);           // 0xE3

    // 0xDD/FD CB Prefixed Extended Instructions
    void RLC_IXIYd(uint16_t& indexReg);
    void RRC_IXIYd(uint16_t& indexReg);
    void RL_IXIYd(uint16_t& indexReg);
    void RR_IXIYd(uint16_t& indexReg);
    void SLA_IXIYd(uint16_t& indexReg);
    void SRA_IXIYd(uint16_t& indexReg);
    void SLL_IXIYd(uint16_t& indexReg);
    void SRL_IXIYd(uint16_t& indexReg);
    void BIT_b_IXIYd(uint8_t bit, uint16_t& indexReg);
    void RES_b_IXIYd(uint8_t bit, uint16_t& indexReg);
    void SET_b_IXIYd(uint8_t bit, uint16_t& indexReg);
};

#endif // Z80_HPP
