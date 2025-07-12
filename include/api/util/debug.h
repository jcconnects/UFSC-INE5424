#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <pthread.h>
#include <string>
#include <chrono>
#include <iomanip>
#include <sys/syscall.h>
#include <unistd.h>
#include "api/traits.h"

class Debug
{
    public:
        Debug() : _message_started(false) {}
        ~Debug() {
            // Flush any remaining content when object is destroyed
            if (_message_started) {
                flush_buffer();
            }
        }
        
        template<typename T>
        Debug & operator<<(T p) {
            get_buffer() << p;
            return *this;
        }

        struct Begl {};
        struct Err {};

        Debug & operator<<(const Begl & begl) { 
            // Start a new log entry - flush any existing buffer and prepare for new message
            if (_message_started) {
                flush_buffer();
            }
            _message_started = true;
            get_buffer() << get_timestamp_and_thread() << " ";
            return *this; 
        }

        Debug & operator<<(const Err & err) { 
            get_buffer() << "[ERROR] ";
            return *this; 
        }

        // Flush buffer when newline is encountered
        Debug & operator<<(const char* str) {
            std::string s(str);
            get_buffer() << s;
            if (s.find('\n') != std::string::npos) {
                flush_buffer();
                _message_started = false;
            }
            return *this;
        }

        static void set_log_file(const std::string &filename) {
            pthread_mutex_lock(&_global_mutex);
            _file_stream = std::make_unique<std::ofstream>(filename, std::ios::out);
            if (!_file_stream->is_open()) {
                std::cerr << "Erro ao abrir arquivo de log: " << filename << std::endl;
                _stream = &std::cout; // Fallback to cout
            } else {
                _stream = _file_stream.get();
            }
            pthread_mutex_unlock(&_global_mutex);
        }

        static void close_log_file() {
            pthread_mutex_lock(&_global_mutex);
            if (_file_stream && _file_stream->is_open()) {
                _file_stream->close();
                _stream = &std::cout; // Fallback to cout
            }
            pthread_mutex_unlock(&_global_mutex);
        }
    
        static Debug & instance() {
            static Debug debug;
            return debug;
        }

        static void init() {
            pthread_mutex_init(&_global_mutex, nullptr);
        }
        
        static void cleanup() {
            pthread_mutex_destroy(&_global_mutex);
        }

    private:
        void flush_buffer() {
            std::string content = get_buffer().str();
            if (!content.empty()) {
                pthread_mutex_lock(&_global_mutex);
                if (_stream) {
                    (*_stream) << content << std::flush;
                }
                pthread_mutex_unlock(&_global_mutex);
                get_buffer().str(""); // Clear buffer
                get_buffer().clear(); // Clear error flags
            }
        }

        static std::ostringstream& get_buffer() {
            thread_local std::ostringstream buffer;
            return buffer;
        }

        static std::string get_timestamp_and_thread() {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            std::ostringstream oss;
            oss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S");
            oss << "." << std::setfill('0') << std::setw(3) << ms.count();
            oss << " T" << syscall(SYS_gettid) << "]";
            return oss.str();
        }

        static inline std::unique_ptr<std::ofstream> _file_stream;
        static inline std::ostream* _stream = &std::cout;
        static inline pthread_mutex_t _global_mutex = PTHREAD_MUTEX_INITIALIZER;
        
        bool _message_started; // Track if a message has been started

    public:
        static inline Begl begl;
        static inline Err error;
};

class Null_Debug
{
public:
    template<typename T>
    Null_Debug & operator<<(const T & o) { return *this; }

    template<typename T>
    Null_Debug & operator<<(const T * o) { return *this; }
    
    Null_Debug & operator<<(const char* str) { return *this; }
    
    struct Begl {};
    struct Err {};
    Null_Debug & operator<<(const Begl & begl) { return *this; }
    Null_Debug & operator<<(const Err & err) { return *this; }
};

template<bool debugged>
class Select_Debug: public Debug {};
template<>
class Select_Debug<false>: public Null_Debug {};

// Error
enum Debug_Error {ERR = 1};

template<typename T>
inline Select_Debug<(Traits<T>::debugged && Traits<Debug>::error)>
db(Debug_Error l)
{
    Select_Debug<(Traits<T>::debugged && Traits<Debug>::error)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << Debug::error;
    return debug_instance;
}

template<typename T1, typename T2>
inline Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::error)>
db(Debug_Error l)
{
    Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::error)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << Debug::error;
    return debug_instance;
}

// Warning
enum Debug_Warning {WRN = 2};

template<typename T>
inline Select_Debug<(Traits<T>::debugged && Traits<Debug>::warning)>
db(Debug_Warning l)
{
    Select_Debug<(Traits<T>::debugged && Traits<Debug>::warning)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << "[WARNING] ";
    return debug_instance;
}

template<typename T1, typename T2>
inline Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::warning)>
db(Debug_Warning l)
{
    Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::warning)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << "[WARNING] ";
    return debug_instance;
}

// Info
enum Debug_Info {INF = 3};

template<typename T>
inline Select_Debug<(Traits<T>::debugged && Traits<Debug>::info)>
db(Debug_Info l)
{
    Select_Debug<(Traits<T>::debugged && Traits<Debug>::info)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << "[INFO] ";
    return debug_instance;
}

template<typename T1, typename T2>
inline Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::info)>
db(Debug_Info l)
{
    Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::info)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << "[INFO] ";
    return debug_instance;
}

// Trace
enum Debug_Trace {TRC = 4};

template<typename T>
inline Select_Debug<(Traits<T>::debugged && Traits<Debug>::trace)>
db(Debug_Trace l)
{
    Select_Debug<(Traits<T>::debugged && Traits<Debug>::trace)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << "[TRACE] ";
    return debug_instance;
}

template<typename T1, typename T2>
inline Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::trace)>
db(Debug_Trace l)
{
    Select_Debug<((Traits<T1>::debugged || Traits<T2>::debugged) && Traits<Debug>::trace)> debug_instance;
    debug_instance << Debug::begl;
    debug_instance << "[TRACE] ";
    return debug_instance;
}

// Call this at program start
class DebugInitializer {
public:
    DebugInitializer() {
        Debug::init();
    }
    ~DebugInitializer() {
        Debug::cleanup();
    }
};

static DebugInitializer debugInitializer;

#endif // DEBUG_H
