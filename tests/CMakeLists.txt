cmake_minimum_required(VERSION 3.10)

# Find all test source files
file(GLOB TEST_SOURCES "*.cpp")

# Create test executable
add_executable(runTests ${TEST_SOURCES})

# Link with the main project
target_link_libraries(runTests ${CMAKE_PROJECT_NAME})

# Add tests
add_test(NAME AllTests COMMAND runTests) 