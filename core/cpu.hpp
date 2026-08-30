#ifndef CPU_HPP
#define CPU_HPP

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <iostream>
#include <string>

class Memory { 
	private: 
		static constexpr std::size_t RAM_SIZE = 128*1024*1024; 
		std::vector<uint8_t> memory_; // store memory on heap s
							   // program doesn't hit 
							   // thread stack limit 
		static const uint32_t BASE_OFFSET = 0x80000000;// standard mem start
		//helper for load functions, convert standard addr to vector addr
		std::size_t translate_address(uint32_t raddress) const;
			
	public:
		Memory();
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
		void store_byte(uint8_t value,uint32_t addr);
		void store_hw(uint16_t value, uint32_t addr);
		void store_word(uint32_t value, uint32_t addr);
};	


class RegisterFile {
	private:
		std::array<uint32_t, 32> registers_= {0};  // array because its unchanging, reduce 
							   // heap overhead
	public:
		uint32_t read(std::size_t index) const;
		void write(std::size_t index, uint32_t value);
}; 


enum class InstructionType{
	R,//register-register arithmetic, opcodes: 0x33
	I,/*immediate arithmetic, opcodes: 0x13(ALU imm), 0x03(loads),
	0x67(jump and link registers), 0x73(system calls), 0x0F(memory fences)
	*/
	S,//stores, opcode: 0x23
	B,//branches, opcode: 0x63
	U,//upper immediates, opcodes: 0x37(load upper imm), 0x17(auipc)
	J,//jump and link, opcode: 0x6F
	UNKNOWN
}; 

struct DecodedInstruction {
	int32_t imm = 0; //immediate value, 
	uint8_t opcode = 0; 
	uint8_t rd = 0; //destination register
	uint8_t funct3 = 0; //sub oberatio identifier 3 bit
	uint8_t rs1= 0; //first source register
	uint8_t rs2 = 0; //second source register
	uint8_t funct7 = 0; //7 bit sub operation identifier
	InstructionType type = InstructionType::UNKNOWN; 
};

enum class ALUop{
	ADD, //arithmetic operations
	SUB,
	AND,//logical operations
	OR,
	XOR,
	SLL,//shift operations, left
	SRL,//right
	SRA,//arithmetic right 
	SLT,	//comparisons, signed set less than, unisgned set less than
	SLTU
}; 

//abstract base class, future proofing for extensions
class ALU {
	public://compute
		virtual ~ALU() = default; 
		virtual uint32_t compute(uint32_t a , uint32_t b, ALUop operation) const = 0; //made virtual so that I can add more extensions later 
};

//extension declarations all go in extensoinALUs.hpp
class IntegerALU : public ALU {
	public:
		uint32_t compute(uint32_t a, uint32_t b, ALUop operation) const override; 
};



template <typename ALUType = IntegerALU> //CPU is now a template for any ALU you implement in extensionALU.hpp

class CPU {
	private:
		Memory ram_; 
		RegisterFile regs_;
		uint32_t pc_ = 0x80000000;
		ALUType alu_;  
		
		using ExecHandler = uint32_t (CPU<ALUType>::*)(const DecodedInstruction&, uint32_t); 
		std::array<ExecHandler, 128> dispatch_table_; //create dispatch table for modularity, for any possible future opcodes
		void table_helper(uint8_t opcode, ExecHandler handler); //helper function to map opcodes ot execution handlers

		//execution handlers, 
		uint32_t execute_R(const DecodedInstruction& instruction, uint32_t current_pc);
		uint32_t execute_I(const DecodedInstruction& instruction, uint32_t current_pc);
		uint32_t execute_S(const DecodedInstruction& instruction, uint32_t current_pc);
		uint32_t execute_branch(const DecodedInstruction& instruction, uint32_t current_pc);
		uint32_t execute_U(const DecodedInstruction& instruction, uint32_t current_pc);
		uint32_t execute_J(const DecodedInstruction& instruction, uint32_t current_pc);





		//bitmasks
		static constexpr uint32_t MASK_3bit = 0x07; //saves memory in case multiple CPUs 
		static constexpr uint32_t MASK_4bit = 0x0F;
		static constexpr uint32_t MASK_5bit = 0x1F;
		static constexpr uint32_t MASK_6bit = 0x3F; 
		static constexpr uint32_t MASK_7bit = 0x7F; 
		static constexpr uint32_t MASK_8bit = 0xFF;
		static constexpr uint32_t MASK_10bit = 0x03FF;
		static constexpr uint32_t MASK_12bit = 0x0FFF;


