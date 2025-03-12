cmake_minimum_required(VERSION 3.10)
project(YourProjectName VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Add source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Testing
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION share/${PROJECT_NAME})

# Custom target for documentation
add_custom_target(doc
    COMMAND echo "Generating documentation..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
