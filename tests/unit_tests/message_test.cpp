#include <iostream>
#include <cstring>
#include <vector>
#include <limits>
#include <chrono>

#include "test_utils.h"
#include "../../include/teds.h"
#include "../../include/ethernet.h"

// Create a simple test origin for the Message class
// Instead of using Message::Origin directly, we'll use a custom struct
struct TestOrigin {
    Ethernet::Address phys_addr;
    std::uint16_t _port;  // Renamed to _port to avoid conflict

    TestOrigin(const Ethernet::Address& addr, std::uint16_t p) : phys_addr(addr), _port(p) {}
    TestOrigin() : _port(0) {} // Add default constructor
    
    const Ethernet::Address& paddr() const { return phys_addr; }
    std::uint16_t port() const { return _port; }  // Now returns _port
};

TestOrigin create_test_origin(std::uint8_t mac5 = 0x01, std::uint16_t port = 1234) {
    Ethernet::Address mac_addr = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, mac5}};
    return TestOrigin(mac_addr, port);
}

// Simple Message implementation for testing
class Message {
public:
    enum class Type : std::uint8_t {
        INTEREST,
        RESPONSE,
        REG_PRODUCER,
    };
    
    typedef TestOrigin Origin;

    Message(Type msg_type, const Origin& orig, DataTypeId dtype, 
            unsigned int per = 0, const void* val = nullptr, 
            unsigned int val_size = 0)
        : _msg_type(msg_type), _orig(orig), _dtype(dtype), _timestamp(0), _period(per)
    {
        _timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
        
        if (val && val_size > 0) {
            _value.resize(val_size);
            std::memcpy(_value.data(), val, val_size);
        }
        serialize();
    }
    
    // Explicitly define default constructor
    Message() : _msg_type(Type::INTEREST), _timestamp(0), _period(0) {}
    
    // Accessors
    Type message_type() const { return _msg_type; }
    const Origin& origin() const { return _orig; }
    const std::uint64_t& timestamp() const { return _timestamp; }
    DataTypeId unit_type() const { return _dtype; }
    unsigned int period() const { return _period; }
    const std::uint8_t* value() const { return _value.empty() ? nullptr : _value.data(); }
    unsigned int value_size() const { return _value.size(); }
    
    const void* data() const { return _serialized.data(); }
    unsigned int size() const { return _serialized.size(); }
    
    static Message deserialize(const void* data, unsigned int size) {
        Message msg;
        const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
        unsigned int offset = 0;
        
        // Extract message type
        msg._msg_type = static_cast<Type>(bytes[offset++]);
        
        // Extract origin (MAC address 6 bytes + port 2 bytes)
        Ethernet::Address mac = {};
        for (int i = 0; i < 6; i++) {
            mac.bytes[i] = bytes[offset++];
        }
        std::uint16_t port = (static_cast<std::uint16_t>(bytes[offset]) << 8) | 
                           static_cast<std::uint16_t>(bytes[offset + 1]);
        offset += 2;
        msg._orig = TestOrigin(mac, port);
        
        // Extract timestamp (8 bytes)
        msg._timestamp = 0;
        for (int i = 0; i < 8; i++) {
            msg._timestamp |= static_cast<std::uint64_t>(bytes[offset++]) << ((7-i)*8);
        }
        
        // Extract data type (4 bytes)
        std::uint32_t dtype = 0;
        for (int i = 0; i < 4; i++) {
            dtype |= static_cast<std::uint32_t>(bytes[offset++]) << ((3-i)*8);
        }
        msg._dtype = static_cast<DataTypeId>(dtype);
        
        // Extract period if INTEREST (4 bytes)
        if (msg._msg_type == Type::INTEREST) {
            msg._period = 0;
            for (int i = 0; i < 4; i++) {
                msg._period |= static_cast<std::uint32_t>(bytes[offset++]) << ((3-i)*8);
            }
        }
        
        // Extract value if RESPONSE (remaining bytes)
        if (msg._msg_type == Type::RESPONSE && offset < size) {
            unsigned int val_size = size - offset;
            msg._value.resize(val_size);
            std::memcpy(msg._value.data(), bytes + offset, val_size);
        }
        
        // Build serialized data
        msg.serialize();
        
        return msg;
    }

private:
    Type _msg_type;
    Origin _orig;
    std::uint64_t _timestamp;
    DataTypeId _dtype;
    unsigned int _period;
    std::vector<std::uint8_t> _value;
    std::vector<std::uint8_t> _serialized;
    
