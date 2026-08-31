#include "cpu.hpp"
#include "bus.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

template <typename Function>
void expect_guest_fault(Function&& function, GuestFaultCause expectedCause, uint32_t expectedMtval) {
    bool faulted = false;
    try {
        function();
    } catch (const GuestFault& fault) {
        faulted = true;
        assert(fault.cause() == expectedCause);
        assert(fault.mtval() == expectedMtval);
    }
    assert(faulted);
}

uint32_t encode_lui(uint8_t rd, uint32_t value) {
    return ((value + 0x800u) & 0xFFFFF000u) |
           (static_cast<uint32_t>(rd) << 7) | 0x37u;
}

uint32_t encode_addi(uint8_t rd, uint8_t rs1, uint32_t value) {
    return ((value & 0xFFFu) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(rd) << 7) | 0x13u;
}

uint32_t encode_m_instruction(uint8_t funct3, uint8_t rd = 3) {
    return (0x01u << 25) | (2u << 20) | (1u << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) | 0x33u;
}

uint32_t encode_csr(uint16_t csr, uint8_t funct3, uint8_t rd, uint8_t source) {
    return (static_cast<uint32_t>(csr) << 20) |
           (static_cast<uint32_t>(source) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) | 0x73u;
}

uint32_t encode_load(uint8_t funct3, uint8_t rd, uint8_t rs1, int32_t immediate) {
    return ((static_cast<uint32_t>(immediate) & 0xFFFu) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) | 0x03u;
}

uint32_t encode_store(uint8_t funct3, uint8_t rs1, uint8_t rs2, int32_t immediate) {
    const uint32_t encodedImmediate = static_cast<uint32_t>(immediate) & 0xFFFu;
    return ((encodedImmediate >> 5) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           ((encodedImmediate & 0x1Fu) << 7) | 0x23u;
}

uint32_t encode_branch(uint8_t funct3, uint8_t rs1, uint8_t rs2, int32_t immediate) {
    const uint32_t encodedImmediate = static_cast<uint32_t>(immediate) & 0x1FFFu;
    return (((encodedImmediate >> 12) & 0x1u) << 31) |
           (((encodedImmediate >> 5) & 0x3Fu) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (((encodedImmediate >> 1) & 0xFu) << 8) |
           (((encodedImmediate >> 11) & 0x1u) << 7) | 0x63u;
}

uint32_t encode_jal(uint8_t rd, int32_t immediate) {
    const uint32_t encodedImmediate = static_cast<uint32_t>(immediate) & 0x1FFFFFu;
    return (((encodedImmediate >> 20) & 0x1u) << 31) |
           (((encodedImmediate >> 1) & 0x3FFu) << 21) |
           (((encodedImmediate >> 11) & 0x1u) << 20) |
           (((encodedImmediate >> 12) & 0xFFu) << 12) |
           (static_cast<uint32_t>(rd) << 7) | 0x6Fu;
}

uint32_t encode_jalr(uint8_t rd, uint8_t rs1, int32_t immediate) {
    return ((static_cast<uint32_t>(immediate) & 0xFFFu) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(rd) << 7) | 0x67u;
}

uint32_t encode_shift_immediate(uint8_t funct7, uint8_t funct3, uint8_t rd,
                                uint8_t rs1, uint8_t shamt) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(shamt & 0x1Fu) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) | 0x13u;
}

uint32_t execute_encoded(CPU& cpu, uint32_t instruction,
                         uint32_t currentPc = Memory::BASE_OFFSET) {
    return cpu.execute(cpu.decode(instruction), currentPc);
}

void set_register(CPU& cpu, uint8_t reg, uint32_t value) {
    static_cast<void>(execute_encoded(cpu, encode_lui(reg, value)));
    static_cast<void>(execute_encoded(cpu, encode_addi(reg, reg, value)));
}

uint32_t read_csr(CPU& cpu, uint16_t csr) {
    static_cast<void>(execute_encoded(cpu, encode_csr(csr, 0b010, 31, 0)));
    return cpu.get_register(31);
}

void write_csr(CPU& cpu, uint16_t csr, uint32_t value) {
    set_register(cpu, 30, value);
    static_cast<void>(execute_encoded(cpu, encode_csr(csr, 0b001, 0, 30)));
}

uint32_t run_m_instruction(CPU& cpu, uint8_t funct3, uint32_t a, uint32_t b,
                           uint8_t rd = 3) {
    constexpr uint32_t base = Memory::BASE_OFFSET;
    cpu.reset();
    cpu.write_memory_word(base, encode_lui(1, a));
    cpu.write_memory_word(base + 4, encode_addi(1, 1, a));
    cpu.write_memory_word(base + 8, encode_lui(2, b));
    cpu.write_memory_word(base + 12, encode_addi(2, 2, b));
    cpu.write_memory_word(base + 16, encode_m_instruction(funct3, rd));
    for (int instructionCount = 0; instructionCount < 5; ++instructionCount) {
        cpu.clk();
    }
    return cpu.get_register(rd);
}

