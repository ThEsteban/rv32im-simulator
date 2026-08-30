#include "bus.hpp"

#include "loader.hpp"

#include <iostream>
#include <utility>

GuestFault::GuestFault(GuestFaultCause cause, uint32_t mtval, std::string message)
    : cause_(cause), mtval_(mtval), message_(std::move(message)) {}

uint32_t GuestFault::cause() const noexcept {
    return static_cast<uint32_t>(cause_);
}

uint32_t GuestFault::mtval() const noexcept {
    return mtval_;
}

const char* GuestFault::what() const noexcept {
    return message_.c_str();
}

Bus::Bus() : Bus(std::cout) {}

Bus::Bus(std::ostream& uart_output) : uart_output_(&uart_output) {}

uint32_t Bus::fetch_instruction(uint32_t address) const {
    if (address % 4 != 0) {
        throw GuestFault(GuestFaultCause::InstructionAddressMisaligned, address,
                         "instruction fetch address is misaligned");
    }
    if (!ram_.contains_range(address, 4)) {
        throw GuestFault(GuestFaultCause::InstructionAccessFault, address,
                         "instruction fetch from unmapped address");
    }
    return ram_.load_uw(address);
}

int32_t Bus::load_byte(uint32_t address) const {
    if (address == UART_LINE_STATUS) {
        return 0x60;
    }
    if (address == UART_TX || address == TEST_FINISHER) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "unsupported MMIO byte load");
    }
    if (!ram_.contains_range(address, 1)) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "byte load from unmapped address");
    }
    return ram_.load_byte(address);
}

uint32_t Bus::load_ubyte(uint32_t address) const {
    if (address == UART_LINE_STATUS) {
        return 0x60;
    }
    if (address == UART_TX || address == TEST_FINISHER) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "unsupported MMIO byte load");
    }
    if (!ram_.contains_range(address, 1)) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "byte load from unmapped address");
    }
    return ram_.load_ubyte(address);
}

int32_t Bus::load_hws(uint32_t address) const {
    if (address % 2 != 0) {
        throw GuestFault(GuestFaultCause::LoadAddressMisaligned, address,
                         "halfword load address is misaligned");
    }
    if (!ram_.contains_range(address, 2)) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "halfword load from unmapped or unsupported MMIO address");
    }
    return ram_.load_hws(address);
}

uint32_t Bus::load_uhw(uint32_t address) const {
    if (address % 2 != 0) {
        throw GuestFault(GuestFaultCause::LoadAddressMisaligned, address,
                         "halfword load address is misaligned");
    }
    if (!ram_.contains_range(address, 2)) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "halfword load from unmapped or unsupported MMIO address");
    }
    return ram_.load_uhw(address);
}

int32_t Bus::load_ws(uint32_t address) const {
    if (address % 4 != 0) {
        throw GuestFault(GuestFaultCause::LoadAddressMisaligned, address,
                         "word load address is misaligned");
    }
    if (!ram_.contains_range(address, 4)) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "word load from unmapped or unsupported MMIO address");
    }
    return ram_.load_ws(address);
}

uint32_t Bus::load_uw(uint32_t address) const {
    if (address % 4 != 0) {
        throw GuestFault(GuestFaultCause::LoadAddressMisaligned, address,
                         "word load address is misaligned");
    }
    if (!ram_.contains_range(address, 4)) {
        throw GuestFault(GuestFaultCause::LoadAccessFault, address,
                         "word load from unmapped or unsupported MMIO address");
    }
    return ram_.load_uw(address);
}

void Bus::store_byte(uint8_t value, uint32_t address) {
    if (address == UART_TX) {
        uart_output_->put(static_cast<char>(value));
        uart_output_->flush();
        return;
    }
    if (address == UART_LINE_STATUS || address == TEST_FINISHER) {
        throw GuestFault(GuestFaultCause::StoreAccessFault, address,
                         "unsupported MMIO byte store");
    }
    if (!ram_.contains_range(address, 1)) {
        throw GuestFault(GuestFaultCause::StoreAccessFault, address,
                         "byte store to unmapped address");
    }
    ram_.store_byte(value, address);
}

void Bus::store_hw(uint16_t value, uint32_t address) {
    if (address % 2 != 0) {
        throw GuestFault(GuestFaultCause::StoreAddressMisaligned, address,
                         "halfword store address is misaligned");
    }
    if (!ram_.contains_range(address, 2)) {
        throw GuestFault(GuestFaultCause::StoreAccessFault, address,
                         "halfword store to unmapped or unsupported MMIO address");
    }
    ram_.store_hw(value, address);
}

void Bus::store_word(uint32_t value, uint32_t address) {
    if (address % 4 != 0) {
        throw GuestFault(GuestFaultCause::StoreAddressMisaligned, address,
                         "word store address is misaligned");
    }
    if (address == TEST_FINISHER) {
        halted_ = true;
        exit_code_ = value;
        return;
    }
    if (!ram_.contains_range(address, 4)) {
        throw GuestFault(GuestFaultCause::StoreAccessFault, address,
                         "word store to unmapped or unsupported MMIO address");
    }
    ram_.store_word(value, address);
}

bool Bus::contains_ram_range(uint32_t address, std::size_t size) const noexcept {
    return ram_.contains_range(address, size);
}

void Bus::load_program(const ProgramImage& image) {
    ram_.load_program(image);
}

bool Bus::halted() const noexcept {
    return halted_;
}

uint32_t Bus::exit_code() const noexcept {
    return exit_code_;
}
