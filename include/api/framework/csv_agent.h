#ifndef CSV_AGENT_HPP
#define CSV_AGENT_HPP

#include <cstring>
#include <cstdint>
#include "api/framework/agent.h"

class CSVAgent : public Agent {
    public:
        CSVAgent(CAN* bus, const std::string& name, Unit unit, Type type, Address address,
                 DataProducer producer, ResponseHandler handler, std::unique_ptr<ComponentData> data, bool external);
        ~CSVAgent();
    
    protected:
        void reply(Unit unit);

};

inline CSVAgent::CSVAgent(CAN* bus, const std::string& name, Unit unit, Type type, Address address,
                 DataProducer producer, ResponseHandler handler, std::unique_ptr<ComponentData> data, bool external) :
                 Agent(bus, name, unit, type, address, producer, handler, std::move(data), external) {}

inline CSVAgent::~CSVAgent() {}

inline void CSVAgent::reply(Unit unit) {
    // Safety check: don't reply if agent is being destroyed
    if (!running()) {
        return;
    }

    if (!thread_running()) {
        return;
    }
    
    // Final safety check before calling function pointer
    if (!running()) {
        return;
    }
    
    // CRITICAL: Call function pointer instead of virtual method
    // This eliminates the race condition that caused "pure virtual method called"
    Value value = get(unit);
    
    // Extract timestamp from the beginning of the CSV data (first 8 bytes)
    if (value.size() >= sizeof(std::uint64_t)) {
        std::uint64_t csv_timestamp;
        std::memcpy(&csv_timestamp, value.data(), sizeof(std::uint64_t));
        
        // Create message with CSV data (without timestamp) and set timestamp separately
        const std::uint8_t* csv_data = value.data() + sizeof(std::uint64_t);
        std::size_t csv_data_size = value.size() - sizeof(std::uint64_t);
        
        Message msg(Message::Type::RESPONSE, address(), unit, Microseconds::zero(), csv_data, csv_data_size);
        msg.timestamp(Microseconds(csv_timestamp));
        msg.external(external());
        
        // Log sent message to CSV
        log_message(msg, "SEND");

        _can->send(&msg);
    } 
}

#endif