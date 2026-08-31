volatile char* const UART_TX = (volatile char*)0x10000000;
volatile int* const TEST_FINISHER = (volatile int*)0x00100000;

// Forward declaration so the compiler knows main() exists
void main();


void __attribute__((naked, section(".init"))) _start() {
    __asm__ volatile ("li sp, 0x88000000"); 
    __asm__ volatile ("call main"); 
}

// 2. Helper functions go next
void print_string(const char* str) {
    while (*str != '\0') {
        *UART_TX = *str;
        str++;
    }
}

// 3. Your actual program logic
void main() {
    print_string("Hello from Bare-Metal RISC-V C!\n");
    
    *TEST_FINISHER = 0; 
    
    while (1) {} 
}