uint32_t reference_m_result(uint8_t funct3, uint32_t a, uint32_t b) {
    const int64_t signedA = static_cast<int64_t>(static_cast<int32_t>(a));
    const int64_t signedB = static_cast<int64_t>(static_cast<int32_t>(b));
    switch (funct3) {
    case 0b000:
        return static_cast<uint32_t>(static_cast<uint64_t>(a) * b);
    case 0b001:
        return static_cast<uint32_t>(static_cast<uint64_t>(signedA * signedB) >> 32);
    case 0b010:
        return static_cast<uint32_t>(
            static_cast<uint64_t>(signedA * static_cast<int64_t>(b)) >> 32);
    case 0b011:
        return static_cast<uint32_t>((static_cast<uint64_t>(a) * b) >> 32);
    case 0b100:
        if (signedB == 0) {
            return 0xFFFFFFFFu;
        }
        if (signedA == std::numeric_limits<int32_t>::min() && signedB == -1) {
            return 0x80000000u;
        }
        return static_cast<uint32_t>(signedA / signedB);
    case 0b101:
        return b == 0 ? 0xFFFFFFFFu : a / b;
    case 0b110:
        if (signedB == 0) {
            return a;
        }
        if (signedA == std::numeric_limits<int32_t>::min() && signedB == -1) {
            return 0;
        }
        return static_cast<uint32_t>(signedA % signedB);
    case 0b111:
        return b == 0 ? a : a % b;
    default:
        throw std::runtime_error("invalid RV32M test funct3");
    }
}

void run_rv32m_test() {
    struct TestCase {
        uint8_t funct3;
        uint32_t a;
        uint32_t b;
        uint32_t expected;
    };

    constexpr std::array directedCases{
        TestCase{0b000, 0, 0, 0},
        TestCase{0b000, 7, 9, 63},
        TestCase{0b000, 0xFFFFFFFDu, 7, 0xFFFFFFEBu},
        TestCase{0b000, 0xFFFFFFFDu, 0xFFFFFFF9u, 21},
        TestCase{0b000, 0xFFFFFFFFu, 2, 0xFFFFFFFEu},
        TestCase{0b000, 0x80000000u, 2, 0},
        TestCase{0b001, 0, 0, 0},
        TestCase{0b001, 0xFFFFFFFFu, 2, 0xFFFFFFFFu},
        TestCase{0b001, 0x80000000u, 2, 0xFFFFFFFFu},
        TestCase{0b001, 0x7FFFFFFFu, 0x7FFFFFFFu, 0x3FFFFFFFu},
        TestCase{0b010, 0, 0, 0},
        TestCase{0b010, 0xFFFFFFFFu, 2, 0xFFFFFFFFu},
        TestCase{0b010, 0x80000000u, 0xFFFFFFFFu, 0x80000000u},
        TestCase{0b011, 0, 0, 0},
        TestCase{0b011, 0xFFFFFFFFu, 2, 1},
        TestCase{0b011, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFEu},
        TestCase{0b100, 0, 0, 0xFFFFFFFFu},
        TestCase{0b100, 0xFFFFFFF9u, 2, 0xFFFFFFFDu},
        TestCase{0b100, 7, 0xFFFFFFFEu, 0xFFFFFFFDu},
        TestCase{0b100, 0xFFFFFFF9u, 0xFFFFFFFEu, 3},
        TestCase{0b100, 0x80000000u, 0xFFFFFFFFu, 0x80000000u},
        TestCase{0b101, 0, 0, 0xFFFFFFFFu},
        TestCase{0b101, 0xFFFFFFFFu, 2, 0x7FFFFFFFu},
        TestCase{0b101, 0x80000000u, 0, 0xFFFFFFFFu},
        TestCase{0b110, 0, 0, 0},
        TestCase{0b110, 0xFFFFFFF9u, 2, 0xFFFFFFFFu},
        TestCase{0b110, 7, 0xFFFFFFFEu, 1},
        TestCase{0b110, 0xFFFFFFF9u, 0xFFFFFFFEu, 0xFFFFFFFFu},
        TestCase{0b110, 0x80000000u, 0, 0x80000000u},
        TestCase{0b110, 0x80000000u, 0xFFFFFFFFu, 0},
        TestCase{0b111, 0, 0, 0},
        TestCase{0b111, 0xFFFFFFFFu, 2, 1},
        TestCase{0b111, 0x80000000u, 0, 0x80000000u},
    };

    CPU cpu;
    for (const TestCase& test : directedCases) {
        assert(run_m_instruction(cpu, test.funct3, test.a, test.b) == test.expected);
    }

    uint32_t randomState = 0x5EED1234u;
    auto nextRandom = [&randomState] {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    };
    for (int sample = 0; sample < 128; ++sample) {
        const uint32_t a = nextRandom();
        const uint32_t b = nextRandom();
        for (uint8_t funct3 = 0; funct3 < 8; ++funct3) {
            assert(run_m_instruction(cpu, funct3, a, b) ==
                   reference_m_result(funct3, a, b));
        }
    }

    static_cast<void>(run_m_instruction(cpu, 0b000, 7, 9, 0));
    assert(cpu.get_register(0) == 0);
}

