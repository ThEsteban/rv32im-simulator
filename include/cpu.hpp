#ifndef CPU_HPP
#define CPU_HPP

#include "alu.hpp"
#include "bus.hpp"
#include "decoder.hpp"
#include "loader.hpp"
#include "register_file.hpp"
#include "csrs.hpp"
#include "guest_fault.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>


//CPU uses the current integer ALU implementation.
class CPU {
private:
    MachineCSRs csrs_;
    Bus bus_;
    RegisterFile regs_;
    uint32_t pc_ = 0x80000000;
    IMALU alu_;

    using ExecHandler =
        uint32_t (CPU::*)(const DecodedInstruction&, uint32_t);
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
    uint32_t execute_system(const DecodedInstruction& instruction, uint32_t current_pc);

public:
    DecodedInstruction decode(uint32_t instruction);
    uint32_t read_pc() const;
    uint32_t execute(const DecodedInstruction& instruction, uint32_t current_pc);
    void clk();
    void enter_trap(GuestFaultCause cause, uint32_t mtval, uint32_t address);

    void reset();
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

#endif // CPU_HPP
