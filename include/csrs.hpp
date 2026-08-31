#ifndef CSRS_HPP
#define CSRS_HPP

#include <cstdint>
#include <optional>

class MachineCSRs{ //implement basic machine mode csrs
    public:
        static constexpr uint16_t MSTATUS  = 0x300;
        static constexpr uint16_t MISA     = 0x301;
        static constexpr uint16_t MTVEC    = 0x305;
        static constexpr uint16_t MSCRATCH = 0x340;
        static constexpr uint16_t MEPC     = 0x341;
        static constexpr uint16_t MCAUSE   = 0x342;
        static constexpr uint16_t MTVAL    = 0x343;

        std::optional<uint32_t> read(uint16_t address) const; // prevents csr storage from thorwing architectural faults
        bool write(uint16_t address, uint32_t value);
        void reset();
    private:
        uint32_t mstatus_ = 0;
        uint32_t mtvec_ = 0;
        uint32_t mscratch_ = 0;
        uint32_t mepc_ = 0;
        uint32_t mcause_ = 0;
        uint32_t mtval_ = 0;
};


#endif