void run_pipeline_test() {
    CPU cpu;
    std::cout << "Starting CPU basic pipeline tests...\n";

    // INSTRUCTION 1: LUI x1, 0x80000
    cpu.write_memory_word(0x80000000, 0x800000B7);

    // INSTRUCTION 2: ADDI x2, x0, 42
    cpu.write_memory_word(0x80000004, 0x02A00113);

    // INSTRUCTION 3: SW x2, 256(x1)  <-- CHANGED TO OFFSET 256
    // Stores 42 into memory address (0x80000000 + 256) = 0x80000100
    cpu.write_memory_word(0x80000008, 0x1020A023);

    // INSTRUCTION 4: LW x3, 256(x1)  <-- CHANGED TO OFFSET 256
    // Loads the word at memory address 0x80000100 into register x3
    cpu.write_memory_word(0x8000000C, 0x1000A183);

    // INSTRUCTION 5: ADD x4, x2, x3
    cpu.write_memory_word(0x80000010, 0x00310233);

    // --- EXECUTION & ASSERTIONS ---

    cpu.clk();
    assert(cpu.get_register(1) == 0x80000000 && "FAIL: LUI");
    std::cout << "[PASS] LUI executed correctly.\n";

    cpu.clk();
    assert(cpu.get_register(2) == 42 && "FAIL: ADDI");
    std::cout << "[PASS] ADDI executed correctly.\n";

    cpu.clk();
    // Check the new memory address 0x80000100!
    assert(cpu.read_memory_word(0x80000100) == 42 && "FAIL: SW");
    std::cout << "[PASS] SW executed correctly.\n";

    cpu.clk();
    assert(cpu.get_register(3) == 42 && "FAIL: LW");
    std::cout << "[PASS] LW executed correctly.\n";

    cpu.clk();
    assert(cpu.get_register(4) == 84 && "FAIL: ADD");
    std::cout << "[PASS] ADD executed correctly.\n";

    std::cout << "\n>>> ALL BASIC PIPELINE TESTS PASSED! <<<\n";
}

void run_bus_ram_test() {
    std::ostringstream uartOutput;
    Bus bus(uartOutput);

    bus.store_word(0xA1B2C3D4, Memory::BASE_OFFSET);
    assert(bus.load_uw(Memory::BASE_OFFSET) == 0xA1B2C3D4);
    assert(bus.load_ubyte(Memory::BASE_OFFSET) == 0xD4);
    assert(bus.fetch_instruction(Memory::BASE_OFFSET) == 0xA1B2C3D4);
}

void run_mmio_test() {
    {
        std::ostringstream uartOutput;
        Bus bus(uartOutput);
        bus.store_byte('A', Bus::UART_TX);
        assert(uartOutput.str() == "A");
        assert(bus.load_ubyte(Bus::UART_LINE_STATUS) == 0x60);
    }

    {
        std::ostringstream uartOutput;
        Bus bus(uartOutput);
        bus.store_word(0, Bus::TEST_FINISHER);
        assert(bus.halted());
        assert(bus.exit_code() == 0);
    }

    {
        std::ostringstream uartOutput;
        Bus bus(uartOutput);
        bus.store_word(9, Bus::TEST_FINISHER);
        assert(bus.halted());
        assert(bus.exit_code() == 9);
    }
}

