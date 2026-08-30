#ifndef DECODER_HPP
#define DECODER_HPP

#include "instruction.hpp"

#include <cstdint>

class Decoder {
public:
    //decodes the instruction 
    static DecodedInstruction decode(uint32_t instruction);
};

#endif