	public: 
		DecodedInstruction decode(uint32_t instruction);
		uint32_t read_pc() const; 
		uint32_t execute(const DecodedInstruction& instruction, uint32_t current_pc); 
		void clk();
		void reset(){pc_ = 0x80000000; regs_ = RegisterFile(); }; 
		CPU(); //constructor populates dispatch table
		void write_memory_word(uint32_t addr, uint32_t value) { ram_.store_word(value, addr); }
    uint32_t read_memory_word(uint32_t addr) const { return ram_.load_uw(addr); }
    uint32_t read_memory_byte(uint32_t addr) const { return ram_.load_ubyte(addr); }
    uint32_t get_register(std::size_t idx) const { return regs_.read(idx); }
};

//implementations for templated CPU class —----—------------------------

template <typename ALUType> //decodes the instruction 
DecodedInstruction CPU<ALUType>::decode(uint32_t instruction){
	DecodedInstruction decInstruction; 
	decInstruction.opcode = instruction & 0x7F ; //isiolate bottom 7 bits
	switch(decInstruction.opcode){// switch to fetch type of instruction 
	case 0x33: 
		decInstruction.type = InstructionType::R ; 
		break; 
	case 0x13:
	case 0x03:
	case 0x67:
	case 0x73:
	case 0x0F:
		decInstruction.type = InstructionType::I ; 
		break; 
	case 0x23:
		decInstruction.type = InstructionType::S ; 
		break;
	case 0x63:
		decInstruction.type = InstructionType::B; 
		break;
	case 0x37:
	case 0x17:
		decInstruction.type = InstructionType::U ; 
		break; 
	case 0x6F:
		decInstruction.type = InstructionType::J ;
		break;
	default:
		decInstruction.type = InstructionType::UNKNOWN; 
	}
	switch(decInstruction.type){ // fetch necessary bits for each op
	case InstructionType::R :{
		decInstruction.rd = (instruction>>7) & MASK_5bit ; 
		decInstruction.funct3 = (instruction >>12) & MASK_3bit; 
		decInstruction.rs1 = (instruction >>15) & MASK_5bit ; 
		decInstruction.rs2 = (instruction >> 20 ) & MASK_5bit;
		decInstruction.funct7 = (instruction >>25) & MASK_7bit; 
		break; 
	}
	case InstructionType::I : {
		decInstruction.imm = static_cast<int32_t>(((instruction >>20) & MASK_12bit) <<20) >> 20;
		decInstruction.rd = (instruction >> 7) & MASK_5bit;
		decInstruction.funct3 = (instruction >> 12 ) & MASK_3bit; 
		decInstruction.rs1 = (instruction >>15) & MASK_5bit; 
		decInstruction.funct7 = (instruction >> 25) & MASK_7bit; 
		break; 
	}
	case InstructionType::S : {
		uint32_t raw_imms = ((instruction >> 7 ) & MASK_5bit )|//process to get 12 bit immediate 
							(((instruction >> 25 ) & MASK_7bit) << 5);
		decInstruction.imm = static_cast<int32_t>(raw_imms << 20) >>20; 
		decInstruction.funct3 = (instruction >> 12) & MASK_3bit; 
		decInstruction.rs1 = (instruction >> 15) & MASK_5bit; 
		decInstruction.rs2 = (instruction >>20) & MASK_5bit; 
		break; 
	}
	case InstructionType::B :{
		/*decInstruction.imm = (instruction <<4 ) & 0x800 ; //isolate instr[7] and put it in imm[11]
		uint8_t immb2 = (instruction >> 7) & 0x1E ; //isolate lowest 4 bits for imm b type
		uint16_t immb3 = (instruction >>20) & 0x07E0; //isolate imm[10:5] 
		uint16_t immb4 = (instruction >>19) & 0x1000; //isolate imm[12]		sign extend
		decInstruction.imm = ((decInstruction.imm | immb2 | immb3 | immb4) <<19) >>19; //combine into 12bitimm
		*/
		uint32_t raw_imm = ((instruction >> 19) & 0x1000) | // imm[12]
                       ((instruction << 4)  & 0x0800) | // imm[11]
                       ((instruction >> 20) & 0x07E0) | // imm[10:5]
                       ((instruction >> 7)  & 0x001E);  // imm[4:1]
        decInstruction.imm = static_cast<int32_t>(raw_imm <<19)>>19; 
		decInstruction.funct3 = (instruction >> 12) & MASK_3bit; 
		decInstruction.rs1 = (instruction >> 15) & MASK_5bit; 
		decInstruction.rs2 = (instruction >> 20) & MASK_5bit; 
		break; 
	}
	case InstructionType::U :{
		decInstruction.rd = (instruction >> 7) & MASK_5bit; 
		decInstruction.imm = static_cast<int32_t>(instruction & 0xFFFFF000); 
		break;
	}
	case InstructionType::J : {
		decInstruction.rd = (instruction >> 7) & MASK_5bit; 
		uint32_t raw_immj= ((instruction >>20) & 0x07FE) | //imm[10:1]
							((instruction>>11) & 0x100000) | //imm[20]
							((instruction >>9 ) & 0x800) | // imm[11]
							( instruction  & 0x000FF000); // imm[19:12]
		decInstruction.imm = static_cast<int32_t>(raw_immj << 11) >> 11; 
		break;
	}
	default:
		break;  //type is unknown, handle illegal instruction in execution phase
	}
	return decInstruction ; 
}

