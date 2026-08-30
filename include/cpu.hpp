#ifndef CPU_HPP
#define CPU_HPP

#include "alu.hpp"
#include "bus.hpp"
#include "decoder.hpp"
#include "loader.hpp"
#include "register_file.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>

template <typename ALUType = IntegerALU> //CPU is now a template for any ALU you implement in extensionALU.hpp
class CPU {
private:
    Bus bus_;
    RegisterFile regs_;
    uint32_t pc_ = 0x80000000;
    ALUType alu_;

    using ExecHandler = uint32_t (CPU<ALUType>::*)(const DecodedInstruction&, uint32_t);
    std::array<ExecHandler, 128> dispatch_table_; //create dispatch table for modularity, for any possible future opcodes
    void table_helper(uint8_t opcode, ExecHandler handler); //helper function to map opcodes ot execution handlers
    void initialize_dispatch_table();

    //execution handlers, 
    uint32_t execute_R(const DecodedInstruction& instruction, uint32_t current_pc);
    uint32_t execute_I(const DecodedInstruction& instruction, uint32_t current_pc);
    uint32_t execute_S(const DecodedInstruction& instruction, uint32_t current_pc);
    uint32_t execute_branch(const DecodedInstruction& instruction, uint32_t current_pc);
    uint32_t execute_U(const DecodedInstruction& instruction, uint32_t current_pc);
    uint32_t execute_J(const DecodedInstruction& instruction, uint32_t current_pc);

public:
    DecodedInstruction decode(uint32_t instruction);
    uint32_t read_pc() const;
    uint32_t execute(const DecodedInstruction& instruction, uint32_t current_pc);
    void clk();
    void reset() { pc_ = 0x80000000; regs_ = RegisterFile(); }
    CPU(); //constructor populates dispatch table
    explicit CPU(std::ostream& uart_output);
    void write_memory_word(uint32_t addr, uint32_t value) { bus_.store_word(value, addr); }
    uint32_t read_memory_word(uint32_t addr) const { return bus_.load_uw(addr); }
    uint32_t read_memory_byte(uint32_t addr) const { return bus_.load_ubyte(addr); }
    uint32_t get_register(std::size_t index) const { return regs_.read(index); }
    void load_program(const ProgramImage& image);
    bool halted() const noexcept { return bus_.halted(); }
    uint32_t exit_code() const noexcept { return bus_.exit_code(); }
};


//implementations for templated CPU class —----—------------------------

template <typename ALUType>//loads program into memory
void CPU<ALUType>::load_program(const ProgramImage& image){
    if(image.entry_point % 4 != 0){ //validate entry point
        throw std::runtime_error("elf image not supported, entrypoint address inaccessible");
    }
    if(!bus_.contains_ram_range(image.entry_point, 4)){
        throw std::runtime_error("elf image not supported, entrypoint address inaccessible"); 
    }
    //validate entry point exists in loaded segment
    const uint64_t entryBegin = image.entry_point;
    const uint64_t entryEnd = entryBegin + 4;
    bool entryPointLoaded = false;
    for(const ProgramSegment& segment : image.segments){
        const uint64_t segmentBegin = segment.virtual_address;
        const uint64_t segmentEnd = segmentBegin + segment.memory_size;
        if(entryBegin >= segmentBegin && entryEnd <= segmentEnd){
            entryPointLoaded = true;
            break;
        }
    }
    if(!entryPointLoaded){
        throw std::runtime_error("entrypoint not within loaded segments"); 
    }
    bus_.load_program(image);
    pc_ = image.entry_point; 
}

template <typename ALUType> //decodes the instruction 
DecodedInstruction CPU<ALUType>::decode(uint32_t instruction) {
    return Decoder::decode(instruction);
}

template <typename ALUType> //read pc
uint32_t CPU<ALUType>::read_pc() const {
    return pc_;
}

template <typename ALUType>
void CPU<ALUType>::table_helper(uint8_t opcode, ExecHandler handler) {
    if (opcode >= dispatch_table_.size()) {
        throw std::out_of_range("Opcode out of range");
    }
    dispatch_table_[opcode] = handler;
}

template <typename ALUType>
CPU<ALUType>::CPU() {
    initialize_dispatch_table();
}

template <typename ALUType>
CPU<ALUType>::CPU(std::ostream& uart_output) : bus_(uart_output) {
    initialize_dispatch_table();
}

