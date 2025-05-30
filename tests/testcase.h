#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <string>

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
        std::cout << "[ RUN      ] " << name << std::endl;
        
        setUp();
        try {
            test();
            std::cout << "[     OK   ] " << name << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[  FAILED  ] " << name << ": " << e.what() << std::endl;
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