void run_bus_fault_test() {
    std::ostringstream uartOutput;
    Bus bus(uartOutput);
    constexpr uint32_t unmapped = 0x20000000;

    // MMIO and other low addresses are rejected before RAM address translation.
    expect_guest_fault([&] { static_cast<void>(bus.load_ubyte(Bus::UART_TX + 1)); },
                       GuestFaultCause::LoadAccessFault, Bus::UART_TX + 1);
    expect_guest_fault([&] { bus.store_byte(0, Bus::UART_TX + 1); },
                       GuestFaultCause::StoreAccessFault, Bus::UART_TX + 1);

    bool rejectedMmioProgram = false;
    try {
        bus.load_program(ProgramImage{Bus::UART_TX, {{Bus::UART_TX, {0x13, 0, 0, 0}, 4}}});
    } catch (const std::runtime_error&) {
        rejectedMmioProgram = true;
    }
    assert(rejectedMmioProgram);
    assert(uartOutput.str().empty());

    expect_guest_fault([&] { static_cast<void>(bus.fetch_instruction(unmapped)); },
                       GuestFaultCause::InstructionAccessFault, unmapped);
    expect_guest_fault([&] { static_cast<void>(bus.load_uw(unmapped)); },
                       GuestFaultCause::LoadAccessFault, unmapped);
    expect_guest_fault([&] { bus.store_word(0, unmapped); },
                       GuestFaultCause::StoreAccessFault, unmapped);

    expect_guest_fault([&] { static_cast<void>(bus.fetch_instruction(Memory::BASE_OFFSET + 2)); },
                       GuestFaultCause::InstructionAddressMisaligned, Memory::BASE_OFFSET + 2);
    expect_guest_fault([&] { static_cast<void>(bus.load_uhw(Memory::BASE_OFFSET + 1)); },
                       GuestFaultCause::LoadAddressMisaligned, Memory::BASE_OFFSET + 1);
    expect_guest_fault([&] { static_cast<void>(bus.load_uw(Memory::BASE_OFFSET + 2)); },
                       GuestFaultCause::LoadAddressMisaligned, Memory::BASE_OFFSET + 2);
    expect_guest_fault([&] { bus.store_hw(0, Memory::BASE_OFFSET + 1); },
                       GuestFaultCause::StoreAddressMisaligned, Memory::BASE_OFFSET + 1);
    expect_guest_fault([&] { bus.store_word(0, Memory::BASE_OFFSET + 2); },
                       GuestFaultCause::StoreAddressMisaligned, Memory::BASE_OFFSET + 2);

    expect_guest_fault([&] { static_cast<void>(bus.load_ubyte(Bus::UART_TX)); },
                       GuestFaultCause::LoadAccessFault, Bus::UART_TX);
    expect_guest_fault([&] { bus.store_byte(0, Bus::UART_LINE_STATUS); },
                       GuestFaultCause::StoreAccessFault, Bus::UART_LINE_STATUS);
    expect_guest_fault([&] { static_cast<void>(bus.load_uhw(Bus::UART_TX)); },
                       GuestFaultCause::LoadAccessFault, Bus::UART_TX);
    expect_guest_fault([&] { bus.store_word(0, Bus::UART_TX); },
                       GuestFaultCause::StoreAccessFault, Bus::UART_TX);
    expect_guest_fault([&] { static_cast<void>(bus.load_uw(Bus::TEST_FINISHER)); },
                       GuestFaultCause::LoadAccessFault, Bus::TEST_FINISHER);
    expect_guest_fault([&] { bus.store_byte(0, Bus::TEST_FINISHER); },
                       GuestFaultCause::StoreAccessFault, Bus::TEST_FINISHER);
}

void run_system_fault_test() {
    std::ostringstream uartOutput;
    CPU cpu(uartOutput);
    const DecodedInstruction ecall = cpu.decode(0x00000073u);
    assert(ecall.raw == 0x00000073u);
    expect_guest_fault([&] { static_cast<void>(cpu.execute(ecall, Memory::BASE_OFFSET)); },
                       GuestFaultCause::MachineModeEcall, 0);

    const DecodedInstruction ebreak = cpu.decode(0x00100073u);
    expect_guest_fault([&] { static_cast<void>(cpu.execute(ebreak, Memory::BASE_OFFSET)); },
                       GuestFaultCause::Breakpoint, Memory::BASE_OFFSET);

    constexpr std::array malformedSystemInstructions{
        0x000000F3u, // ECALL with rd != x0
        0x00108073u, // EBREAK with rs1 != x0
        0x302000F3u, // MRET with rd != x0
        0x00200073u,
    };
    for (const uint32_t instruction : malformedSystemInstructions) {
        expect_guest_fault([&] { static_cast<void>(execute_encoded(cpu, instruction)); },
                           GuestFaultCause::IllegalInstruction, instruction);
    }
}

