#include "component.h"


class BasicProducer : public Component {
    public:
        BasicProducer(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
            : Component(vehicle, name, protocol, address),
              _interest_periods(),
              _current_period(0) {
            // Constructor implementation
        }

        ~BasicProducer() override = default;

    protected:
        std::atomic<bool> _new_period; // Flag for producer thread
        std::vector<std::uint32_t> _interest_periods;
        std::atomic<std::uint32_t> _current_period; // Period in microseconds

        void update_period() {
            std::uint32_t gcd = _interest_periods[0];
            for (std::size_t i = 1; i < _interest_periods.size(); ++i) {
                std::uint32_t period = _interest_periods[i];
                while (period != 0) {
                    std::uint32_t temp = period;
                    period = gcd % period;
                    gcd = temp;
                }
            }
            
            if (_current_period.load(std::memory_order_acquire) != gcd) {
                _current_period.store(gcd, std::memory_order_release);
                _new_period.store(true, std::memory_order_release);
            }
        }

        void reschedule() {
            std::uint32_t current_period = _current_period.load(std::memory_order_acquire);

            struct sched_attr attr;
            attr.size = sizeof(attr);
            attr.sched_policy = SCHED_DEADLINE;
            attr.sched_flags = 0;
            // Update SCHED_DEADLINE parameters based on current period
            attr.sched_runtime = current_period * 500; // 50% of period in ns
            attr.sched_deadline = current_period * 1000; // Period in ns
            attr.sched_period = current_period * 1000; // Period in ns

            int result = sched_setattr(0, &attr, 0);
            if (result < 0) {
                throw std::runtime_error("Failed to reschedule component thread for " + getName());
            }

            _new_period.store(false, std::memory_order_release);
        }

        void run() override {
            db<BasicProducer>(INF) << "[BasicProducer]" << getName() << " main thread started.\n";
            while (running()) {
                try {
                    std::uint8_t data[TheMessage::MAX_SIZE];
                    TheMessage::Interest interest;
                    int bytes_received = receive(data, sizeof(data));

                    if (static_cast<long unsigned int>(bytes_received) < sizeof(interest)) {
                        db<BasicProducer>(ERR) << "[BasicProducer]" << getName() << " received message too small.\n";
                        continue;
                    }
                    memcpy(&interest, data, sizeof(interest));

                    if (interest.message_type == TheMessage::MessageType::INTEREST) {
                        db<BasicProducer>(INF) << "[BasicProducer]" << getName() << " received interest message.\n";
                        _interest_periods.push_back(interest.period);
                        update_period();
                    } 
                } catch (const std::exception& e) {
                    db<BasicProducer>(ERR) << "[BasicProducer]" << getName() << " exception: " << e.what() << "\n";
                    stop();
                    std::runtime_error("Producer thread interrupted due to exeption.");
                }
            }
            db<BasicProducer>(INF) << "[BasicProducer]" << getName() << " main thread terminated.\n";
        }
};