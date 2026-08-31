#include "csrs.hpp"

namespace {
constexpr uint32_t MSTATUS_MIE = 1u << 3;
constexpr uint32_t MSTATUS_MPIE = 1u << 7;
constexpr uint32_t MSTATUS_MPP = 3u << 11;
constexpr uint32_t MSTATUS_WRITABLE = MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP;
constexpr uint32_t MISA_RV32IM = 0x40001100;
}

std::optional<uint32_t> MachineCSRs::read(uint16_t address) const{
    switch(address){
        case MSTATUS:  return mstatus_;
        case MISA:     return MISA_RV32IM;
        case MTVEC:    return mtvec_;
        case MSCRATCH: return mscratch_;
        case MEPC:     return mepc_;
        case MCAUSE:   return mcause_;
        case MTVAL:    return mtval_;
        default:       return std::nullopt;
    }
}

bool MachineCSRs::write(uint16_t address, uint32_t value){
    switch(address){
        case MSTATUS:
            mstatus_ = (value & MSTATUS_WRITABLE) | MSTATUS_MPP;
            return true;
        case MISA:
            return true;
        case MTVEC:
            mtvec_ = value & ~0x3u;
            return true;
        case MSCRATCH:
            mscratch_ = value;
            return true;
        case MEPC:
            mepc_ = value & ~0x3u;
            return true;
        case MCAUSE:
            mcause_= value;
            return true;
        case MTVAL:
            mtval_ = value;
            return true;
        default:
            return false;
    }
}

void MachineCSRs::reset(){
    mstatus_ = 0;
    mtvec_ = 0;
    mscratch_ = 0;
    mepc_ = 0;
    mcause_ = 0;
    mtval_ = 0;
}
