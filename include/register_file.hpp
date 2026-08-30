#ifndef REGISTER_FILE_HPP
#define REGISTER_FILE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

class RegisterFile {
private:
    std::array<uint32_t, 32> registers_{};  // array because its unchanging, reduce 
                                           // heap overhead

public:
    uint32_t read(std::size_t index) const;
    void write(std::size_t index, uint32_t value);
};

#endif