void run_csr_instruction_test() {
    CPU cpu;
    constexpr uint16_t scratch = MachineCSRs::MSCRATCH;

    set_register(cpu, 1, 0xA5A5000Fu);
    assert(execute_encoded(cpu, encode_csr(scratch, 0b001, 2, 1)) ==
           Memory::BASE_OFFSET + 4);
    assert(cpu.get_register(2) == 0);
    assert(read_csr(cpu, scratch) == 0xA5A5000Fu);

    write_csr(cpu, scratch, 0x12345678u);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b001, 2, 0)));
    assert(cpu.get_register(2) == 0x12345678u);
    assert(read_csr(cpu, scratch) == 0);

    write_csr(cpu, scratch, 0x000000F0u);
    set_register(cpu, 1, 0x0000000Fu);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b010, 2, 1)));
    assert(cpu.get_register(2) == 0x000000F0u);
    assert(read_csr(cpu, scratch) == 0x000000FFu);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b010, 2, 0)));
    assert(cpu.get_register(2) == 0x000000FFu);
    assert(read_csr(cpu, scratch) == 0x000000FFu);

    set_register(cpu, 1, 0x0000000Cu);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b011, 2, 1)));
    assert(cpu.get_register(2) == 0x000000FFu);
    assert(read_csr(cpu, scratch) == 0x000000F3u);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b011, 2, 0)));
    assert(cpu.get_register(2) == 0x000000F3u);
    assert(read_csr(cpu, scratch) == 0x000000F3u);

    write_csr(cpu, scratch, 0xDEADBEEFu);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b101, 2, 7)));
    assert(cpu.get_register(2) == 0xDEADBEEFu);
    assert(read_csr(cpu, scratch) == 7);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b101, 2, 0)));
    assert(cpu.get_register(2) == 7);
    assert(read_csr(cpu, scratch) == 0);

    write_csr(cpu, scratch, 0x10u);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b110, 2, 3)));
    assert(cpu.get_register(2) == 0x10u);
    assert(read_csr(cpu, scratch) == 0x13u);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b110, 2, 0)));
    assert(cpu.get_register(2) == 0x13u);
    assert(read_csr(cpu, scratch) == 0x13u);

    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b111, 2, 2)));
    assert(cpu.get_register(2) == 0x13u);
    assert(read_csr(cpu, scratch) == 0x11u);
    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b111, 2, 0)));
    assert(cpu.get_register(2) == 0x11u);
    assert(read_csr(cpu, scratch) == 0x11u);

    static_cast<void>(execute_encoded(cpu, encode_csr(scratch, 0b101, 0, 5)));
    assert(cpu.get_register(0) == 0);
    assert(read_csr(cpu, scratch) == 5);

    constexpr uint32_t unsupported =
        (0xFFFu << 20) | (1u << 12) | (1u << 7) | 0x73u;
    set_register(cpu, 1, 0x89ABCDEFu);
    expect_guest_fault([&] { static_cast<void>(execute_encoded(cpu, unsupported)); },
                       GuestFaultCause::IllegalInstruction, unsupported);
    assert(cpu.get_register(1) == 0x89ABCDEFu);

    assert(read_csr(cpu, MachineCSRs::MISA) == 0x40001100u);
    write_csr(cpu, MachineCSRs::MISA, 0xFFFFFFFFu);
    assert(read_csr(cpu, MachineCSRs::MISA) == 0x40001100u);

    write_csr(cpu, MachineCSRs::MTVEC, Memory::BASE_OFFSET + 3);
    assert(read_csr(cpu, MachineCSRs::MTVEC) == Memory::BASE_OFFSET);
    write_csr(cpu, MachineCSRs::MEPC, Memory::BASE_OFFSET + 7);
    assert(read_csr(cpu, MachineCSRs::MEPC) == Memory::BASE_OFFSET + 4);

    write_csr(cpu, MachineCSRs::MSTATUS, 0xFFFFFFFFu);
    assert(read_csr(cpu, MachineCSRs::MSTATUS) == 0x00001888u);

    cpu.reset();
    write_csr(cpu, MachineCSRs::MEPC, Memory::BASE_OFFSET + 0x100);
    assert(execute_encoded(cpu, 0x30200073u) == Memory::BASE_OFFSET + 0x100);
    assert(read_csr(cpu, MachineCSRs::MSTATUS) == 0x00001880u);
}

void run_end_to_end_trap_test() {
    CPU cpu;
    constexpr uint32_t base = Memory::BASE_OFFSET;
    constexpr uint32_t handler = base + 0x40;

    cpu.write_memory_word(base, encode_lui(1, handler));
    cpu.write_memory_word(base + 4, encode_addi(1, 1, handler));
    cpu.write_memory_word(base + 8, encode_csr(MachineCSRs::MTVEC, 0b001, 0, 1));
    cpu.write_memory_word(base + 12, encode_csr(MachineCSRs::MSTATUS, 0b101, 0, 8));
    cpu.write_memory_word(base + 16, 0x00000073u);
    cpu.write_memory_word(base + 20, encode_addi(10, 0, 0x55));

    cpu.write_memory_word(handler, encode_csr(MachineCSRs::MCAUSE, 0b010, 5, 0));
    cpu.write_memory_word(handler + 4, encode_csr(MachineCSRs::MEPC, 0b010, 6, 0));
    cpu.write_memory_word(handler + 8, encode_csr(MachineCSRs::MTVAL, 0b010, 7, 0));
    cpu.write_memory_word(handler + 12, encode_csr(MachineCSRs::MSTATUS, 0b010, 8, 0));
    cpu.write_memory_word(handler + 16, encode_addi(9, 6, 0));
    cpu.write_memory_word(handler + 20, encode_addi(6, 6, 4));
    cpu.write_memory_word(handler + 24, encode_csr(MachineCSRs::MEPC, 0b001, 0, 6));
    cpu.write_memory_word(handler + 28, 0x30200073u);

    for (int instructionCount = 0; instructionCount < 4; ++instructionCount) {
        cpu.clk();
    }
    assert(cpu.read_pc() == base + 16);

    cpu.clk();
    assert(cpu.read_pc() == handler);

    for (int instructionCount = 0; instructionCount < 8; ++instructionCount) {
        cpu.clk();
    }
    assert(cpu.read_pc() == base + 20);
    assert(cpu.get_register(5) == 11);
    assert(cpu.get_register(9) == base + 16);
    assert(cpu.get_register(7) == 0);
    assert(cpu.get_register(8) == 0x00001880u);
    assert(read_csr(cpu, MachineCSRs::MSTATUS) == 0x00001888u);

    cpu.clk();
    assert(cpu.get_register(10) == 0x55);
    assert(cpu.read_pc() == base + 24);
}

