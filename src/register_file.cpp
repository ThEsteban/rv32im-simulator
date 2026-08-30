#include "register_file.hpp"

#include <stdexcept>

uint32_t RegisterFile::read(std::size_t index) const {
    if (!(index < registers_.size())) {
        throw std::runtime_error("invalid register too large");
    }
    return registers_[index];
}

void RegisterFile::write(std::size_t index, uint32_t value) {
    if (!(index < registers_.size())) {
        throw std::runtime_error("register index too large");
    }
    if (index == 0) {
        return;
    }
    registers_[index] = value;
}
