#include "alu.hpp"

#include <stdexcept>

uint32_t IntegerALU::compute(uint32_t a, uint32_t b, ALUop operation) const {
    switch (operation) {
    case ALUop::ADD: return a + b;
    case ALUop::SUB: return a - b;
    case ALUop::AND: return a & b;
    case ALUop::OR: return a | b;
    case ALUop::XOR: return a ^ b;
    case ALUop::SLL: return a << (b & 0x1F);
    case ALUop::SRL: return a >> (b & 0x1F);
    case ALUop::SRA: return static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1F));
    case ALUop::SLT: return static_cast<int32_t>(a) < static_cast<int32_t>(b) ? 1 : 0;
    case ALUop::SLTU: return a < b ? 1 : 0;
    default: throw std::runtime_error("unkown ALU operation");
    }
}
