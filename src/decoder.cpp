#include "decoder.hpp"

DecodedInstruction Decoder::decode(uint32_t instruction) {
    //bitmasks
    constexpr uint32_t mask3 = 0x07; //saves memory in case multiple CPUs 
    constexpr uint32_t mask5 = 0x1F;
    constexpr uint32_t mask7 = 0x7F;
    constexpr uint32_t mask12 = 0x0FFF;

    DecodedInstruction decoded;
    decoded.opcode = instruction & mask7; //isiolate bottom 7 bits
    switch (decoded.opcode) {// switch to fetch type of instruction 
    case 0x33: decoded.type = InstructionType::R; break;
    case 0x13:
    case 0x03:
    case 0x67:
    case 0x73:
    case 0x0F: decoded.type = InstructionType::I; break;
    case 0x23: decoded.type = InstructionType::S; break;
    case 0x63: decoded.type = InstructionType::B; break;
    case 0x37:
    case 0x17: decoded.type = InstructionType::U; break;
    case 0x6F: decoded.type = InstructionType::J; break;
    default: decoded.type = InstructionType::UNKNOWN; break;
    }

    switch (decoded.type) { // fetch necessary bits for each op
    case InstructionType::R:
        decoded.rd = (instruction >> 7) & mask5;
        decoded.funct3 = (instruction >> 12) & mask3;
        decoded.rs1 = (instruction >> 15) & mask5;
        decoded.rs2 = (instruction >> 20) & mask5;
        decoded.funct7 = (instruction >> 25) & mask7;
        break;
    case InstructionType::I:
        decoded.imm = static_cast<int32_t>(((instruction >> 20) & mask12) << 20) >> 20;
        decoded.rd = (instruction >> 7) & mask5;
        decoded.funct3 = (instruction >> 12) & mask3;
        decoded.rs1 = (instruction >> 15) & mask5;
        decoded.funct7 = (instruction >> 25) & mask7;
        break;
    case InstructionType::S: {
        const uint32_t raw = ((instruction >> 7) & mask5) | //process to get 12 bit immediate 
                             (((instruction >> 25) & mask7) << 5);
        decoded.imm = static_cast<int32_t>(raw << 20) >> 20;
        decoded.funct3 = (instruction >> 12) & mask3;
        decoded.rs1 = (instruction >> 15) & mask5;
        decoded.rs2 = (instruction >> 20) & mask5;
        break;
    }
    case InstructionType::B: {
        const uint32_t raw = ((instruction >> 19) & 0x1000) | // imm[12]
                             ((instruction << 4) & 0x0800) | // imm[11]
                             ((instruction >> 20) & 0x07E0) | // imm[10:5]
                             ((instruction >> 7) & 0x001E); // imm[4:1]
        decoded.imm = static_cast<int32_t>(raw << 19) >> 19;
        decoded.funct3 = (instruction >> 12) & mask3;
        decoded.rs1 = (instruction >> 15) & mask5;
        decoded.rs2 = (instruction >> 20) & mask5;
        break;
    }
    case InstructionType::U:
        decoded.rd = (instruction >> 7) & mask5;
        decoded.imm = static_cast<int32_t>(instruction & 0xFFFFF000);
        break;
    case InstructionType::J: {
        decoded.rd = (instruction >> 7) & mask5;
        const uint32_t raw = ((instruction >> 20) & 0x07FE) | //imm[10:1]
                             ((instruction >> 11) & 0x100000) | //imm[20]
                             ((instruction >> 9) & 0x0800) | // imm[11]
                             (instruction & 0x000FF000); // imm[19:12]
        decoded.imm = static_cast<int32_t>(raw << 11) >> 11;
        break;
    }
    default:
        break; //type is unknown, handle illegal instruction in execution phase
    }
    return decoded;
}
