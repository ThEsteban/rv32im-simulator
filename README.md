# RV32IM Simulator

A C++20 RISC-V RV32IM functional emulator with a minimal Machine-mode
exception system and Zicsr support. Synchronous exceptions use direct `mtvec`
trap entry; interrupts, Supervisor/User modes, paging, and vectored traps are
not implemented.

## Build and test

````bash
mkdir build && cd build
cmake ..
make

Public headers are under `include/`, implementations are under `src/`, tests are under `tests/`, and the RISC-V specification documents are under `docs/`.



