#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>

struct ProgramImage; 

class Memory {
private:
    std::vector<uint8_t> memory_; // store memory on heap s
                                  // program doesn't hit 
                                  // thread stack limit 
    //helper for load functions, convert standard addr to vector addr
    std::size_t translate_address(uint32_t address) const;

public:
    static constexpr std::size_t RAM_SIZE = 128 * 1024 * 1024;
    static constexpr uint32_t BASE_OFFSET = 0x80000000;// standard mem start
    Memory();

    bool contains_range(uint32_t address, std::size_t size) const noexcept;

    //load word, halfword, byte, signed/unsigned
    //byte, load whatever's at given memory address into destination
    int32_t load_byte(uint32_t addr) const;
    uint32_t load_ubyte(uint32_t addr) const;
    //load halfword signed 
    int32_t load_hws(uint32_t addr) const;
    //unsigned halfword load  
    uint32_t load_uhw(uint32_t addr) const;
    //signed wor load 
    int32_t load_ws(uint32_t addr) const;
    //unsigned word load
    uint32_t load_uw(uint32_t addr) const;

    //store
    void store_byte(uint8_t value, uint32_t addr);
    void store_hw(uint16_t value, uint32_t addr);
    void store_word(uint32_t value, uint32_t addr);

    //load program into memory 
    void load_program(const ProgramImage& image);

};

#endif