template <typename ALUType>
uint32_t CPU<ALUType>::execute_branch(const DecodedInstruction& instruction, uint32_t current_pc){
	uint32_t src1 = regs_.read(instruction.rs1); 
	uint32_t src2 = regs_.read(instruction.rs2); 
	bool takebr = false; 
	switch(instruction.funct3){
		case 0b000: takebr = (src1 == src2); break; //BEQ, zero flag 
		case 0b001: takebr = (src1 != src2); break; //BNE, zero flag
		case 0b100: takebr = ((int32_t)src1 < (int32_t)src2); break; // BLT, sign XOR overflow
        case 0b101: takebr = ((int32_t)src1 >= (int32_t)src2); break; // BGE
        case 0b110: takebr = (src1 < src2); break;        // BLTU , borrow flag from full adder
        case 0b111: takebr = (src1 >= src2); break;       // BGEU 
        default:
            //handle illegal instruction exception here (for now, just assert or return pc+4)
           throw std::runtime_error("illegal branch instruction"); 
    }
	if(takebr){
		return current_pc + (instruction.imm); 
	}else{
		return current_pc + 4; 
	}

}

//these all determine aluop and call compute, 
template <typename ALUType> 
uint32_t CPU<ALUType>::execute_R(const DecodedInstruction& instruction, uint32_t current_pc){
	switch(instruction.funct3){
		case 0b000 : switch(instruction.funct7){
			case 0x20 : { uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::SUB);
				regs_.write(instruction.rd, result);
				return current_pc + 4 ;  }
			case 0x00: { uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::ADD);
				regs_.write(instruction.rd, result);
				return current_pc +4 ; }
			default: throw std::runtime_error("unkown ALU instruction, funct7 not valid for funct3=0"); 
		} 
		if(instruction.funct7 == 0x00){
			case 0b001: { uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::SLL);
				regs_.write(instruction.rd, result); 
				return current_pc + 4; }
			case 0b010: {uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::SLT);
				regs_.write(instruction.rd, result);
				return current_pc + 4 ; }
			case 0b011: {uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::SLTU);
				regs_.write(instruction.rd, result);
				return current_pc + 4 ; }
			case 0b100: {uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::XOR);
				regs_.write(instruction.rd, result);
				return current_pc + 4 ; }
			case 0b101: switch(instruction.funct7){
				case 0x20 :{ uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::SRA);
					regs_.write(instruction.rd, result);
					return current_pc + 4 ; }
				case 0x00: {uint32_t result =   alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::SRL);
					regs_.write(instruction.rd, result);
					return current_pc + 4 ; }		
				default: throw std::runtime_error("unknown ALU instruction, funct7 not valid for funct3 = 5"); 
			}
			case 0b110: { uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::OR);
				regs_.write(instruction.rd, result); 
				return current_pc + 4 ; }
			case 0b111: {uint32_t result =  alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), ALUop::AND);
				regs_.write(instruction.rd, result);
				return current_pc + 4 ; }
			default: throw std::runtime_error("unknown ALU operation"); 
		}
		//multiply extension r types utilize r type = 0x01; 
	}
	return current_pc + 4; //for now just skip past unsupported instructions. 
}

