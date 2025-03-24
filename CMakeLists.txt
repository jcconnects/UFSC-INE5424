cmake_minimum_required(VERSION 3.10)
project(ObserverPattern)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Add header files
set(HEADERS
    include/observer.h
    include/observer_impl.h
)

# Common source files
set(COMMON_SOURCES
    src/core/observer_new.cpp
)

# Create test executables
add_executable(test_observer ${COMMON_SOURCES} tests/test_observer.cpp ${HEADERS})
add_executable(test_temp_observer ${COMMON_SOURCES} tests/test_temp_observer.cpp ${HEADERS})

# Set include directories for both tests
target_include_directories(test_observer PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_temp_observer PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Enable testing
enable_testing()
add_test(NAME observer_test COMMAND test_observer)
add_test(NAME temp_observer_test COMMAND test_temp_observer)

# Installation
install(TARGETS test_observer test_temp_observer DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION share/${PROJECT_NAME})

# Custom target for documentation
add_custom_target(doc
    COMMAND echo "Generating documentation..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