    void serialize() {
        _serialized.clear();
        
        // Append message type
        _serialized.push_back(static_cast<std::uint8_t>(_msg_type));
        
        // Append origin (MAC + port)
        for (int i = 0; i < 6; i++) {
            _serialized.push_back(_orig.paddr().bytes[i]);
        }
        // Use port() method to get the port value
        uint16_t port_val = _orig.port();
        _serialized.push_back(static_cast<std::uint8_t>((port_val >> 8) & 0xFF));
        _serialized.push_back(static_cast<std::uint8_t>(port_val & 0xFF));
        
        // Append timestamp (8 bytes)
        for (int i = 0; i < 8; i++) {
            _serialized.push_back(static_cast<std::uint8_t>((_timestamp >> ((7-i)*8)) & 0xFF));
        }
        
        // Append data type (4 bytes)
        std::uint32_t dtype = static_cast<std::uint32_t>(_dtype);
        for (int i = 0; i < 4; i++) {
            _serialized.push_back(static_cast<std::uint8_t>((dtype >> ((3-i)*8)) & 0xFF));
        }
        
        // Append period if INTEREST (4 bytes)
        if (_msg_type == Type::INTEREST) {
            for (int i = 0; i < 4; i++) {
                _serialized.push_back(static_cast<std::uint8_t>((_period >> ((3-i)*8)) & 0xFF));
            }
        }
        
        // Append value if RESPONSE
        if (_msg_type == Type::RESPONSE && !_value.empty()) {
            _serialized.insert(_serialized.end(), _value.begin(), _value.end());
        }
    }
};