template <typename ALUType> 
uint32_t CPU<ALUType>::execute_I(const DecodedInstruction& instruction, uint32_t current_pc ){
	switch(instruction.opcode){
		case(0x13): switch(instruction.funct3){ //alu immediate computations
					case 0b000 : {uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					case 0b001: {uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::SLL);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					case 0b010: {uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::SLT);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					case 0b011:{uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::SLTU);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					case 0b100:{uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::XOR);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					case 0b101: 
						if(instruction.funct7 & 0x20){//funct7 bit 5 distinguishes right shift logical vs right shift arithmetic
							{uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::SRA);
							regs_.write(instruction.rd, result);
							return current_pc + 4;} 
						}else {uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::SRL);
							regs_.write(instruction.rd, result);
							return current_pc + 4;}
					case 0b110:{uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::OR);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					case 0b111:{uint32_t result = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::AND);
						regs_.write(instruction.rd, result);
						return current_pc + 4;}
					default: throw std::runtime_error("unkown ALU operation(failed ALU imm)") ; 
				}
		case 0x03 : { uint32_t addr = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD); //loads
			uint32_t result; 
			switch(instruction.funct3){
				case 0b000: result = static_cast<uint32_t>(ram_.load_byte(addr)); // byte, signed
					break; 
				case 0b001: result = static_cast<uint32_t>(ram_.load_hws(addr)); //halfword , signed
					break; 
				case 0b010: result = ram_.load_ws(addr);//word signed
					break; 
				case 0b100:	result = ram_.load_ubyte(addr); 
					break; 
				case 0b101: result = ram_.load_uhw(addr); 
					break; 
				default : throw std::runtime_error("illegal load instruction"); 
			}	
			regs_.write(instruction.rd, result); 
			return current_pc +4; 
		}
		case 0x67: { //jalr
			uint32_t target = (regs_.read(instruction.rs1) + instruction.imm)  & (~1); //calc target first to prevent issue w rs1 = rd
			if(target%4==0 && (instruction.funct3 == 0)){
				regs_.write(instruction.rd, current_pc + 4);
				return (target); // JALR requires bit 0 to be cleared in riscv spec
			}else{
				throw std::runtime_error("instruction address misaligned"); //make this actual trap later 
			}

		}
		case 0x73: { //system instructions, implementing privelege modes later 
			if(instruction.funct3 == 0 && instruction.imm == 0){//check if it's an ECALL/EBREAK
				//ECALL neesd syscall number, a7, a0-5 hold arguments for syscall, a0 is overwritten by os handler

				uint32_t syscall_num = regs_.read(17); // for now just implementing print, exit, and 
				if(syscall_num == 1){
					std::cout << regs_.read(10)<< std::endl ;  //read a0 and return to program
					return current_pc + 4; 
				}
				else if (syscall_num == 93){
					std::cout << "exit code = " << regs_.read(10) << std::endl; 
					throw std::runtime_error("simulation halted"); 
				}
				else{
					std::cerr<< "EBREAK HIT or unkown ECALL, syscal =" << regs_.read(10) << std::endl; 
					throw std::runtime_error("simulation halted"); 
				}
			}
			else {//csr instruction
				throw std::runtime_error ("unsupported csr instruction"); 
			}
		}
		case 0x0F:	// keep fence as no-op for now, too complicated plus just single-core emulator rn 
			return current_pc +4; 
			break; 
		default: throw std::runtime_error("illegal i-type instruction");
	}
}

template <typename ALUType> 
uint32_t CPU<ALUType>::execute_S(const DecodedInstruction& instruction, uint32_t current_pc){
	uint32_t target;
	switch(instruction.funct3){
		case 0b000: target = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD);  //store byte
		ram_.store_byte(static_cast<uint8_t>(regs_.read(instruction.rs2)), target); 
		break;  
		case 0b001: target = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD);// halfword 
		ram_.store_hw(static_cast<uint16_t>(regs_.read(instruction.rs2)), target); 
		break; 
		case 0b010 : target = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD); //word 
		ram_.store_word(regs_.read(instruction.rs2), target); 
		break; 
		default: throw std::runtime_error("illegal funct3 for S-type instruction") ; break; 
	}
	return current_pc + 4; 
}