void run_trap_precision_test() {
    constexpr uint32_t base = Memory::BASE_OFFSET;
    constexpr uint32_t handler = base + 0x400;
    CPU cpu;

    auto prepareTrap = [&] {
        cpu.reset();
        write_csr(cpu, MachineCSRs::MTVEC, handler);
    };
    auto assertTrap = [&](GuestFaultCause cause, uint32_t mepc, uint32_t mtval) {
        assert(cpu.read_pc() == handler);
        assert(read_csr(cpu, MachineCSRs::MCAUSE) == static_cast<uint32_t>(cause));
        assert(read_csr(cpu, MachineCSRs::MEPC) == mepc);
        assert(read_csr(cpu, MachineCSRs::MTVAL) == mtval);
    };

    prepareTrap();
    cpu.write_memory_word(base, 0xFFFFFFFFu);
    cpu.clk();
    assertTrap(GuestFaultCause::IllegalInstruction, base, 0xFFFFFFFFu);

    prepareTrap();
    set_register(cpu, 1, base + 0x102);
    set_register(cpu, 2, 0xCAFEBABEu);
    cpu.write_memory_word(base, encode_load(0b010, 2, 1, 0));
    cpu.clk();
    assert(cpu.get_register(2) == 0xCAFEBABEu);
    assertTrap(GuestFaultCause::LoadAddressMisaligned, base, base + 0x102);

    prepareTrap();
    set_register(cpu, 1, base + 0x102);
    set_register(cpu, 2, 0xAABBCCDDu);
    cpu.write_memory_word(base + 0x100, 0x11223344u);
    cpu.write_memory_word(base + 0x104, 0x55667788u);
    cpu.write_memory_word(base, encode_store(0b010, 1, 2, 0));
    cpu.clk();
    assert(cpu.read_memory_word(base + 0x100) == 0x11223344u);
    assert(cpu.read_memory_word(base + 0x104) == 0x55667788u);
    assertTrap(GuestFaultCause::StoreAddressMisaligned, base, base + 0x102);

    prepareTrap();
    set_register(cpu, 3, 0x13579BDFu);
    cpu.write_memory_word(base, encode_jal(3, 2));
    cpu.clk();
    assert(cpu.get_register(3) == 0x13579BDFu);
    assertTrap(GuestFaultCause::InstructionAddressMisaligned, base, base + 2);

    prepareTrap();
    cpu.write_memory_word(base, encode_branch(0b000, 0, 0, 2));
    cpu.clk();
    assertTrap(GuestFaultCause::InstructionAddressMisaligned, base, base + 2);

    prepareTrap();
    set_register(cpu, 1, base + 2);
    set_register(cpu, 3, 0x2468ACE0u);
    cpu.write_memory_word(base, encode_jalr(3, 1, 0));
    cpu.clk();
    assert(cpu.get_register(3) == 0x2468ACE0u);
    assertTrap(GuestFaultCause::InstructionAddressMisaligned, base, base + 2);

    prepareTrap();
    constexpr uint32_t unmapped = 0x20000000u;
    set_register(cpu, 1, unmapped);
    cpu.write_memory_word(base, encode_jalr(0, 1, 0));
    cpu.clk();
    assert(cpu.read_pc() == unmapped);
    cpu.clk();
    assertTrap(GuestFaultCause::InstructionAccessFault, unmapped, unmapped);
}

