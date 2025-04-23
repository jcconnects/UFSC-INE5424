# Add the new component test to the unit tests
add_executable(component_test components/component_test.cpp)
target_link_libraries(component_test PRIVATE project_lib pthread)
add_test(NAME component_test COMMAND component_test)

# Add this test to the list of all unit tests
add_custom_target(unit_tests 
    DEPENDS 
        buffer_test 
        ethernet_test 
        list_test 
        message_test
        socketEngine_test
        nic_test
        protocol_test
        initializer_test
        vehicle_test
        component_test # Add the new test
) 