template <typename ALUType> 
uint32_t CPU<ALUType>::execute_U(const DecodedInstruction& instruction, uint32_t current_pc){
	switch(instruction.opcode){
		case 0x37: regs_.write(instruction.rd, instruction.imm); break;  //LUI
		case 0x17: regs_.write(instruction.rd, current_pc + instruction.imm); break; //AUIPC
		default: throw std::runtime_error("invalid U-type instruction"); 
	}
	return current_pc +4 ; 
}


template <typename ALUType> 
uint32_t CPU<ALUType>::execute_J(const DecodedInstruction& instruction, uint32_t current_pc){
	uint32_t target = current_pc + instruction.imm ;
	regs_.write(instruction.rd, current_pc + 4); 
	return target; 
}

template <typename ALUType>	//clock, fetch/decode/execute cycle contained here
void CPU<ALUType>::clk(){
	uint32_t instruction = ram_.load_uw(pc_); //fetch
	DecodedInstruction decInstruction = decode(instruction);//decode
	uint32_t current_pc =  pc_; //save current pc for jumps
	/*if(decInstruction.type== InstructionType::B){
		next_pc = execute_branch(decInstruction, current_pc);
	}else{								//!!!!!!!!!!!!!!!!!!
		switch(decInstruction.type){ //write the actual instructions for each handler, not every output is a change in pc
			case InstructionType::R : {
				next_pc = execute_R(decInstruction, current_pc);
			break; 
			}
			case InstructionType::I :{
				next_pc = execute_I(decInstruction, current_pc);
			break;
			}
			case InstructionType::S : {
				next_pc = execute_S(decInstruction, current_pc);
				break;
			}
			case InstructionType::U :{
				next_pc = execute_U(decInstruction, current_pc);
				break; 
			}
			case InstructionType::J : {
				next_pc = execute_J(decInstruction, current_pc);
				break; 
			}
			default: throw std::runtime_error("Illegal instruction"); 
		}
	}*/

	auto handler = dispatch_table_[decInstruction.opcode]; 
	if(handler == nullptr) throw std::runtime_error("unavailable instruciton"); 
	uint32_t next_pc = (this->*handler)(decInstruction, current_pc); 
	pc_ = next_pc; 
}

template <typename ALUType> //read pc
uint32_t CPU<ALUType>::read_pc() const{
	return pc_; 
}

template <typename ALUType>
void CPU<ALUType>::table_helper(uint8_t opcode, ExecHandler handler){
	if (opcode >= dispatch_table_.size()) {
        throw std::out_of_range("Opcode out of range");
    }
    dispatch_table_[opcode] = handler;
}

template <typename ALUType> 
CPU<ALUType>::CPU(){
    dispatch_table_.fill(nullptr);
    
    
    //R-type
    table_helper(0x33, &CPU<ALUType>::execute_R);
    
    // I-type: opcodes 0x13, 0x03, 0x67, 0x73, 0x0F
    table_helper(0x13, &CPU<ALUType>::execute_I);
    table_helper(0x03, &CPU<ALUType>::execute_I);
    table_helper(0x67, &CPU<ALUType>::execute_I);   // JALR
    table_helper(0x73, &CPU<ALUType>::execute_I);   // System
    table_helper(0x0F, &CPU<ALUType>::execute_I);   // FENCE
    
    // S-type (opcode 0x23)
    table_helper(0x23, &CPU<ALUType>::execute_S);
    
    // B-type (opcode 0x63)
    table_helper(0x63, &CPU<ALUType>::execute_branch);
    
    // U-type: 0x37 (LUI) and 0x17 (AUIPC)
    table_helper(0x37, &CPU<ALUType>::execute_U);
    table_helper(0x17, &CPU<ALUType>::execute_U);
    
    // J-type (opcode 0x6F)
    table_helper(0x6F, &CPU<ALUType>::execute_J);
}

#endif // CPU_HPP