template <typename ALUType>
void CPU<ALUType>::initialize_dispatch_table() {
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

template <typename ALUType> //clock, fetch/decode/execute cycle contained here
void CPU<ALUType>::clk() {
    const uint32_t instruction = bus_.fetch_instruction(pc_); //fetch
    const DecodedInstruction decoded = decode(instruction);//decode
    const uint32_t current_pc = pc_; //save current pc for jumps
    const auto handler = dispatch_table_[decoded.opcode];
    if (handler == nullptr) {
        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction, "unavailable instruciton");
    }
    pc_ = (this->*handler)(decoded, current_pc);
}


//public generic execute function t
template <typename ALUType>
uint32_t CPU<ALUType>::execute(const DecodedInstruction& instruction, uint32_t current_pc) {
    const auto handler = dispatch_table_[instruction.opcode];
    if (handler == nullptr) {
        throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unavailable instruciton");
    }
    return (this->*handler)(instruction, current_pc);
}

//
template <typename ALUType>
uint32_t CPU<ALUType>::execute_branch(const DecodedInstruction& instruction, uint32_t current_pc) {
    const uint32_t src1 = regs_.read(instruction.rs1);
    const uint32_t src2 = regs_.read(instruction.rs2);
    bool take = false;
    switch (instruction.funct3) {
    case 0b000: take = src1 == src2; break; //BEQ, zero flag 
    case 0b001: take = src1 != src2; break; //BNE, zero flag
    case 0b100: take = static_cast<int32_t>(src1) < static_cast<int32_t>(src2); break; // BLT, sign XOR overflow
    case 0b101: take = static_cast<int32_t>(src1) >= static_cast<int32_t>(src2); break; // BGE
    case 0b110: take = src1 < src2; break;        // BLTU , borrow flag from full adder
    case 0b111: take = src1 >= src2; break;       // BGEU 
    default:
        //handle illegal instruction exception here (
        throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "illegal branch instruction");
    }
    if(take){
        uint32_t jump = current_pc + instruction.imm; 
        if(jump%4 != 0){
            throw GuestFault(GuestFaultCause::InstructionAddressMisaligned, jump, "Misaligned address");
        }else return jump; 
    }
    return current_pc +4 ; 
   // return take ? current_pc + instruction.imm : current_pc + 4;
}

//these all determine aluop and call compute, 
template <typename ALUType>
uint32_t CPU<ALUType>::execute_R(const DecodedInstruction& instruction, uint32_t current_pc) {
    ALUop operation;
    switch (instruction.funct3) {
        case 0b000:
            if (instruction.funct7 == 0x20) operation = ALUop::SUB;
            else if (instruction.funct7 == 0x00) operation = ALUop::ADD;
            else throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unkown ALU instruction, funct7 not valid for funct3=0");
            break;
        case 0b001:
            if (instruction.funct7 != 0x00) throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid");
            operation = ALUop::SLL;
            break;
        case 0b010:
            if (instruction.funct7 != 0x00) throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid");
            operation = ALUop::SLT;
            break;
        case 0b011:
            if (instruction.funct7 != 0x00) throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid");
            operation = ALUop::SLTU;
            break;
        case 0b100:
            if (instruction.funct7 != 0x00) throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid");
            operation = ALUop::XOR;
            break;
        case 0b110:
            if (instruction.funct7 != 0x00) throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid");
            operation = ALUop::OR;
            break;
        case 0b111:
            if (instruction.funct7 != 0x00) throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid");
            operation = ALUop::AND;
            break;
        case 0b101:
            if (instruction.funct7 == 0x20) operation = ALUop::SRA;
            else if (instruction.funct7 == 0x00) operation = ALUop::SRL;
            else throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU instruction, funct7 not valid for funct3 = 5");
            break;
        default: throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unknown ALU operation");
    }
    //multiply extension r types utilize r type = 0x01; 
    regs_.write(instruction.rd, alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), operation));
    return current_pc + 4;
}

