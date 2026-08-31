#ifndef BUS_HPP
#define BUS_HPP

#include "memory.hpp"
#include "guest_fault.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iosfwd>
#include <string>

struct ProgramImage;


class Bus {
public:
    // Platform memory map:
    // 0x00100000             custom word-width test finisher
    // 0x10000000             byte-width UART transmit register
    // 0x10000005             byte-width UART line-status register
    // 0x80000000-0x87FFFFFF  128 MB RAM
    static constexpr uint32_t TEST_FINISHER = 0x00100000;
    static constexpr uint32_t UART_TX = 0x10000000;
    static constexpr uint32_t UART_LINE_STATUS = 0x10000005;

    Bus();
    explicit Bus(std::ostream& uart_output);

    uint32_t fetch_instruction(uint32_t address) const;

    int32_t load_byte(uint32_t address) const;
    uint32_t load_ubyte(uint32_t address) const;
    int32_t load_hws(uint32_t address) const;
    uint32_t load_uhw(uint32_t address) const;
    int32_t load_ws(uint32_t address) const;
    uint32_t load_uw(uint32_t address) const;

    void store_byte(uint8_t value, uint32_t address);
    void store_hw(uint16_t value, uint32_t address);
    void store_word(uint32_t value, uint32_t address);

    bool contains_ram_range(uint32_t address, std::size_t size) const noexcept;
    void load_program(const ProgramImage& image);

    bool halted() const noexcept;
    uint32_t exit_code() const noexcept;
    void reset_halt_state() noexcept;

private:
    Memory ram_;
    std::ostream* uart_output_;
    bool halted_ = false;
    uint32_t exit_code_ = 0;
};

#endif
