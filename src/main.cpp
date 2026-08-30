#include "cpu.hpp"
#include "loader.hpp"

#include <cstddef>
#include <exception>
#include <iostream>

//runner for main function, 

int main(int argc, char* argv[]){
    if(argc!= 2){
        std::cerr << "Usage: " << argv[0] << " <program.elf>" << std::endl;
        return 1;
    }
    try{
        auto fileBytes = read_binary(argv[1]);
        auto image = parse_elf(fileBytes);

        CPU<> cpu;
        cpu.load_program(image);

        constexpr std::size_t maxInstructions = 1'000'000;
        for(std::size_t instructionCount = 0; instructionCount < maxInstructions; ++instructionCount){
            if(cpu.halted()){
                if(cpu.exit_code() != 0){
                    std::cerr << "guest failed with status " << cpu.exit_code() << std::endl;
                    return 1;
                }
                return 0;
            }
            cpu.clk();
        }

        if(cpu.halted()){
            if(cpu.exit_code() != 0){
                std::cerr << "guest failed with status " << cpu.exit_code() << std::endl;
                return 1;
            }
            return 0;
        }

        std::cerr << "guest timed out after " << maxInstructions << " instructions" << std::endl;
        return 2;

    }catch(const GuestFault& fault){
        std::cerr << "guest fault: cause=" << fault.cause()
                  << " mtval=0x" << std::hex << fault.mtval()
                  << ": " << fault.what() << std::endl;
        return 1;
    }catch(const std::exception& error){
        std::cerr << "emulator error: " << error.what() << std::endl;
        return 1;
    }
}
