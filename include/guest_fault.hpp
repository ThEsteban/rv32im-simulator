#ifndef GUEST_FAULT_HPP
#define GUEST_FAULT_HPP

#include <cstdint>
#include <exception>
#include <string>

enum class GuestFaultCause : uint32_t {
    InstructionAddressMisaligned = 0,
    InstructionAccessFault = 1,
    IllegalInstruction = 2,
    Breakpoint = 3,
    LoadAddressMisaligned = 4,
    LoadAccessFault = 5,
    StoreAddressMisaligned = 6,
    StoreAccessFault = 7,
    MachineModeEcall = 11,
};

class GuestFault : public std::exception {
public:
    GuestFault(GuestFaultCause cause, uint32_t mtval, std::string message);

    GuestFaultCause cause() const noexcept;
    uint32_t mtval() const noexcept;
    const char* what() const noexcept override;

private:
    GuestFaultCause cause_;
    uint32_t mtval_;
    std::string message_;
};

#endif