void run_rv32i_legality_test() {
    CPU cpu;
    set_register(cpu, 1, 0x80000001u);

    static_cast<void>(execute_encoded(cpu, encode_shift_immediate(0x00, 0b001, 2, 1, 1)));
    assert(cpu.get_register(2) == 2);
    static_cast<void>(execute_encoded(cpu, encode_shift_immediate(0x00, 0b101, 3, 1, 1)));
    assert(cpu.get_register(3) == 0x40000000u);
    static_cast<void>(execute_encoded(cpu, encode_shift_immediate(0x20, 0b101, 4, 1, 1)));
    assert(cpu.get_register(4) == 0xC0000000u);

    constexpr uint32_t invalidSlli =
        (0x01u << 25) | (1u << 20) | (1u << 15) | (0b001u << 12) | (2u << 7) | 0x13u;
    constexpr uint32_t invalidRightShift =
        (0x01u << 25) | (1u << 20) | (1u << 15) | (0b101u << 12) | (2u << 7) | 0x13u;
    expect_guest_fault([&] { static_cast<void>(execute_encoded(cpu, invalidSlli)); },
                       GuestFaultCause::IllegalInstruction, invalidSlli);
    expect_guest_fault([&] { static_cast<void>(execute_encoded(cpu, invalidRightShift)); },
                       GuestFaultCause::IllegalInstruction, invalidRightShift);

    constexpr uint32_t fence = 0x0000000Fu;
    assert(cpu.decode(fence).raw == fence);
    assert(execute_encoded(cpu, fence) == Memory::BASE_OFFSET + 4);
    constexpr uint32_t fenceI = 0x0000100Fu;
    expect_guest_fault([&] { static_cast<void>(execute_encoded(cpu, fenceI)); },
                       GuestFaultCause::IllegalInstruction, fenceI);
}

void run_reset_test() {
    CPU cpu;
    constexpr uint32_t preservedAddress = Memory::BASE_OFFSET + 0x800;
    cpu.write_memory_word(preservedAddress, 0xAABBCCDDu);
    set_register(cpu, 5, 0x12345678u);
    write_csr(cpu, MachineCSRs::MSTATUS, 0x88u);
    write_csr(cpu, MachineCSRs::MTVEC, Memory::BASE_OFFSET + 0x100);
    write_csr(cpu, MachineCSRs::MSCRATCH, 0xFFFFFFFFu);
    write_csr(cpu, MachineCSRs::MEPC, Memory::BASE_OFFSET + 0x104);
    write_csr(cpu, MachineCSRs::MCAUSE, 7);
    write_csr(cpu, MachineCSRs::MTVAL, 0x1234u);
    cpu.write_memory_word(Bus::TEST_FINISHER, 9);
    assert(cpu.halted());

    cpu.reset();
    assert(cpu.read_pc() == Memory::BASE_OFFSET);
    assert(cpu.get_register(5) == 0);
    assert(!cpu.halted());
    assert(cpu.exit_code() == 0);
    assert(cpu.read_memory_word(preservedAddress) == 0xAABBCCDDu);
    assert(read_csr(cpu, MachineCSRs::MSTATUS) == 0);
    assert(read_csr(cpu, MachineCSRs::MTVEC) == 0);
    assert(read_csr(cpu, MachineCSRs::MSCRATCH) == 0);
    assert(read_csr(cpu, MachineCSRs::MEPC) == 0);
    assert(read_csr(cpu, MachineCSRs::MCAUSE) == 0);
    assert(read_csr(cpu, MachineCSRs::MTVAL) == 0);
}

