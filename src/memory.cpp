#include "memory.hpp"
#include "loader.hpp"
#include <stdexcept>
#include <span>
#include <algorithm>

std::size_t Memory::translate_address(uint32_t address) const {
    return address - BASE_OFFSET;
}

Memory::Memory() : memory_(RAM_SIZE, 0) {}

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
        uint32_t offset = translate_address(segment.virtual_address); 
        //make sure elf fits into ram 
        if(offset > memory_.size()){
            throw std::runtime_error("segment beyond RAM");
        }
        if(segment.memory_size > memory_.size()-offset){
            throw std::runtime_error("segment extends beyond ram"); 
        }
        if(segment.file_data.size() > segment.memory_size){
            throw std::runtime_error("file data exceeds memory size");
        }
        //it for sure fits now
        //loop to load in data
        if(segment.memory_size == segment.file_data.size()){
            for(int i = 0; i < segment.memory_size ; i++){
                store_byte(segment.file_data[i], offset + i); 
            }
        }else{
            for(int i = 0; i < segment.file_data.size() ; i++){
                store_byte(segment.file_data[i], offset + i); 
            }
            //zero initialize rest of memory size for .bss 
            for(uint32_t j = 0; j < segment.memory_size - segment.file_data.size() ; j++){
                store_byte(0x00, offset + segment.file_data.size() + j); 
            }
        }
    }

}




 /*
void Memory::load_binary(uint32_t guest_address, std::span<const uint8_t> bytes){
    if(!(guest_address >= BASE_OFFSET)){
        throw std::runtime_error("program address is invalid"); 
    }
    uint32_t offset = guest_address - BASE_OFFSET;
    if(!(bytes.size() > memory_.size())){// verify program size fits in memory 
        throw std::runtime_error("Program too big"); 
    }
    std::copy(bytes.begin(), bytes.end(), memory_.begin() + offset); 
}

*/
