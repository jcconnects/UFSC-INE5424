#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>

namespace test {

class Logger {
private:
    std::ofstream logFile;
    std::string testName;
    bool verbose;

public:
    Logger(const std::string& name, bool verboseOutput = false) : testName(name), verbose(verboseOutput) {
        // Create logs directory if it doesn't exist
        mkdir("tests/logs", 0777);
        
        // Open log file
        std::string logPath = "tests/logs/" + testName + ".log";
        logFile.open(logPath);
        
        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logPath << std::endl;
            exit(1);
        }
        
        log("Test started: " + testName);
    }
    
    ~Logger() {
        log("Test completed: " + testName);
        logFile.close();
    }
    
    void log(const std::string& message) {
        logFile << message << std::endl;
        if (verbose) {
            std::cout << "[" << testName << "] " << message << std::endl;
        }
    }
    
    void assertThat(bool condition, const std::string& message) {
        if (condition) {
            log("PASS: " + message);
        } else {
            log("FAIL: " + message);
            std::cout << "[" << testName << "] FAIL: " << message << std::endl;
            exit(1);
        }
    }
};

} // namespace test

#define TEST_INIT(name) \
    test::Logger logger(name); \
    std::cout << "Running test: " << name << std::endl;

#define TEST_ASSERT(condition, message) \
    logger.log("Asserting: " + std::string(message)); \
    logger.assertThat((condition), message);

#define TEST_LOG(message) \
    logger.log(message);

#endif // TEST_UTILS_H 