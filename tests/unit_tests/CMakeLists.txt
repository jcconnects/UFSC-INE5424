# Add the new component test to the unit tests
add_executable(component_test components/component_test.cpp)
target_link_libraries(component_test PRIVATE project_lib pthread)
add_test(NAME component_test COMMAND component_test)

# Add clock test
add_executable(clock_test clock_test.cpp)
target_link_libraries(clock_test PRIVATE project_lib pthread)
add_test(NAME clock_test COMMAND clock_test)

# Add protocol structure test
add_executable(protocol_structure_test protocol_structure_test.cpp)
target_link_libraries(protocol_structure_test PRIVATE project_lib pthread)
add_test(NAME protocol_structure_test COMMAND protocol_structure_test)

# Add leaderKeyStorage test
add_executable(leaderKeyStorage_test leaderKeyStorage_test.cpp)
target_link_libraries(leaderKeyStorage_test PRIVATE project_lib pthread)
add_test(NAME leaderKeyStorage_test COMMAND leaderKeyStorage_test)

# Add statusManager test
add_executable(statusManager_test statusManager_test.cpp)
target_link_libraries(statusManager_test PRIVATE project_lib pthread)
add_test(NAME statusManager_test COMMAND statusManager_test)

# Add radius collision test
add_executable(radius_collision_test radius_collision_test.cpp)
target_link_libraries(radius_collision_test PRIVATE project_lib pthread)
add_test(NAME radius_collision_test COMMAND radius_collision_test)

# Add location service test
add_executable(location_service_test location_service_test.cpp)
target_link_libraries(location_service_test PRIVATE project_lib pthread)
add_test(NAME location_service_test COMMAND location_service_test)

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
        clock_test
        protocol_structure_test
        component_test
        leaderKeyStorage_test
        statusManager_test
        radius_collision_test
        location_service_test
) 