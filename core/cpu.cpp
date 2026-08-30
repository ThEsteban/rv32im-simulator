#include "cpu.hpp"
#include <stdexcept>

std::size_t Memory::translate_address(uint32_t raddress) const {
	std::size_t vaddress = raddress - BASE_OFFSET; 
	return vaddress; 
}

Memory::Memory(): memory_(RAM_SIZE,0)  {}

int32_t Memory::load_byte(uint32_t addr) const{//signed 
	return static_cast<int8_t>(memory_[translate_address(addr)]);
}

uint32_t Memory::load_ubyte(uint32_t addr) const { // unsigned ld
	return memory_[translate_address(addr)];
}

int32_t Memory::load_hws(uint32_t addr) const { //little endian
	std::size_t offset = translate_address(addr); //save time;  
	if (offset + 1 < RAM_SIZE){
		throw std::runtime_error("out of bounds address for lhw"); 
	}
	uint16_t raw = memory_[offset+1] << 8;                   
	raw = raw | memory_[offset];
	return static_cast<int16_t>(raw); 
}

uint32_t Memory::load_uhw(uint32_t addr) const {
	std::size_t offset = translate_address(addr); //save time;  
	if((offset + 1 < RAM_SIZE ) == false){
		throw std::runtime_error("luhw out of bounds address");
	}
	uint16_t raw = memory_[offset+1] << 8; 
	raw = raw | memory_[offset]; 
	return raw; 
}

int32_t Memory::load_ws(uint32_t addr) const {
	std::size_t offset = translate_address(addr); 
	if((offset+3 < RAM_SIZE)== false){
		throw std::runtime_error("out of bounds address for lws"); 
	}
	uint32_t raw = static_cast<uint32_t>(memory_[offset+3])<<24 | (static_cast<uint32_t>(memory_[offset+2]) << 16 )|
	(static_cast<uint32_t>(memory_[offset+1]) << 8 )|
	(static_cast<uint32_t>(memory_[offset])); 
	return static_cast<int32_t>(raw); 
}

uint32_t Memory::load_uw(uint32_t addr) const{
	std::size_t offset = translate_address(addr); 
	if(!(offset+3 < RAM_SIZE)){
		throw std::runtime_error("out of bounds address for luw");
	}
	uint32_t raw = static_cast<uint32_t>(memory_[offset+3])<<24 |                        (static_cast<uint32_t>(memory_[offset+2]) << 16 )|
	(static_cast<uint32_t>(memory_[offset+1]) << 8 )|
	(static_cast<uint32_t>(memory_[offset]));
	return raw; 
}

void Memory::store_byte(uint8_t value,uint32_t addr){//store byte unsigned
	memory_[translate_address(addr)] = value;  
}

void Memory::store_hw(uint16_t value, uint32_t addr) {
	std::size_t offset = translate_address(addr); 
	if(!(offset + 1 < RAM_SIZE)){
		throw std::runtime_error("out of bounds address for shw"); 
	}
	memory_[offset] = static_cast<uint8_t>(value) ; 
	memory_[offset +1] = static_cast<uint8_t>(value >> 8); 
}

void Memory::store_word(uint32_t value, uint32_t addr) {
	std::size_t offset = translate_address(addr); 
	if(!(offset + 3 < RAM_SIZE)){
		throw std::runtime_error("out of bounds address for sw"); 
	}
	memory_[offset] = static_cast<uint8_t>(value); 
	memory_[offset+1] = static_cast<uint8_t>(value >> 8);
	memory_[offset+2] = static_cast<uint8_t>(value >> 16);
	memory_[offset+3] = static_cast<uint8_t>(value >> 24);
}

uint32_t RegisterFile::read(std::size_t index) const {
	if(!(index < registers_.size())){
		throw std::runtime_error("invalid register too large"); 
	}
	return registers_[index];
}  

void RegisterFile::write(std::size_t index, uint32_t value) {
	if(!(index < registers_.size())){
		throw std::runtime_error("register index too large"); 
	}
	if (index == 0) return; 
	registers_[index] = value; 
}

uint32_t IntegerALU::compute(uint32_t a , uint32_t b, ALUop operation) const {
	switch(operation){	
	    case ALUop::ADD: return a + b;
        case ALUop::SUB: return a - b;
        case ALUop::AND: return a & b;
        case ALUop::OR:  return a | b;
        case ALUop::XOR: return a ^ b;
        case ALUop::SLL: return a << (b & 0x1F);
		case(ALUop::SRL):return a >> (b &0x1F) ;
		case(ALUop::SRA):return static_cast<uint32_t>(static_cast<int32_t>(a) >> (b &0x1F));
		case(ALUop::SLT):return (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1 : 0; 
		case(ALUop::SLTU): return (a< b) ? 1 : 0; 
		default: throw std::runtime_error("unkown ALU operation") ;
	}
}

