#include "alu.hpp"
#include <stdexcept>


uint32_t IMALU::compute(uint32_t a, uint32_t b, ALUop operation) const {
    
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
    case ALUop::MUL: return static_cast<uint32_t>(a * b);  // truncate it
    case ALUop::MULH:{
        int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(b); 
        return static_cast<uint64_t>(result) >> 32 ; 
    }
    case ALUop::MULHSU:{ //c++ known issue where int * uint could make operant unsigned and mess up bits 
        int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(b); 
        return static_cast<uint64_t>(result) >> 32 ; 
    }
    case ALUop::DIV: if(b==0){
        return 0xFFFFFFFF; 
        }else if((a == INT32_MIN) && (b == -1)){
            return a; 
        }else return  (a/b); 
    case ALUop::DIVU: return (b == 0) ? 0xFFFFFFFF : a ; 
    case ALUop::REM: if(b==0){
        return a; 
        }else if((a == INT32_MIN) && (b == -1)){
            return 0; 
        }else return (a%b); 
    case ALUop::REMU: return (b == 0)? a : (a%b) ; 
    default: throw std::runtime_error("unkown ALU operation");//host-side issue
    }
}
