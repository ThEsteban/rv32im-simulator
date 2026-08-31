# RV32IM Emulator

A C++20 functional emulator for the RISC-V RV32IM instruction set, with Zicsr support and a minimal Machine-mode exception system.

The emulator supports synchronous exception handling through direct `mtvec` trap entry. Interrupts, Supervisor/User modes, paging, and vectored trap handling are not implemented.

## Features

- RV32I base integer instruction set
- RV32M integer multiplication and division extension
- Zicsr CSR instructions
- Machine-mode Control and Status Registers (CSRs)
- Synchronous exception and trap handling
- ELF32 executable loading
- Memory-mapped I/O (MMIO)
- UART terminal output
- Guest-controlled CPU halt

## Project Structure

- `include/` — public headers
- `src/` — emulator implementation
- `tests/` — emulator tests
- `docs/` — RISC-V specification documents

## Build

```bash
mkdir build
cd build
cmake ..
make
```

The resulting emulator executable is located at:
./build/sim

## Running a Bare-Metal C Program

A RISC-V cross-compiler is required to compile programs for the emulated architecture.

For example, compile a freestanding RV32I C program with:

riscv64-linux-gnu-gcc \
    -march=rv32i \
    -mabi=ilp32 \
    -Wl,-m,elf32lriscv \
    -ffreestanding \
    -nostartfiles \
    -nostdlib \
    -T link.ld \
    -o hello.elf \
    hello.c

link.ld places the program at addresses corresponding to the emulator's simulated memory map.

Run the resulting ELF executable with:

./build/sim hello.elf

Guest programs can communicate with the host terminal through the emulator's memory-mapped UART interface.

## Limitations

The emulator currently implements a simplified Machine-mode execution environment. It does not implement:

Asynchronous interrupts.
Supervisor or User privilege modes,
Virtual memory or paging, 
Vectored mtvec trap handling,
Operating-system execution
