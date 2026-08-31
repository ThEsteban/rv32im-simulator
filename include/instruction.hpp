#ifndef INSTRUCTION_HPP
#define INSTRUCTION_HPP

#include <cstdint>

enum class InstructionType {
    R,//register-register arithmetic, opcodes: 0x33
    I,/*immediate arithmetic, opcodes: 0x13(ALU imm), 0x03(loads),
    0x67(jump and link registers), 0x73(system calls), 0x0F(memory fences)
    */
    S,//stores, opcode: 0x23
    B,//branches, opcode: 0x63
    U,//upper immediates, opcodes: 0x37(load upper imm), 0x17(auipc)
    J,//jump and link, opcode: 0x6F
    UNKNOWN
};

struct DecodedInstruction {
    uint32_t raw = 0;
    int32_t imm = 0; //immediate value, 
    uint8_t opcode = 0;
    uint8_t rd = 0; //destination register
    uint8_t funct3 = 0; //sub oberatio identifier 3 bit
    uint8_t rs1 = 0; //first source register
    uint8_t rs2 = 0; //second source register
    uint8_t funct7 = 0; //7 bit sub operation identifier
    InstructionType type = InstructionType::UNKNOWN;
};

#endif
