#ifndef RSU_H
#define RSU_H

#include <chrono>
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstring>

#include "api/framework/network.h"
#include "api/framework/periodicThread.h"
#include "api/util/debug.h"
#include "api/framework/clock.h"
#include "api/framework/leaderKeyStorage.h"


class RSU {
public:
    typedef Network::Communicator Communicator;
    typedef Network::Protocol Protocol;
    typedef Protocol::Address Address;
    typedef Network::Message Message;
    typedef Message::Unit Unit;

    RSU(unsigned int rsu_id, Unit unit, std::chrono::milliseconds period,
        double x = 0.0, double y = 0.0, double radius = 400.0, const void* data = nullptr, unsigned int data_size = 0);
    ~RSU();
    void start();
    void stop();
    bool running() const;
    const Address& address() const;
    Unit unit() const;
    std::chrono::milliseconds period() const;
    void adjust_period(std::chrono::milliseconds new_period);
    void broadcast();
    
    // Neighbor RSU management (directly in Protocol)
    void initialize_neighbor_rsus(); // Initialize neighbor RSUs directly in Protocol

private:
    // RSU configuration
    unsigned int _rsu_id;
    Unit _unit;
    std::chrono::milliseconds _period;
    std::vector<std::uint8_t> _data;
    MacKeyType _rsu_key;
    double _x;
    double _y;
    double _radius;
    
    // Note: Neighbor RSU information is now stored directly in Protocol's structure
    // No need for RSU-level duplicate storage
    
    // Network stack
    Network* _network;
    Communicator* _comm;
    
    // Periodic broadcasting (order matters for initialization)
    std::atomic<bool> _running;
    Periodic_Thread<RSU> _periodic_thread;
    
};

/********** RSU Implementation **********/


/**
 * @brief Construct a new RSU object
 * @param rsu_id Unique identifier for this RSU (used for MAC address generation)
 * @param unit The unit type that this RSU will broadcast
 * @param period The broadcasting period in milliseconds
 * @param data Optional data payload for the RESPONSE message
 * @param data_size Size of the data payload
 */
inline RSU::RSU(unsigned int rsu_id, Unit unit, std::chrono::milliseconds period, 
         double x, double y, double radius, const void* data, unsigned int data_size) 
    : _rsu_id(rsu_id), _unit(unit), _period(period), _x(x), _y(y), _radius(radius), _running(false),
      _periodic_thread(this, &RSU::broadcast) {
    
    db<RSU>(TRC) << "RSU::RSU() called with id=" << rsu_id << ", unit=" << unit << ", period=" << period.count() << "ms\n";

    // Store broadcast data if provided
    if (data && data_size > 0) {
        _data.resize(data_size);
        std::memcpy(_data.data(), data, data_size);
    }

    // Create network stack with RSU ID for MAC address generation
    _network = new Network(_rsu_id, Network::EntityType::RSU);
    
    // Set NIC radius to match RSU's configured radius for consistent collision domain filtering
    _network->channel()->setRadius(_radius);
    
    // Create communicator using the network's protocol channel and RSU's address
    Address rsu_addr(_network->address(), _rsu_id);
    _comm = new Communicator(_network->channel(), rsu_addr);

    _rsu_key.fill(0);
    // Use RSU ID in the key to make it unique
    _rsu_key[0] = (rsu_id >> 8) & 0xFF;
    _rsu_key[1] = rsu_id & 0xFF;
    _rsu_key[2] = 0xAA; // Marker for RSU
    _rsu_key[3] = 0xBB;

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

    // Set RSU key in LeaderKeyStorage so it can be used for MAC verification
    LeaderKeyStorage::getInstance().setLeaderId(rsu_addr.paddr());
    LeaderKeyStorage::getInstance().setGroupMacKey(_rsu_key);
    db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " registered key in LeaderKeyStorage for MAC verification.\n";
    
    // Initialize neighbor RSUs
    initialize_neighbor_rsus();
}

/**
 * @brief Destroy the RSU object
 */
inline RSU::~RSU() {
    db<RSU>(TRC) << "RSU::~RSU() called!\n";
    
    stop();
    
    delete _comm;
    delete _network;
    
    db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " destroyed\n";
}

/**
 * @brief Start the RSU broadcasting
 */
inline void RSU::start() {
    db<RSU>(TRC) << "RSU::start() called!\n";
    
    if (!_running.load()) {
        _running.store(true);
        _periodic_thread.start(static_cast<std::int64_t>(_period.count()) * 1000);
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " started broadcasting every " << _period.count() << "ms\n";
    }
}

/**
 * @brief Stop the RSU broadcasting
 */
inline void RSU::stop() {
    db<RSU>(TRC) << "RSU::stop() called!\n";
    
    if (_running.load(std::memory_order_acquire)) {
        // Step 1: Signal RSU that it should stop its operations
        _running.store(false, std::memory_order_release);
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " stopping broadcasting\n";
        
        // Step 2: Stop periodic thread and wait for it to finish.
        // This ensures RSU::broadcast() (and thus _comm->send()) is no longer called.
        _periodic_thread.join();
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " periodic thread stopped\n";
        
        // Step 3: Release communicator now that the thread is guaranteed to not use it.
        _comm->release();
        
        // Step 4: Stop network stack after threads are fully stopped
        _network->stop();
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " stopped broadcasting\n";
    }
}

