#include <iostream>
#include <cstring>
#include <vector>
#include <limits>

#include "test_utils.h"
#include "../../include/message.h"
#include "../../include/ethernet.h"

Message::Origin create_test_origin(std::uint8_t mac5 = 0x01, std::uint16_t port = 1234) {
    Ethernet::Address mac_addr = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, mac5}};
    return Message::Origin(mac_addr, port);
}

int main() {
    TEST_INIT("P3 Message Test");
    
    // Test 1: Create and verify an INTEREST message
    {
        Message::Origin origin1 = create_test_origin(0x01, 1001);
        DataTypeId unit_type1 = DataTypeId::VEHICLE_SPEED;
        unsigned int period1 = 100; // 100 microseconds

        Message interest_msg(Message::Type::INTEREST, origin1, unit_type1, period1);

        TEST_ASSERT(interest_msg.message_type() == Message::Type::INTEREST, "Test 1.1: Message type should be INTEREST");
        TEST_ASSERT(interest_msg.origin().paddr() == origin1.paddr(), "Test 1.2: Origin MAC should match");
        TEST_ASSERT(interest_msg.origin().port() == origin1.port(), "Test 1.3: Origin Port should match");
        TEST_ASSERT(interest_msg.unit_type() == unit_type1, "Test 1.4: Unit type should match");
        TEST_ASSERT(interest_msg.period() == period1, "Test 1.5: Period should match");
        TEST_ASSERT(interest_msg.timestamp() > 0, "Test 1.6: Timestamp should be set");
        TEST_ASSERT(interest_msg.value() == nullptr, "Test 1.7: Value should be null for INTEREST");
        TEST_ASSERT(interest_msg.value_size() == 0, "Test 1.8: Value size should be 0 for INTEREST");

        // Test 1.9: Serialization and Deserialization
        const void* serialized_data = interest_msg.data();
        unsigned int serialized_size = interest_msg.size();
        TEST_ASSERT(serialized_data != nullptr && serialized_size > 0, "Test 1.9.1: Serialized data should be valid");

        Message deserialized_msg = Message::deserialize(serialized_data, serialized_size);
        TEST_ASSERT(deserialized_msg.message_type() == Message::Type::INTEREST, "Test 1.9.2: Deserialized type mismatch");
        TEST_ASSERT(deserialized_msg.origin().paddr() == origin1.paddr(), "Test 1.9.3: Deserialized MAC mismatch");
        TEST_ASSERT(deserialized_msg.origin().port() == origin1.port(), "Test 1.9.4: Deserialized Port mismatch");
        TEST_ASSERT(deserialized_msg.unit_type() == unit_type1, "Test 1.9.6: Deserialized unit_type mismatch");
        TEST_ASSERT(deserialized_msg.period() == period1, "Test 1.9.7: Deserialized period mismatch");
        TEST_ASSERT(deserialized_msg.value() == nullptr, "Test 1.9.8: Deserialized value should be null for INTEREST");
    }

    // Test 2: Create and verify a RESPONSE message
    {
        Message::Origin origin2 = create_test_origin(0x02, 2002);
        DataTypeId unit_type2 = DataTypeId::ENGINE_RPM;
        std::vector<std::uint8_t> value2 = {0x01, 0x02, 0x03, 0x04, 0x05};
        unsigned int period2 = 0; // Period is not used for RESPONSE, but constructor takes it.

        Message response_msg(Message::Type::RESPONSE, origin2, unit_type2, period2, value2.data(), value2.size());

        TEST_ASSERT(response_msg.message_type() == Message::Type::RESPONSE, "Test 2.1: Message type should be RESPONSE");
        TEST_ASSERT(response_msg.origin().paddr() == origin2.paddr(), "Test 2.2: Origin MAC should match");
        TEST_ASSERT(response_msg.origin().port() == origin2.port(), "Test 2.3: Origin Port should match");
        TEST_ASSERT(response_msg.unit_type() == unit_type2, "Test 2.4: Unit type should match");
        TEST_ASSERT(response_msg.value_size() == value2.size(), "Test 2.5: Value size should match");
        TEST_ASSERT(response_msg.value() != nullptr && std::memcmp(response_msg.value(), value2.data(), value2.size()) == 0, "Test 2.6: Value data should match");

        // Test 2.7: Serialization and Deserialization
        const void* serialized_data_resp = response_msg.data();
        unsigned int serialized_size_resp = response_msg.size();
        TEST_ASSERT(serialized_data_resp != nullptr && serialized_size_resp > 0, "Test 2.7.1: Serialized data for RESPONSE should be valid");

        Message deserialized_resp_msg = Message::deserialize(serialized_data_resp, serialized_size_resp);
        TEST_ASSERT(deserialized_resp_msg.message_type() == Message::Type::RESPONSE, "Test 2.7.2: Deserialized RESPONSE type mismatch");
        TEST_ASSERT(deserialized_resp_msg.origin().paddr() == origin2.paddr(), "Test 2.7.3: Deserialized RESPONSE MAC mismatch");
        TEST_ASSERT(deserialized_resp_msg.origin().port() == origin2.port(), "Test 2.7.4: Deserialized RESPONSE Port mismatch");
        TEST_ASSERT(deserialized_resp_msg.unit_type() == unit_type2, "Test 2.7.5: Deserialized RESPONSE unit_type mismatch");
        TEST_ASSERT(deserialized_resp_msg.value_size() == value2.size(), "Test 2.7.6: Deserialized RESPONSE value size mismatch");
        TEST_ASSERT(deserialized_resp_msg.value() != nullptr && std::memcmp(deserialized_resp_msg.value(), value2.data(), value2.size()) == 0, "Test 2.7.7: Deserialized RESPONSE value data mismatch");
    }

    // Test 3: Create and verify a REG_PRODUCER message
    {
        Message::Origin origin3 = create_test_origin(0x03, 3003);
        DataTypeId unit_type3 = DataTypeId::SYSTEM_INTERNAL_REG_PRODUCER;
        // Period and value are not used for REG_PRODUCER

        Message reg_msg(Message::Type::REG_PRODUCER, origin3, unit_type3);

        TEST_ASSERT(reg_msg.message_type() == Message::Type::REG_PRODUCER, "Test 3.1: Message type should be REG_PRODUCER");
        TEST_ASSERT(reg_msg.origin().paddr() == origin3.paddr(), "Test 3.2: Origin MAC should match for REG_PRODUCER");
        TEST_ASSERT(reg_msg.origin().port() == origin3.port(), "Test 3.3: Origin Port should match for REG_PRODUCER");
        TEST_ASSERT(reg_msg.unit_type() == unit_type3, "Test 3.4: Unit type should be SYSTEM_INTERNAL_REG_PRODUCER");
        TEST_ASSERT(reg_msg.period() == 0, "Test 3.5: Period should be 0 for REG_PRODUCER");
        TEST_ASSERT(reg_msg.value() == nullptr, "Test 3.6: Value should be null for REG_PRODUCER");
        TEST_ASSERT(reg_msg.value_size() == 0, "Test 3.7: Value size should be 0 for REG_PRODUCER");

        // Test 3.8: Serialization and Deserialization
        const void* serialized_data_reg = reg_msg.data();
        unsigned int serialized_size_reg = reg_msg.size();
        TEST_ASSERT(serialized_data_reg != nullptr && serialized_size_reg > 0, "Test 3.8.1: Serialized data for REG_PRODUCER should be valid");

        Message deserialized_reg_msg = Message::deserialize(serialized_data_reg, serialized_size_reg);
        TEST_ASSERT(deserialized_reg_msg.message_type() == Message::Type::REG_PRODUCER, "Test 3.8.2: Deserialized REG_PRODUCER type mismatch");
        TEST_ASSERT(deserialized_reg_msg.origin().paddr() == origin3.paddr(), "Test 3.8.3: Deserialized REG_PRODUCER MAC mismatch");
        TEST_ASSERT(deserialized_reg_msg.origin().port() == origin3.port(), "Test 3.8.4: Deserialized REG_PRODUCER Port mismatch");
        TEST_ASSERT(deserialized_reg_msg.unit_type() == unit_type3, "Test 3.8.5: Deserialized REG_PRODUCER unit_type mismatch");
    }
    
    // Test 4: RESPONSE message with empty value
    {
        Message::Origin origin4 = create_test_origin(0x04, 4004);
        DataTypeId unit_type4 = DataTypeId::GPS_POSITION;
        Message response_empty_value_msg(Message::Type::RESPONSE, origin4, unit_type4, 0, nullptr, 0);

        TEST_ASSERT(response_empty_value_msg.message_type() == Message::Type::RESPONSE, "Test 4.1: Empty value RESPONSE type");
        TEST_ASSERT(response_empty_value_msg.unit_type() == unit_type4, "Test 4.2: Empty value RESPONSE unit_type");
        TEST_ASSERT(response_empty_value_msg.value() == nullptr, "Test 4.3: Empty value RESPONSE value ptr is null");
        TEST_ASSERT(response_empty_value_msg.value_size() == 0, "Test 4.4: Empty value RESPONSE value_size is 0");

        const void* serialized_data_ev = response_empty_value_msg.data();
        unsigned int serialized_size_ev = response_empty_value_msg.size();
        Message deserialized_ev_msg = Message::deserialize(serialized_data_ev, serialized_size_ev);

        TEST_ASSERT(deserialized_ev_msg.message_type() == Message::Type::RESPONSE, "Test 4.5: Deser Empty value RESPONSE type");
        TEST_ASSERT(deserialized_ev_msg.unit_type() == unit_type4, "Test 4.6: Deser Empty value RESPONSE unit_type");
        TEST_ASSERT(deserialized_ev_msg.value() == nullptr, "Test 4.7: Deser Empty value RESPONSE value ptr is null");
        TEST_ASSERT(deserialized_ev_msg.value_size() == 0, "Test 4.8: Deser Empty value RESPONSE value_size is 0");
    }

    std::cout << "P3 Message tests passed successfully!" << std::endl;
    return 0;
} 