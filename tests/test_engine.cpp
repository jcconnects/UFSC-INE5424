#include <iostream>
#include <cassert>
#include "engine.h"

// Simple test function for Engine initialization
void test_engine_initialization() {
    std::cout << "Testing Engine initialization..." << std::endl;
    
    Engine engine;
    bool result = engine.initialize();
    
    assert(result == true && "Engine initialization should return true");
    
    std::cout << "Engine initialization test passed!" << std::endl;
}

// Simple test function for Engine run
void test_engine_run() {
    std::cout << "Testing Engine run..." << std::endl;
    
    Engine engine;
    engine.initialize();
    int result = engine.run();
    
    assert(result == 0 && "Engine run should return 0 on success");
    
    std::cout << "Engine run test passed!" << std::endl;
}

// Main test function
int main() {
    std::cout << "Running Engine tests..." << std::endl;
    
    test_engine_initialization();
    test_engine_run();
    
    std::cout << "All Engine tests passed!" << std::endl;
    return 0;
}