void write_u16_le(std::vector<uint8_t>& bytes, std::size_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void write_u32_le(std::vector<uint8_t>& bytes, std::size_t offset, uint32_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<uint8_t>(value >> 24);
}

std::vector<uint8_t> make_test_elf() {
    constexpr std::size_t programHeaderOffset = 52;
    constexpr std::size_t loadHeaderOffset = programHeaderOffset + 32;
    constexpr std::size_t segmentOffset = 0x100;
    constexpr uint32_t segmentAddress = Memory::BASE_OFFSET + 0x1000;
    std::vector<uint8_t> bytes(segmentOffset + 4, 0);

    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 1;
    bytes[5] = 1;
    bytes[6] = 1;
    write_u16_le(bytes, 16, 2);
    write_u16_le(bytes, 18, 243);
    write_u32_le(bytes, 20, 1);
    write_u32_le(bytes, 24, segmentAddress);
    write_u32_le(bytes, 28, programHeaderOffset);
    write_u16_le(bytes, 40, 52);
    write_u16_le(bytes, 42, 32);
    write_u16_le(bytes, 44, 2);

    write_u32_le(bytes, programHeaderOffset, 4); // ignored non-PT_LOAD header
    write_u32_le(bytes, loadHeaderOffset, 1);
    write_u32_le(bytes, loadHeaderOffset + 4, segmentOffset);
    write_u32_le(bytes, loadHeaderOffset + 8, segmentAddress);
    write_u32_le(bytes, loadHeaderOffset + 16, 4);
    write_u32_le(bytes, loadHeaderOffset + 20, 8);
    write_u32_le(bytes, loadHeaderOffset + 24, 5);
    write_u32_le(bytes, loadHeaderOffset + 28, 4);
    write_u32_le(bytes, segmentOffset, 0x00000013u);
    return bytes;
}

void run_elf_loader_test() {
    constexpr uint32_t segmentAddress = Memory::BASE_OFFSET + 0x1000;
    auto expectHostError = [](auto&& function) {
        bool failed = false;
        try {
            function();
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    };

    const std::vector<uint8_t> bytes = make_test_elf();
    const ProgramImage image = parse_elf(bytes);
    assert(image.entry_point == segmentAddress);
    assert(image.segments.size() == 1);
    assert(image.segments[0].virtual_address == segmentAddress);
    assert(image.segments[0].file_data == std::vector<uint8_t>({0x13, 0, 0, 0}));
    assert(image.segments[0].memory_size == 8);

    Memory memory;
    memory.store_word(0xFFFFFFFFu, segmentAddress + 4);
    memory.load_program(image);
    assert(memory.load_uw(segmentAddress) == 0x00000013u);
    assert(memory.load_uw(segmentAddress + 4) == 0);

    memory.store_word(0xAABBCCDDu, segmentAddress);
    ProgramImage invalidAtomicImage{
        segmentAddress,
        {
            ProgramSegment{segmentAddress, {1, 2, 3, 4}, 4},
            ProgramSegment{Bus::UART_TX, {5, 6, 7, 8}, 4},
        },
    };
    expectHostError([&] { memory.load_program(invalidAtomicImage); });
    assert(memory.load_uw(segmentAddress) == 0xAABBCCDDu);

    CPU cpu;
    cpu.write_memory_word(segmentAddress + 4, 0xFFFFFFFFu);
    cpu.load_program(image);
    assert(cpu.read_pc() == segmentAddress);
    assert(cpu.read_memory_word(segmentAddress) == 0x00000013u);
    assert(cpu.read_memory_word(segmentAddress + 4) == 0);

    CPU invalidEntryCpu;
    invalidEntryCpu.write_memory_word(segmentAddress, 0xAABBCCDDu);
    ProgramImage invalidEntry = image;
    invalidEntry.entry_point = segmentAddress + 2;
    invalidEntry.segments[0].file_data = {1, 2, 3, 4};
    expectHostError([&] { invalidEntryCpu.load_program(invalidEntry); });
    assert(invalidEntryCpu.read_pc() == Memory::BASE_OFFSET);
    assert(invalidEntryCpu.read_memory_word(segmentAddress) == 0xAABBCCDDu);

    invalidEntry = image;
    invalidEntry.entry_point = segmentAddress + 8;
    expectHostError([&] { invalidEntryCpu.load_program(invalidEntry); });

    invalidEntry = image;
    invalidEntry.entry_point = segmentAddress + 4;
    invalidEntry.segments[0].memory_size = 6;
    expectHostError([&] { invalidEntryCpu.load_program(invalidEntry); });

    expectHostError([&] { static_cast<void>(parse_elf(std::vector<uint8_t>(51))); });

    std::vector<uint8_t> malformed = bytes;
    malformed[0] = 0;
    expectHostError([&] { static_cast<void>(parse_elf(malformed)); });

    malformed = bytes;
    write_u16_le(malformed, 18, 0);
    expectHostError([&] { static_cast<void>(parse_elf(malformed)); });

    malformed = bytes;
    write_u16_le(malformed, 44, 100);
    expectHostError([&] { static_cast<void>(parse_elf(malformed)); });

    malformed = bytes;
    write_u32_le(malformed, 84, 4);
    expectHostError([&] { static_cast<void>(parse_elf(malformed)); });

    malformed = bytes;
    write_u32_le(malformed, 84 + 20, 3);
    expectHostError([&] { static_cast<void>(parse_elf(malformed)); });

    malformed = bytes;
    write_u32_le(malformed, 84 + 4, 0x102);
    expectHostError([&] { static_cast<void>(parse_elf(malformed)); });
}

void run_uart_guest_test() {
    std::ostringstream uartOutput;
    CPU cpu(uartOutput);

    // LUI x1, 0x10000; ADDI x2, x0, 'A'; SB x2, 0(x1)
    cpu.write_memory_word(0x80000000, 0x100000B7);
    cpu.write_memory_word(0x80000004, 0x04100113);
    cpu.write_memory_word(0x80000008, 0x00208023);
    // LUI x3, 0x00100; SW x0, 0(x3)
    cpu.write_memory_word(0x8000000C, 0x001001B7);
    cpu.write_memory_word(0x80000010, 0x0001A023);

    for (int instructionCount = 0; instructionCount < 5 && !cpu.halted(); ++instructionCount) {
        cpu.clk();
    }

    assert(uartOutput.str() == "A");
    assert(cpu.halted());
    assert(cpu.exit_code() == 0);
}

int main() {
    try {
        run_pipeline_test();
        run_bus_ram_test();
        run_mmio_test();
        run_bus_fault_test();
        run_system_fault_test();
        run_csr_instruction_test();
        run_end_to_end_trap_test();
        run_trap_precision_test();
        run_rv32i_legality_test();
        run_reset_test();
        run_elf_loader_test();
        run_uart_guest_test();
        run_rv32m_test();
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] Exception caught during simulation: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