/**
 * @brief Check if the RSU is running
 * @return bool True if running, false otherwise
 */
inline bool RSU::running() const {
    return _running.load(std::memory_order_acquire);
}

/**
 * @brief Get the RSU's network address
 * @return const Address& RSU's address
 */
inline const RSU::Address& RSU::address() const {
    return _comm->address();
}

/**
 * @brief Get the unit type being broadcast
 * @return Unit The unit type
 */
inline RSU::Unit RSU::unit() const {
    return _unit;
}

/**
 * @brief Get the broadcasting period
 * @return std::chrono::milliseconds The current period
 */
inline std::chrono::milliseconds RSU::period() const {
    return _period;
}

/**
 * @brief Adjust the broadcasting period
 * @param new_period New period in milliseconds
 */
inline void RSU::adjust_period(std::chrono::milliseconds new_period) {
    db<RSU>(TRC) << "RSU::adjust_period() called with new_period=" << new_period.count() << "ms\n";
    
    _period = new_period;
    if (running()) {
        _periodic_thread.adjust_period(static_cast<std::int64_t>(new_period.count()) * 1000);
    }
    
    db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " period adjusted to " << new_period.count() << "ms\n";
}

/**
 * @brief Send a single broadcast message (called by periodic thread)
 */
inline void RSU::broadcast() {
    if (!running()) {
        return;
    }

    db<RSU>(TRC) << "RSU::broadcast() called!\n";

    // Create STATUS message
    Message* msg;
    
    // Calculate payload size first
    unsigned int payload_size = sizeof(_x) + sizeof(_y) + sizeof(_radius) + sizeof(_rsu_key) + _data.size();
    std::vector<uint8_t> payload(payload_size);  // Properly size the vector
    
    unsigned int offset = 0;
    std::memcpy(payload.data() + offset, &_x, sizeof(_x));
    offset += sizeof(_x);
    std::memcpy(payload.data() + offset, &_y, sizeof(_y));
    offset += sizeof(_y);
    std::memcpy(payload.data() + offset, &_radius, sizeof(_radius));
    offset += sizeof(_radius);
    std::memcpy(payload.data() + offset, &_rsu_key, sizeof(_rsu_key));
    offset += sizeof(_rsu_key);

    if (!_data.empty()) {
        std::memcpy(payload.data() + offset, _data.data(), _data.size());
    } 

    // With data payload
    msg = new Message(Message::Type::STATUS, address(), _unit, 
                        Message::ZERO, payload.data(), payload.size());

    db<RSU>(TRC) << "[RSU] RSU " << _rsu_id << " broadcasting STATUS for unit " << _unit 
               << " with data size " << payload.size() << "\n";

    // Send broadcast message
    bool sent = _comm->send(msg);
    
    if (sent) {
        db<RSU>(INF) << "[RSU] RSU " << _rsu_id << " broadcast STATUS for unit " << _unit << "\n";
    } else {
        db<RSU>(WRN) << "[RSU] RSU " << _rsu_id << " failed to broadcast STATUS for unit " << _unit << "\n";
    }

    delete msg;
}



/**
 * @brief Initialize neighbor RSUs directly in Protocol structure
 * In a real system, this would be loaded from configuration
 */
inline void RSU::initialize_neighbor_rsus() {
    if (!_network || !_network->channel()) {
        db<RSU>(WRN) << "[RSU] Cannot initialize protocol neighbors - no network or protocol\n";
        return;
    }
    
    db<RSU>(INF) << "[RSU] Initializing neighbor RSUs directly in Protocol for RSU " << _rsu_id << "\n";
    
    // Clear existing neighbors in protocol first
    _network->channel()->clear_neighbor_rsus();
    
    // For demo purposes, let's assume we know about other RSUs
    // In practice, this would be loaded from configuration or discovered
    
    // Example: If this is RSU 1000, add RSU 1001, 1002, etc.
    for (unsigned int neighbor_id = 1000; neighbor_id < 1010; ++neighbor_id) {
        if (neighbor_id == _rsu_id) continue; // Skip self
        
        // Create neighbor RSU key (similar to how we create our own key)
        MacKeyType neighbor_key;
        neighbor_key.fill(0);
        neighbor_key[0] = (neighbor_id >> 8) & 0xFF;
        neighbor_key[1] = neighbor_id & 0xFF;
        neighbor_key[2] = 0xAA; // Marker for RSU
        neighbor_key[3] = 0xBB;
        
        // Create neighbor RSU address
        Ethernet::Address neighbor_mac;
        neighbor_mac.bytes[0] = 0x02; // Locally administered
        neighbor_mac.bytes[1] = 0x00;
        neighbor_mac.bytes[2] = 0x00;
        neighbor_mac.bytes[3] = 0x00;
        neighbor_mac.bytes[4] = (neighbor_id >> 8) & 0xFF;
        neighbor_mac.bytes[5] = neighbor_id & 0xFF;
        
        Address neighbor_address(neighbor_mac, neighbor_id);
        
        // Add directly to Protocol structure
        _network->channel()->add_neighbor_rsu(neighbor_id, neighbor_key, neighbor_address);
    }
    
    db<RSU>(INF) << "[RSU] Successfully initialized neighbor RSUs directly in Protocol\n";
}

#endif // RSU_H 