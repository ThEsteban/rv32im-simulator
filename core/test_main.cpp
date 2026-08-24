#include "cpu.hpp" 
#include <string> 

int main() {
    CPU<> cpu; 

    try {
        while(true){
            cpu.clk(); 
        }
    } catch(const std::runtime_error& error){
        std::cerr << "Error :" << error.what() <<std::endl; 
    }
}