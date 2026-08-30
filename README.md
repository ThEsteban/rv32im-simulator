# RV32I Simulator

A C++17 RISC-V RV32I simulator.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Public headers are under `include/`, implementations are under `src/`, tests are under `tests/`, and the RISC-V specification documents are under `docs/`.
