# Add integration tests executables
add_executable(memory_management_test memory_management_test.cpp)
target_link_libraries(memory_management_test PRIVATE project_lib pthread)
add_test(NAME memory_management_test COMMAND memory_management_test)

add_executable(nic_test nic_test.cpp)
target_link_libraries(nic_test PRIVATE project_lib pthread)
add_test(NAME nic_test COMMAND nic_test)

add_executable(protocol_test protocol_test.cpp)
target_link_libraries(protocol_test PRIVATE project_lib pthread)
add_test(NAME protocol_test COMMAND protocol_test)

add_executable(communicator_test communicator_test.cpp)
target_link_libraries(communicator_test PRIVATE project_lib pthread)
add_test(NAME communicator_test COMMAND communicator_test)

add_executable(vehicle_test vehicle_test.cpp)
target_link_libraries(vehicle_test PRIVATE project_lib pthread)
add_test(NAME vehicle_test COMMAND vehicle_test)

add_executable(initializer_test initializer_test.cpp)
target_link_libraries(initializer_test PRIVATE project_lib pthread)
add_test(NAME initializer_test COMMAND initializer_test)

# Add this test to the list of all integration tests
add_custom_target(integration_tests 
    DEPENDS 
        memory_management_test
        nic_test
        protocol_test
        communicator_test
        vehicle_test
        initializer_test
) 