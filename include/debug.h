#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <pthread.h>
#include "traits.h"

class Debug
{
    public:
        Debug() { 
            // pthread_mutex_init(&_mutex, nullptr); // Member mutex removed
        }
        
        ~Debug() { 
            // pthread_mutex_destroy(&_mutex); // Member mutex removed
            // Flush any remaining buffered output
            flush();
        }
        
        template<typename T>
        Debug & operator<<(T p) {
            _buffer << p;
            return *this;
        }

        struct Begl {};
        struct Err {};

        Debug & operator<<(const Begl & begl) { return *this; }

        Debug & operator<<(const Err & err) { _error = true; return *this; }

        // Add flush method to write buffered content atomically
        void flush() {
            if (!_buffer.str().empty()) {
                pthread_mutex_lock(&_stream_access_mutex);
                if (_stream) {
                    (*_stream) << _buffer.str() << std::flush;
                }
                pthread_mutex_unlock(&_stream_access_mutex);
                _buffer.str(""); // Clear the buffer
            }
        }

        static void set_log_file(const std::string &filename) {
            pthread_mutex_lock(&_file_mutex); // Lock during file operations
            _file_stream = std::make_unique<std::ofstream>(filename, std::ios::out);
            if (!_file_stream->is_open()) {
                std::cerr << "Erro ao abrir arquivo de log: " << filename << std::endl;
            } else {
                _stream = _file_stream.get(); // Redireciona a saída para o arquivo
            }
            pthread_mutex_unlock(&_file_mutex);
        }

        static void close_log_file() {
            pthread_mutex_lock(&_file_mutex); // Lock during file operations
            if (_file_stream && _file_stream->is_open()) {
                _file_stream->close();
            }
            pthread_mutex_unlock(&_file_mutex);
        }
    
        static Debug & instance() {
            static Debug debug;
            return debug;
        }

        static void init() {
            pthread_mutex_init(&_file_mutex, nullptr); 
            // If _stream_access_mutex is initialized with PTHREAD_MUTEX_INITIALIZER, explicit init might not be needed here
            // Or, ensure it's initialized: pthread_mutex_init(&_stream_access_mutex, nullptr);
        }
        
        static void cleanup() {
            pthread_mutex_destroy(&_file_mutex);
            // Corresponding destroy if init is used: pthread_mutex_destroy(&_stream_access_mutex);
        }

    private:
        static std::unique_ptr<std::ofstream> _file_stream;
        static std::ostream* _stream;
        volatile bool _error;
        static pthread_mutex_t _file_mutex; // Mutex for file operations
        // pthread_mutex_t _mutex; // Member mutex removed
        static pthread_mutex_t _stream_access_mutex; // Static mutex for all stream output
        std::stringstream _buffer; // Add buffer for atomic writes

    public:
        static Begl begl;
        static Err error;
};

class Null_Debug
{
public:
    template<typename T>
    Null_Debug & operator<<(const T & o) { return *this; }

    template<typename T>
    Null_Debug & operator<<(const T * o) { return *this; }
};

template<bool debugged>
class Select_Debug: public Debug {};
template<>
class Select_Debug<false>: public Null_Debug {};

// Error
enum Debug_Error {ERR = 1};

template<typename T>
inline Debug& db(Debug_Error l)
{
    static Debug debug;
    debug << Debug::begl;
    debug << Debug::error;
    debug.flush(); // Flush after the debug statement
    return debug;
}

template<typename T1, typename T2>
inline Debug& db(Debug_Error l)
{
    static Debug debug;
    debug << Debug::begl;
    debug << Debug::error;
    debug.flush(); // Flush after the debug statement
    return debug;
}

// Warning
enum Debug_Warning {WRN = 2};

template<typename T>
inline Debug& db(Debug_Warning l)
{
    static Debug debug;
    debug << Debug::begl;
    debug.flush(); // Flush after the debug statement
    return debug;
}

template<typename T1, typename T2>
inline Debug& db(Debug_Warning l)
{
    static Debug debug;
    debug << Debug::begl;
    debug.flush(); // Flush after the debug statement
    return debug;
}

// Info
enum Debug_Info {INF = 3};

template<typename T>
inline Debug& db(Debug_Info l)
{
    static Debug debug;
    debug << Debug::begl;
    debug.flush(); // Flush after the debug statement
    return debug;
}

template<typename T1, typename T2>
inline Debug& db(Debug_Info l)
{
    static Debug debug;
    debug << Debug::begl;
    debug.flush(); // Flush after the debug statement
    return debug;
}

// Trace
enum Debug_Trace {TRC = 4};

template<typename T>
inline Debug& db(Debug_Trace l)
{
    static Debug debug;
    debug << Debug::begl;
    debug.flush(); // Flush after the debug statement
    return debug;
}

template<typename T1, typename T2>
inline Debug& db(Debug_Trace l)
{
    static Debug debug;
    debug << Debug::begl;
    debug.flush(); // Flush after the debug statement
    return debug;
}

// Initialize static members
Debug::Begl Debug::begl;
Debug::Err Debug::error;
std::unique_ptr<std::ofstream> Debug::_file_stream;
std::ostream* Debug::_stream = &std::cout;
pthread_mutex_t Debug::_file_mutex = PTHREAD_MUTEX_INITIALIZER; // Initialize static mutex
pthread_mutex_t Debug::_stream_access_mutex = PTHREAD_MUTEX_INITIALIZER; // Initialize new static mutex

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
