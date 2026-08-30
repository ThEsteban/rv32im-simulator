#ifndef ALU_HPP
#define ALU_HPP

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
    SLTU
};

//abstract base class, future proofing for extensions
class ALU {
public://compute
    virtual ~ALU() = default;
    virtual uint32_t compute(uint32_t a, uint32_t b, ALUop operation) const = 0; //made virtual so that I can add more extensions later 
};

//extension declarations all go in some other file extensoinALUs.hpp
class IntegerALU : public ALU {
public:
    uint32_t compute(uint32_t a, uint32_t b, ALUop operation) const override;
};

#endif
