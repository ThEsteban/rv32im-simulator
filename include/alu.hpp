#ifndef ALU_HPP
#define ALU_HPP
#define INT32_MIN (-2147483647-1); 

#include <cstdint>

enum class ALUop {
    ADD, //arithmetic operations
    SUB,
    AND,//logical operations
    OR,
    XOR,
    SLL,//shift operations, left
    SRL,//right
    SRA,//arithmetic right 
    SLT, //comparisons, signed set less than, unisgned set less than
    SLTU,
    //multiplying instructions 
    MUL, //choose lower 32 bits unsigned/signed, produces same bit pattern modulo 2^32
    MULH,  //signed * signed upper 32 bits
    MULHSU,// upper 32 bits unsigned * signed
    MULHU, //upper 32 bits unsigned * unsigned
    DIV, //signed div
    DIVU, // unsigned
    REM, //remainder,  non-zero has same sign as dividend
    REMU // unsigned, 
};


class ALU {
public://compute
    virtual ~ALU() = default;
    virtual uint32_t compute(uint32_t a, uint32_t b, ALUop operation) const = 0; //made virtual so that I can add more extensions later 
};

//extension declarations all go in some other file extensoinALUs.hpp
class IMALU : public ALU {
public:
    uint32_t compute(uint32_t a, uint32_t b, ALUop operation) const override;
};

#endif
