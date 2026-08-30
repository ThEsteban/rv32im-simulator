#include "memory.hpp"
#include "loader.hpp"
#include <stdexcept>
#include <span>
#include <algorithm>

std::size_t Memory::translate_address(uint32_t address) const {
    return address - BASE_OFFSET;
}

Memory::Memory() : memory_(RAM_SIZE, 0) {}

bool Memory::contains_range(uint32_t address, std::size_t size) const noexcept {
    if (address < BASE_OFFSET) {
        return false;
    }

    const std::size_t offset = static_cast<std::size_t>(address - BASE_OFFSET);
    return offset <= memory_.size() && size <= memory_.size() - offset;
}

int32_t Memory::load_byte(uint32_t addr) const {//signed 
    return static_cast<int8_t>(memory_[translate_address(addr)]);
}

uint32_t Memory::load_ubyte(uint32_t addr) const { // unsigned ld
    return memory_[translate_address(addr)];
}

int32_t Memory::load_hws(uint32_t addr) const { //little endian
    const std::size_t offset = translate_address(addr); //save time;  
    if (!(offset + 1 < RAM_SIZE)){
        throw std::runtime_error("out of bounds address for lhw");
    }
    uint16_t raw = memory_[offset + 1] << 8;
    raw |= memory_[offset];
    return static_cast<int16_t>(raw);
}

uint32_t Memory::load_uhw(uint32_t addr) const {
    const std::size_t offset = translate_address(addr); //save time;  
    if (!(offset + 1 < RAM_SIZE)) {
        throw std::runtime_error("luhw out of bounds address");
    }
    uint16_t raw = memory_[offset + 1] << 8;
    raw |= memory_[offset];
    return raw;
}

int32_t Memory::load_ws(uint32_t addr) const {
    const std::size_t offset = translate_address(addr);
    if (!(offset + 3 < RAM_SIZE)) {
        throw std::runtime_error("out of bounds address for lws");
    }
    const uint32_t raw = static_cast<uint32_t>(memory_[offset + 3]) << 24 |
                         static_cast<uint32_t>(memory_[offset + 2]) << 16 |
                         static_cast<uint32_t>(memory_[offset + 1]) << 8 |
                         static_cast<uint32_t>(memory_[offset]);
    return static_cast<int32_t>(raw);
}

uint32_t Memory::load_uw(uint32_t addr) const {
    const std::size_t offset = translate_address(addr);
    if (!(offset + 3 < RAM_SIZE)) {
        throw std::runtime_error("out of bounds address for luw");
    }
    return static_cast<uint32_t>(memory_[offset + 3]) << 24 |
           static_cast<uint32_t>(memory_[offset + 2]) << 16 |
           static_cast<uint32_t>(memory_[offset + 1]) << 8 |
           static_cast<uint32_t>(memory_[offset]);
}

void Memory::store_byte(uint8_t value, uint32_t addr) {//store byte unsigned
    memory_[translate_address(addr)] = value;
}

void Memory::store_hw(uint16_t value, uint32_t addr) {
    const std::size_t offset = translate_address(addr);
    if (!(offset + 1 < RAM_SIZE)) {
        throw std::runtime_error("out of bounds address for shw");
    }
    memory_[offset] = static_cast<uint8_t>(value);
    memory_[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void Memory::store_word(uint32_t value, uint32_t addr) {
    const std::size_t offset = translate_address(addr);
    if (!(offset + 3 < RAM_SIZE)) {
        throw std::runtime_error("out of bounds address for sw");
    }
    memory_[offset] = static_cast<uint8_t>(value);
    memory_[offset + 1] = static_cast<uint8_t>(value >> 8);
    memory_[offset + 2] = static_cast<uint8_t>(value >> 16);
    memory_[offset + 3] = static_cast<uint8_t>(value >> 24);
}

//copy the file data and zero filled bytes into memoery from file, then test the entire cpu tomorrow
void Memory::load_program(const ProgramImage& image){
    //loop to make sure each segment memor size fits in ram, copy file data into virtual addres, then zero ramining bytes

    for(const ProgramSegment& segment : image.segments ) {
        if(segment.virtual_address < BASE_OFFSET) {
            throw std::runtime_error("segment begins below RAM"); 
        }
        //make file offset into a virtual offset from memory vector 0 index
        const std::size_t offset = translate_address(segment.virtual_address);

        //make sure elf fits into ram 
        if(offset > memory_.size()){
            throw std::runtime_error("segment beyond RAM");
        }
        if(!contains_range(segment.virtual_address, segment.memory_size)){
            throw std::runtime_error("segment extends beyond ram"); 
        }
        if(segment.file_data.size() > segment.memory_size){
            throw std::runtime_error("file data exceeds memory size");
        }
    } //don't let invalid program write into ram

    //validated so now copy data
        //copy in data
    for(const ProgramSegment& segment : image.segments){
        const std::size_t offset = translate_address(segment.virtual_address);

        std::copy(segment.file_data.begin(), segment.file_data.end(), memory_.begin() + offset);

        //zero initialize bss portion if there is one
        const std::size_t zeroBegin = offset + segment.file_data.size() ;
        const std::size_t zeroEnd = offset + segment.memory_size;
        std::fill(memory_.begin() + zeroBegin, memory_.begin() + zeroEnd, uint8_t{0});
    }

}
