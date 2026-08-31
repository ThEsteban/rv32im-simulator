#include "cpu.hpp"
#include <iostream>


//loads program into memory
void CPU::load_program(const ProgramImage& image){
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

//decodes the instruction 
DecodedInstruction CPU::decode(uint32_t instruction) {
    return Decoder::decode(instruction);
}

//read pc
uint32_t CPU::read_pc() const {
    return pc_;
}

void CPU::table_helper(uint8_t opcode, ExecHandler handler) {
    if (opcode >= dispatch_table_.size()) {
        throw std::out_of_range("Opcode out of range");
    }
    dispatch_table_[opcode] = handler;
}

CPU::CPU() {
    initialize_dispatch_table();
}

CPU::CPU(std::ostream& uart_output) : bus_(uart_output) {
    initialize_dispatch_table();
}

void CPU::initialize_dispatch_table() {
    dispatch_table_.fill(nullptr);

    //R-type
    table_helper(0x33, &CPU::execute_R);

    // I-type: opcodes 0x13, 0x03, 0x67, 0x73, 0x0F
    table_helper(0x13, &CPU::execute_I);
    table_helper(0x03, &CPU::execute_I);
    table_helper(0x67, &CPU::execute_I);   // JALR
    table_helper(0x73, &CPU::execute_I);   // System
    table_helper(0x0F, &CPU::execute_I);   // FENCE

    // S-type (opcode 0x23)
    table_helper(0x23, &CPU::execute_S);

    // B-type (opcode 0x63)
    table_helper(0x63, &CPU::execute_branch);

    // U-type: 0x37 (LUI) and 0x17 (AUIPC)
    table_helper(0x37, &CPU::execute_U);
    table_helper(0x17, &CPU::execute_U);

    // J-type (opcode 0x6F)
    table_helper(0x6F, &CPU::execute_J);
}

void CPU::reset() {
    pc_ = Memory::BASE_OFFSET;
    regs_ = RegisterFile();
    csrs_.reset();
    bus_.reset_halt_state();
}

//clock, fetch/decode/execute cycle contained here
void CPU::clk() {
    const uint32_t current_pc = pc_; //save current

    try{
        const uint32_t instruction = bus_.fetch_instruction(pc_); //fetch
        const DecodedInstruction decoded = decode(instruction);//decode
        const auto handler = dispatch_table_[decoded.opcode];

        if (handler == nullptr) {
            throw GuestFault(GuestFaultCause::IllegalInstruction, instruction, "unavailable instruciton");
        }
        pc_ = (this->*handler)(decoded, current_pc);
    }catch(const GuestFault& fault){            
        uint32_t mtvec = (csrs_.read(MachineCSRs::MTVEC).value() & ~0x0003) ; 
        if (mtvec == current_pc){// catch a double fault error and show where it was using CSRs 
            uint32_t mepc = csrs_.read(MachineCSRs::MEPC).value();
            uint32_t mcause = csrs_.read(MachineCSRs::MCAUSE).value();
            uint32_t mtval = csrs_.read(MachineCSRs::MTVAL).value();

            std::cerr << "\n--- FATAL DOUBLE FAULT ---\n";
            std::cerr << "Original Faulting PC: 0x" << std::hex << mepc << "\n";
            std::cerr << "MCAUSE code: " << std::dec << mcause << "\n";
            std::cerr << "MTVAL (Bad Address/Instruction): 0x" << std::hex << mtval << "\n\n";
            
            throw std::runtime_error("Double fault: trap vector is unmapped (oops) or invalid"); 
        }
        enter_trap(fault.cause(), fault.mtval(), current_pc);
    }
}

//cpu handles traps
void CPU::enter_trap(GuestFaultCause cause, uint32_t mtval, uint32_t faulting_pc){
    csrs_.write(MachineCSRs::MEPC, faulting_pc);
    csrs_.write(
        MachineCSRs::MCAUSE,
        static_cast<uint32_t>(cause)
    );
    csrs_.write(MachineCSRs::MTVAL, mtval);

    const uint32_t old_status =
        csrs_.read(MachineCSRs::MSTATUS).value();

    uint32_t new_status = old_status;

    // MPIE <- MIE
    const bool mie = (old_status & (1u << 3)) != 0;
    if (mie) {
        new_status |= (1u << 7);
    } else {
        new_status &= ~(1u << 7);
    }

    // MIE <- 0
    new_status &= ~(1u << 3);

    // MPP <- Machine mode
    new_status &= ~(3u << 11);
    new_status |=  (3u << 11);

    csrs_.write(MachineCSRs::MSTATUS, new_status);

    pc_ = csrs_.read(MachineCSRs::MTVEC).value() & ~0x3u;
}


//public generic execute function t
uint32_t CPU::execute(const DecodedInstruction& instruction, uint32_t current_pc) {
    if (instruction.opcode >= dispatch_table_.size()) {
        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unavailable instruciton");
    }
    const auto handler = dispatch_table_[instruction.opcode];
    if (handler == nullptr) {
        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unavailable instruciton");
    }
    return (this->*handler)(instruction, current_pc);
}

//
uint32_t CPU::execute_branch(const DecodedInstruction& instruction, uint32_t current_pc) {
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
        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal branch instruction");
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


uint32_t CPU::execute_system(
    const DecodedInstruction& instruction,
    uint32_t current_pc
) {
    if (instruction.funct3 == 0) {
        switch (instruction.raw) {
        case 0x00000073: // ECALL
            throw GuestFault(
                GuestFaultCause::MachineModeEcall,
                0,
                "machine-mode ECALL"
            );

        case 0x00100073: // EBREAK
            throw GuestFault(
                GuestFaultCause::Breakpoint,
                current_pc,
                "EBREAK"
            );

        case 0x30200073: { // MRET
            const uint32_t oldStatus = csrs_.read(MachineCSRs::MSTATUS).value();
            uint32_t newStatus = oldStatus;
            const bool mpie = (oldStatus & (1u << 7)) != 0;
            if (mpie) {
                newStatus |= 1u << 3;
            } else {
                newStatus &= ~(1u << 3);
            }
            newStatus |= 1u << 7;
            newStatus &= ~(3u << 11);
            newStatus |= 3u << 11;
            csrs_.write(MachineCSRs::MSTATUS, newStatus);
            return csrs_.read(MachineCSRs::MEPC).value();
        }
        default:
            throw GuestFault(
                GuestFaultCause::IllegalInstruction,
                instruction.raw ,
                "illegal SYSTEM instruction"
            );
        }
    }

    const uint16_t csrAddress = static_cast<uint16_t>(static_cast<uint32_t>(instruction.imm) & 0xFFFu);
    const auto oldValue = csrs_.read(csrAddress);
    if (!oldValue.has_value()) {
        throw GuestFault(
            GuestFaultCause::IllegalInstruction,
            instruction.raw,
            "unsupported CSR read"
        );
    }

    bool writeRequired = false;
    uint32_t newValue = 0;
    switch (instruction.funct3) {
    case 0b001: // CSRRW
        writeRequired = true;
        newValue = regs_.read(instruction.rs1);
        break;
    case 0b010: // CSRRS
        writeRequired = instruction.rs1 != 0;
        newValue = *oldValue | regs_.read(instruction.rs1);
        break;
    case 0b011: // CSRRC
        writeRequired = instruction.rs1 != 0;
        newValue = *oldValue & ~regs_.read(instruction.rs1);
        break;
    case 0b101: // CSRRWI
        writeRequired = true;
        newValue = instruction.rs1;
        break;
    case 0b110: // CSRRSI
        writeRequired = instruction.rs1 != 0;
        newValue = *oldValue | instruction.rs1;
        break;
    case 0b111: // CSRRCI
        writeRequired = instruction.rs1 != 0;
        newValue = *oldValue & ~static_cast<uint32_t>(instruction.rs1);
        break;
    default:
        throw GuestFault(
            GuestFaultCause::IllegalInstruction,
            instruction.raw,
            "illegal CSR instruction"
        );
    }

    if (writeRequired && !csrs_.write(csrAddress, newValue)) {
        throw GuestFault(
            GuestFaultCause::IllegalInstruction,
            instruction.raw,
            "unsupported CSR write"
        );
    }
    regs_.write(instruction.rd, *oldValue);

    return current_pc + 4;
}

//these all determine aluop and call compute, 
uint32_t CPU::execute_R(const DecodedInstruction& instruction, uint32_t current_pc) {
    ALUop operation;
    switch (instruction.funct7) {
    case 0x00:
        switch (instruction.funct3) {
        case 0b000: operation = ALUop::ADD; break;
        case 0b001: operation = ALUop::SLL; break;
        case 0b010: operation = ALUop::SLT; break;
        case 0b011: operation = ALUop::SLTU; break;
        case 0b100: operation = ALUop::XOR; break;
        case 0b101: operation = ALUop::SRL; break;
        case 0b110: operation = ALUop::OR; break;
        case 0b111: operation = ALUop::AND; break;
        default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unknown ALU operation");
        }
        break;
    case 0x20:
        switch (instruction.funct3) {
        case 0b000: operation = ALUop::SUB; break;
        case 0b101: operation = ALUop::SRA; break;
        case 0b001:
        case 0b010:
        case 0b011:
        case 0b100:
        case 0b110:
        case 0b111:
            throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unknown ALU instruction, funct7 not valid");
        default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unknown ALU operation");
        }
        break;
    case 0x01:{ // multiplication funct7
        switch(instruction.funct3){
            case 0b000: operation = ALUop::MUL; break; 
            case 0b101: operation = ALUop::DIVU; break; 
            case 0b001: operation = ALUop::MULH; break; 
            case 0b010: operation = ALUop::MULHSU; break; 
            case 0b011: operation = ALUop::MULHU; break; 
            case 0b100: operation = ALUop::DIV; break; 
            case 0b110: operation = ALUop::REM; break; 
            case 0b111: operation = ALUop::REMU; break; 
            default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unknown ALU instruciotn, funct7 not valid");
        }
        break;
    }
    default:
        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unknown ALU instruction");
    }
    //multiply extension r types utilize r type = 0x01; 
    regs_.write(instruction.rd, alu_.compute(regs_.read(instruction.rs1), regs_.read(instruction.rs2), operation));
    return current_pc + 4;
}

uint32_t CPU::execute_I(const DecodedInstruction& instruction, uint32_t current_pc) {
    switch (instruction.opcode) {
    case 0x13: { //alu immediate computations
        ALUop operation;
        switch (instruction.funct3) {
                case 0b000: operation = ALUop::ADD; break;
                case 0b001: 
                    if(instruction.funct7 == 0x00){
                        operation = ALUop::SLL;  
                    }else throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal operation");
                break;
                case 0b010: operation = ALUop::SLT; break;
                case 0b011: operation = ALUop::SLTU; break;
                case 0b100: operation = ALUop::XOR; break;
                case 0b101:
                    if (instruction.funct7 == 0x00) {
                        operation = ALUop::SRL;
                    } else if (instruction.funct7 == 0x20) {
                        operation = ALUop::SRA;
                    } else {
                        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal operation");
                    }
                    break;
                case 0b110: operation = ALUop::OR; break;
                case 0b111: operation = ALUop::AND; break;
                default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "unkown ALU operation(failed ALU imm)");
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
        default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal load instruction");
        }
        regs_.write(instruction.rd, result);
        return current_pc + 4;
    }
    case 0x67: { //jalr
        const uint32_t target = (regs_.read(instruction.rs1) + instruction.imm) & ~1U; //calc target first to prevent issue w rs1 = rd
        if (instruction.funct3 != 0) {
            throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal JALR instruction");
        }
        if (target % 4 != 0) {
            throw GuestFault(GuestFaultCause::InstructionAddressMisaligned, target, "instruction address misaligned");
        }
        regs_.write(instruction.rd, current_pc + 4);
        return target; // JALR requires bit 0 to be cleared in riscv spec
    }
    case 0x73: //system instructions
        return execute_system(instruction, current_pc);
    case 0x0F: //FENCE is a no-op in this single-core functional emulator
        if (instruction.funct3 == 0) {
            return current_pc + 4;
        }
        throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal FENCE instruction");
    default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal i-type instruction");
    }
}


uint32_t CPU::execute_S(const DecodedInstruction& instruction, uint32_t current_pc) {
    const uint32_t target = alu_.compute(regs_.read(instruction.rs1), instruction.imm, ALUop::ADD);
    switch (instruction.funct3) {
    case 0b000: bus_.store_byte(static_cast<uint8_t>(regs_.read(instruction.rs2)), target); break; //store byte
    case 0b001: bus_.store_hw(static_cast<uint16_t>(regs_.read(instruction.rs2)), target); break;// halfword
    case 0b010: bus_.store_word(regs_.read(instruction.rs2), target); break; //word
    default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "illegal funct3 for S-type instruction");
    }
    return current_pc + 4;
}

uint32_t CPU::execute_U(const DecodedInstruction& instruction, uint32_t current_pc) {
    switch (instruction.opcode) {
    case 0x37: regs_.write(instruction.rd, instruction.imm); break;  //LUI
    case 0x17: regs_.write(instruction.rd, current_pc + instruction.imm); break; //AUIPC
    default: throw GuestFault(GuestFaultCause::IllegalInstruction, instruction.raw, "invalid U-type instruction");
    }
    return current_pc + 4;
}

uint32_t CPU::execute_J(const DecodedInstruction& instruction, uint32_t current_pc) {
    const uint32_t target = current_pc + instruction.imm;
    if (target % 4 != 0) {
        throw GuestFault(GuestFaultCause::InstructionAddressMisaligned, target, "instruction address misaligned");
    }
    regs_.write(instruction.rd, current_pc + 4);
    return target;
}