template <typename ALUType>
uint32_t CPU<ALUType>::execute_I(const DecodedInstruction& instruction, uint32_t current_pc) {
    switch (instruction.opcode) {
    case 0x13: { //alu immediate computations
        ALUop operation;
        switch (instruction.funct3) {
                case 0b000: operation = ALUop::ADD; break;
                case 0b001: 
                    if(instruction.funct7 == 0x00){
                        operation = ALUop::SLL;  
                    }else throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "illegal operation");
                break;
                case 0b010: operation = ALUop::SLT; break;
                case 0b011: operation = ALUop::SLTU; break;
                case 0b100: operation = ALUop::XOR; break;
                case 0b101: operation = instruction.funct7 & 0x20 ? ALUop::SRA : ALUop::SRL; break; //funct7 bit 5 distinguishes right shift logical vs right shift arithmetic
                case 0b110: operation = ALUop::OR; break;
                case 0b111: operation = ALUop::AND; break;
                default: throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unkown ALU operation(failed ALU imm)");
            }
        
        regs_.write(instruction.rd, alu_.compute(regs_.read(instruction.rs1), instruction.imm, operation));
        return current_pc + 4;
    }
    case 0x03: { //loads
        const uint32_t addr = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD);
        uint32_t result;
        switch (instruction.funct3) {
        case 0b000: result = static_cast<uint32_t>(bus_.load_byte(addr)); break; // byte, signed
        case 0b001: result = static_cast<uint32_t>(bus_.load_hws(addr)); break; //halfword , signed
        case 0b010: result = bus_.load_ws(addr); break;//word signed
        case 0b100: result = bus_.load_ubyte(addr); break;
        case 0b101: result = bus_.load_uhw(addr); break;
        default: throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "illegal load instruction");
        }
        regs_.write(instruction.rd, result);
        return current_pc + 4;
    }
    case 0x67: { //jalr
        const uint32_t target = (regs_.read(instruction.rs1) + instruction.imm) & ~1U; //calc target first to prevent issue w rs1 = rd
        if (instruction.funct3 != 0) {
            throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "illegal JALR instruction");
        }
        if (target % 4 != 0) {
            throw GuestFault(GuestFaultCause::InstructionAddressMisaligned, target, "instruction address misaligned"); //make this actual trap later
        }
        regs_.write(instruction.rd, current_pc + 4);
        return target; // JALR requires bit 0 to be cleared in riscv spec
    }
    case 0x73: //system instructions, implementing privelege modes later 
        if (instruction.funct3 == 0 && instruction.imm == 0) {//check if it's an ECALL/EBREAK
            throw GuestFault(GuestFaultCause::MachineModeEcall, 0, "machine-mode ECALL");
        }
        if (instruction.funct3 == 0 && instruction.imm == 1) {
            throw GuestFault(GuestFaultCause::Breakpoint, 0, "EBREAK");
        }
        throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "unsupported csr instruction"); //csr instruction
    case 0x0F: return current_pc + 4; // keep fence as no-op for now, too complicated plus just single-core emulator rn 
    default: throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "illegal i-type instruction");
    }
}


template <typename ALUType>
uint32_t CPU<ALUType>::execute_S(const DecodedInstruction& instruction, uint32_t current_pc) {
    const uint32_t target = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD);
    switch (instruction.funct3) {
    case 0b000: bus_.store_byte(static_cast<uint8_t>(regs_.read(instruction.rs2)), target); break; //store byte
    case 0b001: bus_.store_hw(static_cast<uint16_t>(regs_.read(instruction.rs2)), target); break;// halfword
    case 0b010: bus_.store_word(regs_.read(instruction.rs2), target); break; //word
    default: throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "illegal funct3 for S-type instruction");
    }
    return current_pc + 4;
}

template <typename ALUType>
uint32_t CPU<ALUType>::execute_U(const DecodedInstruction& instruction, uint32_t current_pc) {
    switch (instruction.opcode) {
    case 0x37: regs_.write(instruction.rd, instruction.imm); break;  //LUI
    case 0x17: regs_.write(instruction.rd, current_pc + instruction.imm); break; //AUIPC
    default: throw GuestFault(GuestFaultCause::IllegalInstruction, 0, "invalid U-type instruction");
    }
    return current_pc + 4;
}

template <typename ALUType>
uint32_t CPU<ALUType>::execute_J(const DecodedInstruction& instruction, uint32_t current_pc) {
    const uint32_t target = current_pc + instruction.imm;
    regs_.write(instruction.rd, current_pc + 4);
    return target;
}

#endif // CPU_HPP
