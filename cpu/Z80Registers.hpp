#ifndef Z80_REGISTERS_HPP
#define Z80_REGISTERS_HPP

#include <cstdint>

class Z80Registers {
public:
    // Enum for flag names (now outside the class)
    enum Flag {
        Carry,
        Subtract,
        ParityOverflow,
        HalfCarry,
        Zero,
        Sign
    };

    // 8-bit registers with 16-bit access using unions
    union {
        struct {
            uint8_t F;    // Flags
            uint8_t A;    // Accumulator
        };
        uint16_t AF;         // Combined AF register
    };

    union {
        struct {
            uint8_t C;
            uint8_t B;
        };
        uint16_t BC;
    };

    union {
        struct {
            uint8_t E;
            uint8_t D;
        };
        uint16_t DE;
    };

    union {
        struct {
            uint8_t L;
            uint8_t H;
        };
        uint16_t HL;
    };

    // Alternate register set (AF', BC', DE', HL')
    union {
        struct {
            uint8_t F_;
            uint8_t A_;
        };
        uint16_t AF_;
    };

    union {
        struct {
            uint8_t C_;
            uint8_t B_;
        };
        uint16_t BC_;
    };

    union {
        struct {
            uint8_t E_;
            uint8_t D_;
        };
        uint16_t DE_;
    };

    union {
        struct {
            uint8_t L_;
            uint8_t H_;
        };
        uint16_t HL_;
    };

    union {
        struct {
            uint8_t IXL;
            uint8_t IXH;
        };
        uint16_t IX;
    };

    union {
        struct {
            uint8_t IYL;
            uint8_t IYH;
        };
        uint16_t IY;
    };
    
    // 16-bit registers
    uint16_t SP;            // Stack Pointer
    uint16_t PC;            // Program Counter
//    uint16_t IX;            // Index Register IX
//    uint16_t IY;            // Index Register IY

    // Other registers
    uint8_t I;              // Interrupt Vector
    uint8_t R;              // Refresh Counter (msb not used)

    // Flag bitfields in the F register
    struct FlagRegister {
        uint8_t C : 1;      // Carry flag
        uint8_t N : 1;      // Add/Subtract flag
        uint8_t PV : 1;     // Parity/Overflow flag
        uint8_t H : 1;      // Half-Carry flag
        uint8_t Z : 1;      // Zero flag
        uint8_t S : 1;      // Sign flag
        uint8_t U1 : 1;     // Undocumented bit 5 (unused)
        uint8_t U2 : 1;     // Undocumented bit 3 (unused)
    } flags;

    // Flag bitfields in the alternate F' register
    struct AlternateFlagRegister {
        uint8_t C : 1;
        uint8_t N : 1;
        uint8_t PV : 1;
        uint8_t H : 1;
        uint8_t Z : 1;
        uint8_t S : 1;
        uint8_t U1 : 1;
        uint8_t U2 : 1;
    } flags_;

    // Interrupt flip-flops
    bool IFF1;             // Interrupt Enable Flip-Flop 1
    bool IFF2;             // Interrupt Enable Flip-Flop 2

    // Interrupt mode
    uint8_t interruptMode; // Interrupt Mode (0, 1, or 2)

    // Constructor
    Z80Registers();

    // Public member functions
    void reset();                   // Reset all registers to their initial state
    void exchangeAF();              // Exchange AF and AF'
    void exchangeMainRegisters();   // Exchange BC, DE, HL with BC', DE', HL'
    void setFlag(Flag flag, bool value);
    bool getFlag(Flag flag) const;
    void setAlternateFlag(Flag flag, bool value);
    bool getAlternateFlag(Flag flag) const;
    void updateFlagsRegister();     // Update the F register from the flags struct
    void updateFromFlagsRegister(); // Update the flags struct from the F register
    void setFlags(uint8_t flagsValue);
    void setFlags_(uint8_t flagsValue);

    // Getters for undocumented flags
    bool getUndocumentedFlagU1() const;
    bool getUndocumentedFlagU2() const;

    // Setters for undocumented flags
    void setUndocumentedFlagU1(bool value);
    void setUndocumentedFlagU2(bool value);

    // Getters for undocumented flags for alternate register
    bool getUndocumentedFlagU1_() const;
    bool getUndocumentedFlagU2_() const;

    // Setters for undocumented flags for alternate register
    void setUndocumentedFlagU1_(bool value);
    void setUndocumentedFlagU2_(bool value);
};

#endif // Z80_REGISTERS_HPP