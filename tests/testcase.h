#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// ANSI color codes for test output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BLUE    "\033[34m"

/**
 * @brief Check if colors should be used in terminal output
 * @return true if colors are supported and should be used, false otherwise
 */
bool shouldUseColors() {
    // Check if NO_COLOR environment variable is set (standard way to disable colors)
    const char* no_color = std::getenv("NO_COLOR");
    if (no_color && std::strlen(no_color) > 0) {
        return false;
    }
    
    // Check if output is being redirected (not a terminal)
    if (!isatty(STDOUT_FILENO)) {
        return false;
    }
    
    // Check TERM environment variable for color support
    const char* term = std::getenv("TERM");
    if (!term) {
        return false;
    }
    
    // Common terminal types that support colors
    return (std::strstr(term, "color") != nullptr || 
            std::strstr(term, "xterm") != nullptr ||
            std::strstr(term, "screen") != nullptr ||
            std::strstr(term, "tmux") != nullptr ||
            std::strcmp(term, "linux") == 0);
}

/**
 * @brief Get color code if colors are enabled, empty string otherwise
 * @param color_code The ANSI color code to use
 * @return Color code string or empty string
 */
std::string getColor(const char* color_code) {
    static bool use_colors = shouldUseColors();
    return use_colors ? color_code : "";
}

#define DEFINE_TEST(name) registerTest(#name, [this]() { this->name(); });

class TestCase {
    public:
        virtual ~TestCase() = default;

        static void setUpClass() {};
        static void tearDownClass() {};

        virtual void setUp() = 0;
        virtual void tearDown() = 0;

        void run();

        template<typename T, typename U>
        void assert_equal(const T& expected, const U& actual, const std::string& msg = "");

        void assert_true(bool expr, const std::string& msg = "");

        void assert_false(bool expr, const std::string& msg = "");

        template<typename ExceptionType, typename Func>
        void assert_throw(Func func, const std::string& msg = "");
    
    protected:
        std::vector<std::pair<std::string, std::function<void()>>> tests;
    
        void registerTest(const std::string& name, std::function<void()> func) {
            tests.emplace_back(name, func);
        }

};

void TestCase::run() {
    for (const auto& [name, test] : tests) {
        std::cout << getColor(COLOR_BLUE) << "[ RUN      ] " << getColor(COLOR_RESET) << name << std::endl;
        
        setUp();
        try {
            test();
            std::cout << getColor(COLOR_GREEN) << "[     OK   ] " << getColor(COLOR_RESET) << name << std::endl;
        } catch (const std::exception& e) {
            std::cout << getColor(COLOR_RED) << "[  FAILED  ] " << getColor(COLOR_RESET) << name << ": " << e.what() << std::endl;
        }
        tearDown();
    }
}

template<typename T, typename U>
void TestCase::assert_equal(const T& expected, const U& actual, const std::string& msg) {
    if (!(expected == actual)) {
        
        std::ostringstream oss;
        oss << "Assertion failed: expected [" << expected << "] but got [" << actual << "]";
        std::string full_msg = oss.str();
        if (!msg.empty()) full_msg += " - " + msg;
        throw std::runtime_error(full_msg);
    }
}

void TestCase::assert_true(bool expr, const std::string& msg) {
    if (!expr) {
        std::string full_msg = "Assertion failed: expected true";
        if (!msg.empty()) full_msg += " - " + msg;
        throw std::runtime_error(full_msg);
    }
}

void TestCase::assert_false(bool expr, const std::string& msg) {
    if (expr) {
        std::string full_msg = "Assertion failed: expected false";
        if (!msg.empty()) full_msg += " - " + msg;
        throw std::runtime_error(full_msg);
    }
}

template<typename ExceptionType, typename Func>
void TestCase::assert_throw(Func func, const std::string& msg) {
    bool thrown = false;
    try {
        func();
    } catch (const ExceptionType&) {
        thrown = true;
    } catch (...) {
        throw std::runtime_error("Assertion failed: thrown exception is not of expected type");
    }
    if (!thrown) {
        std::string full_msg = "Assertion failed: exception was not thrown";
        if (!msg.empty()) full_msg += " - " + msg;
        throw std::runtime_error(full_msg);
    }
}