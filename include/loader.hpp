#pragma once 
#include <filesystem>
#include <fstream>
#include <vector> 
#include <cstdint>
#include <span>


std::vector<uint8_t> read_binary(const std::filesystem::path& filePath);

struct ProgramSegment{ 
    uint32_t virtual_address; 
    std::vector<uint8_t> file_data; 
    uint32_t memory_size; 
};

struct ProgramImage{
    uint32_t entry_point; 
    std::vector<ProgramSegment> segments; 
};

//interprets raw binary as elf for memory 
ProgramImage parse_elf(std::span<const uint8_t> fileBytes); 
