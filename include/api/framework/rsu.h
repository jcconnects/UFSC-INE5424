#ifndef RSU_H
#define RSU_H

#include <chrono>
#include <atomic>
#include <vector>
#include <cstdint>

#include "api/framework/network.h"
#include "api/framework/periodicThread.h"
#include "api/util/debug.h"
#include "api/framework/clock.h"

class RSU {
public:
    typedef Network::Communicator Communicator;
    typedef Network::Protocol Protocol;
    typedef Protocol::Address Address;
    typedef Network::Message Message;
    typedef Message::Unit Unit;

    /**
     * @brief Construct a new RSU object
     * @param rsu_id Unique identifier for this RSU (used for MAC address generation)
     * @param unit The unit type that this RSU will broadcast
     * @param period The broadcasting period in milliseconds
     * @param data Optional data payload for the RESPONSE message
     * @param data_size Size of the data payload
     */
    RSU(unsigned int rsu_id, Unit unit, std::chrono::milliseconds period, 
        const void* data = nullptr, unsigned int data_size = 0);
    
    /**
     * @brief Destroy the RSU object
     */
    ~RSU();

    /**
     * @brief Start the RSU broadcasting
     */
    void start();

    /**
     * @brief Stop the RSU broadcasting
     */
    void stop();

    /**
     * @brief Check if RSU is currently running
     * @return true if running, false otherwise
     */
    bool running() const;

    /**
     * @brief Get the RSU's network address
     * @return const Address& RSU's address
     */
    const Address& address() const;

    /**
     * @brief Get the unit type being broadcast
     * @return Unit The unit type
     */
    Unit unit() const;

    /**
     * @brief Get the broadcasting period
     * @return std::chrono::milliseconds The current period
     */
    std::chrono::milliseconds period() const;

    /**
     * @brief Adjust the broadcasting period
     * @param new_period New period in milliseconds
     */
    void adjust_period(std::chrono::milliseconds new_period);

    /**
     * @brief Send a single broadcast message (called by periodic thread)
     */
    void broadcast();

private:
    // RSU configuration
    unsigned int _rsu_id;
    Unit _unit;
    std::chrono::milliseconds _period;
    std::vector<std::uint8_t> _data;
    
    // Network stack
    Network* _network;
    Communicator* _comm;
    
    // Periodic broadcasting (order matters for initialization)
    std::atomic<bool> _running;
    Periodic_Thread<RSU> _periodic_thread;
};

/********** RSU Implementation **********/

RSU::RSU(unsigned int rsu_id, Unit unit, std::chrono::milliseconds period, 
         const void* data, unsigned int data_size) 
    : _rsu_id(rsu_id), _unit(unit), _period(period), _running(false),
      _periodic_thread(this, &RSU::broadcast) {
    
    db<RSU>(TRC) << "RSU::RSU() called with id=" << rsu_id << ", unit=" << unit << ", period=" << period.count() << "ms\n";

    // Store broadcast data if provided
    if (data && data_size > 0) {
        _data.resize(data_size);
        std::memcpy(_data.data(), data, data_size);
    }

    // Create network stack with RSU ID for MAC address generation
    _network = new Network(_rsu_id);
    
    // Create communicator using the network's protocol channel and RSU's address
    Address rsu_addr(_network->address(), _rsu_id);
    _comm = new Communicator(_network->channel(), rsu_addr);

    db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " initialized with address " << rsu_addr.to_string() << "\n";

    // Set self ID for the Clock instance
    LeaderIdType self_leader_id = static_cast<LeaderIdType>(rsu_addr.paddr().bytes[5]);
    if (self_leader_id != INVALID_LEADER_ID) {
        Clock::getInstance().setSelfId(self_leader_id);
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " registered self_id " << self_leader_id << " with Clock.\n";
        Clock::getInstance().activate(nullptr); // Activate clock to evaluate leader state
    } else {
        db<RSU>(WRN) << "[RSU] RSU " << _rsu_id << " has an INVALID_LEADER_ID based on its MAC. Clock self_id not set.\n";
    }
}

RSU::~RSU() {
    db<RSU>(TRC) << "RSU::~RSU() called!\n";
    
    stop();
    
    delete _comm;
    delete _network;
    
    db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " destroyed\n";
}

void RSU::start() {
    db<RSU>(TRC) << "RSU::start() called!\n";
    
    if (!_running.load()) {
        _running.store(true);
        _periodic_thread.start(static_cast<std::int64_t>(_period.count()) * 1000);
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " started broadcasting every " << _period.count() << "ms\n";
    }
}

void RSU::stop() {
    db<RSU>(TRC) << "RSU::stop() called!\n";
    
    if (_running.load(std::memory_order_acquire)) {
        // Step 1: Signal threads to stop
        _running.store(false, std::memory_order_release);
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " stopping broadcasting\n";
        
        // Step 2: Release communicator to unblock any waiting operations
        _comm->release();
        
        // Step 3: Stop periodic thread and wait for it to finish
        _periodic_thread.join();
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " periodic thread stopped\n";
        
        // Step 4: Stop network stack after threads are fully stopped
        _network->stop();
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " stopped broadcasting\n";
    }
}

bool RSU::running() const {
    return _running.load(std::memory_order_acquire);
}

const RSU::Address& RSU::address() const {
    return _comm->address();
}

RSU::Unit RSU::unit() const {
    return _unit;
}

std::chrono::milliseconds RSU::period() const {
    return _period;
}

void RSU::adjust_period(std::chrono::milliseconds new_period) {
    db<RSU>(TRC) << "RSU::adjust_period() called with new_period=" << new_period.count() << "ms\n";
    
    _period = new_period;
    if (running()) {
        _periodic_thread.adjust_period(static_cast<std::int64_t>(new_period.count()) * 1000);
    }
    
    db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " period adjusted to " << new_period.count() << "ms\n";
}

void RSU::broadcast() {
    if (!running()) {
        return;
    }

    db<RSU>(TRC) << "RSU::broadcast() called!\n";

    // Create RESPONSE message
    Message* msg;
    if (_data.empty()) {
        // No data payload
        msg = new Message(Message::Type::RESPONSE, address(), _unit);
    } else {
        // With data payload
        msg = new Message(Message::Type::RESPONSE, address(), _unit, 
                         Message::ZERO, _data.data(), _data.size());
    }

    db<RSU>(TRC) << "[RSU] RSU " << _rsu_id << " broadcasting RESPONSE for unit " << _unit 
               << " with data size " << _data.size() << "\n";

    // Send broadcast message
    bool sent = _comm->send(msg);
    
    if (sent) {
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " broadcast RESPONSE for unit " << _unit << "\n";
    } else {
        db<RSU>(WRN) << "[RSU] RSU " << _rsu_id << " failed to broadcast RESPONSE for unit " << _unit << "\n";
    }

    delete msg;
}

#endif // RSU_H 