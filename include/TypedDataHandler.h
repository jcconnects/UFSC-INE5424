#ifndef TYPED_DATA_HANDLER_H
#define TYPED_DATA_HANDLER_H

#include <functional>
#include <pthread.h>
#include <atomic>

#include "observer.h" // For Concurrent_Observer
#include "message.h"  // For Message class
#include "teds.h"     // For DataTypeId

// Forward declaration
class Component;

/**
 * @brief Handler for type-specific data processing in the P3 publish-subscribe system
 * 
 * TypedDataHandler is responsible for processing messages of a specific DataTypeId.
 * Each handler runs its own processing thread which waits for messages to arrive 
 * and then executes the provided callback function.
 */
class TypedDataHandler : public Concurrent_Observer<Message, DataTypeId> {
public:
    /**
     * @brief Construct a new Typed Data Handler object
     * 
     * @param type The DataTypeId this handler is interested in
     * @param callback_func The callback function to execute when a message arrives
     * @param parent_component The Component this handler belongs to
     * @param observed The observable object to attach to (typically Component's _internal_typed_observed)
     */
    TypedDataHandler(DataTypeId type, 
                     std::function<void(const Message&)> callback_func, 
                     Component* parent_component,
                     Conditionally_Data_Observed<Message, DataTypeId>* observed)
    : _callback_func(callback_func),
      _parent_component(parent_component),
      _handler_running(false),
      _thread_id(0),
      _type(type),
      _observed(observed)
    {
        // Attach to the observed object with our specific data type
        if (_observed) {
            _observed->attach(this, _type);
        }
    }

    /**
     * @brief Destructor - ensures the handler is detached from observers
     */
    ~TypedDataHandler() {
        // Stop the processing thread if it's running
        if (_handler_running.load()) {
            stop_processing_thread();
        }

        // Detach from the observed object
        if (_observed) {
            _observed->detach(this, _type);
        }
    }

    /**
     * @brief Start the handler's processing thread
     */
    void start_processing_thread() {
        // Only start if not already running
        if (!_handler_running.load()) {
            _handler_running.store(true);
            
            // Create the processing thread
            int result = pthread_create(&_thread_id, nullptr, 
                                        &TypedDataHandler::processing_loop_entry, 
                                        static_cast<void*>(this));
            
            if (result != 0) {
                // Thread creation failed
                _handler_running.store(false);
                _thread_id = 0;
            }
        }
    }

    /**
     * @brief Signal the processing thread to stop
     * 
     * This doesn't join the thread, just signals it to exit.
     * The actual thread join should be done by the Component.
     */
    void stop_processing_thread() {
        if (_handler_running.load()) {
            // Signal the thread to stop
            _handler_running.store(false);
            
            // Post to the semaphore to unblock updated() if it's waiting
            Concurrent_Observer<Message, DataTypeId>::wakeup();
        }
    }

    /**
     * @brief Get the thread ID for thread joining
     * 
     * @return pthread_t The thread ID of the processing thread
     */
    pthread_t get_thread_id() const {
        return _thread_id;
    }

    /**
     * @brief Get the DataTypeId this handler is processing
     * 
     * @return DataTypeId The type this handler is associated with
     */
    DataTypeId get_handled_type() const { return _type; }

private:
    /**
     * @brief Entry point for the processing thread
     * 
     * @param arg A pointer to the TypedDataHandler instance
     * @return void* Always returns nullptr
     */
    static void* processing_loop_entry(void* arg) {
        TypedDataHandler* handler = static_cast<TypedDataHandler*>(arg);
        if (handler) {
            handler->processing_loop();
        }
        return nullptr;
    }

    /**
     * @brief Main processing loop
     * 
     * Waits for messages, processes them, and passes them to the callback.
     */
    void processing_loop() {
        while (_handler_running.load()) {
            // Wait for a message to be delivered
            Message* msg = Concurrent_Observer<Message, DataTypeId>::updated();
            
            // Check if we should still be running
            if (!_handler_running.load()) {
                // If we're not running but received a message, clean it up
                if (msg) {
                    delete msg;
                }
                break;
            }
            
            // Process the message if it's valid
            if (msg) {
                // Execute the callback with the message
                _callback_func(*msg);
                
                // Clean up the heap-allocated message
                delete msg;
            }
        }
    }

    /**
     * @brief The callback function to execute on received messages
     */
    std::function<void(const Message&)> _callback_func;

    /**
     * @brief Reference to the parent component
     */
    Component* _parent_component;

    /**
     * @brief Flag indicating if the processing thread should continue running
     */
    std::atomic<bool> _handler_running;

    /**
     * @brief ID of the processing thread
     */
    pthread_t _thread_id;

    /**
     * @brief The DataTypeId this handler is interested in
     */
    DataTypeId _type;

    /**
     * @brief The observed object this handler is attached to
     */
    Conditionally_Data_Observed<Message, DataTypeId>* _observed;
};

#endif // TYPED_DATA_HANDLER_H 