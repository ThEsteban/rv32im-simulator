#include "guest_fault.hpp"

#include <utility>

GuestFault::GuestFault(GuestFaultCause cause, uint32_t mtval, std::string message)
    : cause_(cause), mtval_(mtval), message_(std::move(message)) {}

GuestFaultCause GuestFault::cause() const noexcept {
    return cause_;
}

uint32_t GuestFault::mtval() const noexcept {
    return mtval_;
}

const char* GuestFault::what() const noexcept {
    return message_.c_str();
}