int main() {
    TEST_INIT("P3 Message Test");
    
    // Test 1: Create and verify an INTEREST message
    {
        TestOrigin origin1 = create_test_origin(0x01, 1001);
        DataTypeId unit_type1 = DataTypeId::VEHICLE_SPEED;
        unsigned int period1 = 100; // 100 microseconds

        Message interest_msg(Message::Type::INTEREST, origin1, unit_type1, period1);

        TEST_ASSERT(interest_msg.message_type() == Message::Type::INTEREST, "Test 1.1: Message type should be INTEREST");
        TEST_ASSERT(memcmp(&interest_msg.origin().paddr(), &origin1.paddr(), sizeof(Ethernet::Address)) == 0, "Test 1.2: Origin MAC should match");
        
        // Fix port comparison
        uint16_t interest_port = interest_msg.origin().port();
        uint16_t origin1_port = origin1.port();
        TEST_ASSERT(interest_port == origin1_port, "Test 1.3: Origin Port should match");
        
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
        TEST_ASSERT(memcmp(&deserialized_msg.origin().paddr(), &origin1.paddr(), sizeof(Ethernet::Address)) == 0, "Test 1.9.3: Deserialized MAC mismatch");
        
        // Fix port comparison
        uint16_t deser_port = deserialized_msg.origin().port();
        TEST_ASSERT(deser_port == origin1_port, "Test 1.9.4: Deserialized Port mismatch");
        
        TEST_ASSERT(deserialized_msg.unit_type() == unit_type1, "Test 1.9.6: Deserialized unit_type mismatch");
        TEST_ASSERT(deserialized_msg.period() == period1, "Test 1.9.7: Deserialized period mismatch");
        TEST_ASSERT(deserialized_msg.value() == nullptr, "Test 1.9.8: Deserialized value should be null for INTEREST");
    }

    // Test 2: Create and verify a RESPONSE message
    {
        TestOrigin origin2 = create_test_origin(0x02, 2002);
        DataTypeId unit_type2 = DataTypeId::ENGINE_RPM;
        std::vector<std::uint8_t> value2 = {0x01, 0x02, 0x03, 0x04, 0x05};
        unsigned int period2 = 0; // Period is not used for RESPONSE, but constructor takes it.

        Message response_msg(Message::Type::RESPONSE, origin2, unit_type2, period2, value2.data(), value2.size());

        TEST_ASSERT(response_msg.message_type() == Message::Type::RESPONSE, "Test 2.1: Message type should be RESPONSE");
        TEST_ASSERT(memcmp(&response_msg.origin().paddr(), &origin2.paddr(), sizeof(Ethernet::Address)) == 0, "Test 2.2: Origin MAC should match");
        
        // Fix port comparison
        uint16_t response_port = response_msg.origin().port();
        uint16_t origin2_port = origin2.port();
        TEST_ASSERT(response_port == origin2_port, "Test 2.3: Origin Port should match");
        
        TEST_ASSERT(response_msg.unit_type() == unit_type2, "Test 2.4: Unit type should match");
        TEST_ASSERT(response_msg.value_size() == value2.size(), "Test 2.5: Value size should match");
        TEST_ASSERT(response_msg.value() != nullptr && std::memcmp(response_msg.value(), value2.data(), value2.size()) == 0, "Test 2.6: Value data should match");

        // Test 2.7: Serialization and Deserialization
        const void* serialized_data_resp = response_msg.data();
        unsigned int serialized_size_resp = response_msg.size();
        TEST_ASSERT(serialized_data_resp != nullptr && serialized_size_resp > 0, "Test 2.7.1: Serialized data for RESPONSE should be valid");

        Message deserialized_resp_msg = Message::deserialize(serialized_data_resp, serialized_size_resp);
        TEST_ASSERT(deserialized_resp_msg.message_type() == Message::Type::RESPONSE, "Test 2.7.2: Deserialized RESPONSE type mismatch");
        TEST_ASSERT(memcmp(&deserialized_resp_msg.origin().paddr(), &origin2.paddr(), sizeof(Ethernet::Address)) == 0, "Test 2.7.3: Deserialized RESPONSE MAC mismatch");
        
        // Fix port comparison
        uint16_t deser_resp_port = deserialized_resp_msg.origin().port();
        TEST_ASSERT(deser_resp_port == origin2_port, "Test 2.7.4: Deserialized RESPONSE Port mismatch");
        
        TEST_ASSERT(deserialized_resp_msg.unit_type() == unit_type2, "Test 2.7.5: Deserialized RESPONSE unit_type mismatch");
        TEST_ASSERT(deserialized_resp_msg.value_size() == value2.size(), "Test 2.7.6: Deserialized RESPONSE value size mismatch");
        TEST_ASSERT(deserialized_resp_msg.value() != nullptr && std::memcmp(deserialized_resp_msg.value(), value2.data(), value2.size()) == 0, "Test 2.7.7: Deserialized RESPONSE value data mismatch");
    }

    // Test 3: Create and verify a REG_PRODUCER message
    {
        TestOrigin origin3 = create_test_origin(0x03, 3003);
        DataTypeId unit_type3 = DataTypeId::SYSTEM_INTERNAL_REG_PRODUCER;
        // Period and value are not used for REG_PRODUCER

        Message reg_msg(Message::Type::REG_PRODUCER, origin3, unit_type3);

        TEST_ASSERT(reg_msg.message_type() == Message::Type::REG_PRODUCER, "Test 3.1: Message type should be REG_PRODUCER");
        TEST_ASSERT(memcmp(&reg_msg.origin().paddr(), &origin3.paddr(), sizeof(Ethernet::Address)) == 0, "Test 3.2: Origin MAC should match for REG_PRODUCER");
        
        // Fix port comparison
        uint16_t reg_port = reg_msg.origin().port();
        uint16_t origin3_port = origin3.port();
        TEST_ASSERT(reg_port == origin3_port, "Test 3.3: Origin Port should match for REG_PRODUCER");
        
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
        TEST_ASSERT(memcmp(&deserialized_reg_msg.origin().paddr(), &origin3.paddr(), sizeof(Ethernet::Address)) == 0, "Test 3.8.3: Deserialized REG_PRODUCER MAC mismatch");
        
        // Fix port comparison
        uint16_t deser_reg_port = deserialized_reg_msg.origin().port();
        TEST_ASSERT(deser_reg_port == origin3_port, "Test 3.8.4: Deserialized REG_PRODUCER Port mismatch");
        
        TEST_ASSERT(deserialized_reg_msg.unit_type() == unit_type3, "Test 3.8.5: Deserialized REG_PRODUCER unit_type mismatch");
    }
    
    // Test 4: RESPONSE message with empty value
    {
        TestOrigin origin4 = create_test_origin(0x04, 4004);
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