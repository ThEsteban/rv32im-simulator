// Memory-mapped I/O addresses matching your Bus class
volatile char* const UART_TX = (volatile char*)0x10000000;
volatile int* const TEST_FINISHER = (volatile int*)0x00100000;

void print_string(const char* str) {
    while (*str != '\0') {
        *UART_TX = *str; // Triggers the UART output in your emulator
        str++;
    }
}

// _start replaces the standard main() since we have no operating system
void __attribute__((naked)) _start() {
    print_string("Hello from Bare-Metal RISC-V C!\n");
    
    // Write to the test finisher to cleanly halt the CPU
    *TEST_FINISHER = 1; 
    
    while (1) {} // Infinite loop fallback
}