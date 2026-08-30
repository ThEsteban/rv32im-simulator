#include "loader.hpp"
#include <stdexcept>

//function to extract the raw binary from the object file
std::vector<uint8_t> read_binary(const std::filesystem::path& filePath){

    std::ifstream file(filePath, std::ios::binary | std::ios::ate); 
    const std::streampos endPosition = file.tellg(); 
    if (endPosition < 0){
        throw std::runtime_error("failed to determine file size");
    }
    const auto fileSize = static_cast<std::size_t>(endPosition); 
    std::vector<uint8_t> bytes(fileSize);
    file.seekg(0, std::ios::beg); 
    if(!file){
        throw std::runtime_error("filaed to seek beginning of file"); 
    }

    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); 
    if(!file){
        throw std::runtime_error("failed to read complete file");
    }
    return bytes; 
}


//helper function for reading 16/32 bit little endian integers

uint16_t read_u16_le(std::span<const uint8_t> bytes, std::size_t offset){
    uint16_t integer = bytes[offset] | static_cast<uint32_t>(bytes[offset + 1]) << 8; 
    return integer ; 
};

uint32_t read_u32_le(std::span<const uint8_t> bytes, std::size_t offset){
    uint32_t integer = bytes[offset] |
                        static_cast<uint32_t>(bytes[offset + 1]) << 8 |
                        static_cast<uint32_t>(bytes[offset + 2]) << 16 | 
                        static_cast<uint32_t>(bytes[offset + 3]) << 24; 
    return integer; 
};

//convert elf binary to usable elf program image 
ProgramImage parse_elf(std::span<const uint8_t> fileBytes){
    if(fileBytes.size() < 52) throw std::runtime_error("truncated ELF error"); 
    if(fileBytes[0] != 0x7F ||
        fileBytes[1] != 'E' || // check magic bytes 
        fileBytes[2] != 'L' ||
        fileBytes[3] != 'F' ||
        fileBytes[4] != 1   ||
        fileBytes[5] != 1   ||
        fileBytes[6] != 1    ){// rv32 supports elf32 little endian 
            throw std::runtime_error("Not an ELF file or incorrect kind (elf32 little-endian current elf version)"); 
        }
    //ef32 header fields
    uint16_t e_type = read_u16_le(fileBytes, 16); 
    uint16_t machine = read_u16_le(fileBytes, 18);
    uint32_t version = read_u32_le(fileBytes, 20);
    uint32_t entry = read_u32_le(fileBytes, 24);
    uint32_t programHeaderOffset = read_u32_le(fileBytes, 28);
    uint16_t elfHeaderSize= read_u16_le(fileBytes, 40); 
    uint16_t programHeaderEntrySize= read_u16_le(fileBytes, 42); 
    uint16_t programHeaderCount = read_u16_le(fileBytes, 44); 
    if(e_type != 2 || machine != 243 || version != 1 || elfHeaderSize != 52 
        || programHeaderEntrySize != 32 || programHeaderCount ==0){
            throw std::runtime_error("not a supported elf file"); 
        }
    ProgramImage image; 
    image.entry_point = entry; 
    

    if (programHeaderOffset > fileBytes.size()) {
        throw std::runtime_error("program header table outside ELF");
    }

    if (programHeaderCount >
        (fileBytes.size() - programHeaderOffset) /
            programHeaderEntrySize) {
        throw std::runtime_error("truncated program header table");
    }
// program headers 
    // initialize each segment loop
    for(uint16_t i = 0; i < programHeaderCount; i ++){
        const std::size_t headerOffset = programHeaderOffset + static_cast<std::size_t>(i) * programHeaderEntrySize ; 
        if( read_u32_le(fileBytes, headerOffset + 0) != 1){ // this is PT_LOAD
            continue; 
        }
        ProgramSegment localSegment;

        uint32_t fileOffset = read_u32_le(fileBytes, headerOffset +4); //location of program segments in fileByts
        uint32_t virtualAddress  = read_u32_le(fileBytes, headerOffset + 8); //location to use in guest ram
        uint32_t fileSize = read_u32_le(fileBytes, headerOffset + 16); //number of bytes to copy 
        uint32_t memorySize = read_u32_le(fileBytes, headerOffset + 20);//total number of guest-memory bytes

        if (memorySize < fileSize) {
            throw std::runtime_error(
                "ELF segment memory size smaller than file size"
            );
        }

        if (fileOffset > fileBytes.size()) {
            throw std::runtime_error(
                "ELF segment offset outside file"
            );
        }

        if (fileSize > fileBytes.size() - fileOffset) {
            throw std::runtime_error(
                "ELF segment extends beyond file"
            );
        }

        localSegment.virtual_address = virtualAddress; 
        localSegment.memory_size = memorySize; 
        localSegment.file_data.assign(fileBytes.begin() + fileOffset, fileBytes.begin() + fileOffset + fileSize); 
        image.segments.push_back(std::move(localSegment)); 
    }
    if(image.segments.empty()){
        throw std::runtime_error("ELF contains no loadable segments");
    }
    return image; 
}


