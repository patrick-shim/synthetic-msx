#include "./cpu/Z80.hpp"
#include "./memory/Memory.hpp"
#include <iostream>
#include <iomanip>

// Helper function to print register state
void printRegisters(const Z80Registers& regs) {
    std::cout << "Registers:" << std::endl;
    std::cout << "AF: " << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << regs.AF;
    std::cout << " BC: " << std::setw(4) << regs.BC;
    std::cout << " DE: " << std::setw(4) << regs.DE;
    std::cout << " HL: " << std::setw(4) << regs.HL << std::endl;
    std::cout << "PC: " << std::setw(4) << regs.PC;
    std::cout << " SP: " << std::setw(4) << regs.SP << std::endl;
    
    // Print flags
    std::cout << "Flags: ";
    std::cout << (regs.getFlag(Z80Registers::Sign) ? 'S' : '-');
    std::cout << (regs.getFlag(Z80Registers::Zero) ? 'Z' : '-');
    std::cout << (regs.getFlag(Z80Registers::HalfCarry) ? 'H' : '-');
    std::cout << (regs.getFlag(Z80Registers::ParityOverflow) ? 'P' : '-');
    std::cout << (regs.getFlag(Z80Registers::Subtract) ? 'N' : '-');
    std::cout << (regs.getFlag(Z80Registers::Carry) ? 'C' : '-');
    std::cout << std::endl;
}

int main() {
    Memory memory;
    Z80 cpu(memory);

    // Load a simple program
    std::vector<uint8_t> program = {0x3E, 0x0A,  // LD A, 0x0A
                                   0x06, 0x05,  // LD B, 0x05
                                   0x80,        // ADD A, B
                                   0x04,        // INC B
                                   0x78,        // LD A, B
                                   0x3D,        // DEC A
                                   0x47,        // LD B, A
                                   0x18, 0xF5}; // JR -11 (loop back)
    
    cpu.loadProgram(program, 0x100);

    std::cout << "Program loaded at 0x100. Initial state:" << std::endl;
    printRegisters(cpu.getRegisters());
    std::cout << "\nExecuting program..." << std::endl;

    int instructionCount = 0;
    while (cpu.getRegisters().PC != 0x100 + program.size()) {
        std::cout << "\nStep " << std::dec << ++instructionCount << ":" << std::endl;
        std::cout << "Executing at PC = 0x" << std::hex << cpu.getRegisters().PC << std::endl;
        
        cpu.executeInstruction();
        printRegisters(cpu.getRegisters());
        
        // Optional: add a breakpoint condition
        if (instructionCount > 100) {
            std::cout << "Possible infinite loop detected. Stopping." << std::endl;
            break;
        }
    }

    std::cout << "\nProgram finished. Final state:" << std::endl;
    printRegisters(cpu.getRegisters());
    
    std::cout << "\nMemory dump at program location:" << std::endl;
    memory.dumpMemory(0x100, program.size());

    return 0;
}