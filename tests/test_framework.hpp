#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace smart::test {

class Registry {
public:
    using TestFunc = void (*)();

    static Registry &instance()
    {
        static Registry registry;
        return registry;
    }

    void add(TestFunc func, std::string name)
    {
        tests_.push_back({std::move(name), func});
    }

    int run_all() const
    {
        int failures = 0;
        for (const auto &test : tests_) {
            try {
                test.func();
                std::cout << ".";
            } catch (const std::exception &ex) {
                ++failures;
                std::cerr << "\n[FAIL] " << test.name << " - " << ex.what() << "\n";
            } catch (...) {
                ++failures;
                std::cerr << "\n[FAIL] " << test.name << " - unknown exception\n";
            }
        }
        if (failures == 0) {
            std::cout << "\nAll tests passed.\n";
        } else {
            std::cout << "\n" << failures << " test(s) failed.\n";
        }
        return failures;
    }

private:
    struct Test {
        std::string name;
        TestFunc func;
    };

    std::vector<Test> tests_;
};

class TestRegistrar {
public:
    TestRegistrar(Registry::TestFunc func, std::string name)
    {
        Registry::instance().add(func, std::move(name));
    }
};

class AssertionError : public std::runtime_error {
public:
    explicit AssertionError(std::string message)
        : std::runtime_error(std::move(message))
    {
    }
};

inline void assert_true(bool condition, std::string message)
{
    if (!condition) {
        throw AssertionError(std::move(message));
    }
}

inline void assert_equal(double lhs, double rhs, double epsilon, std::string message)
{
    if (std::abs(lhs - rhs) > epsilon) {
        throw AssertionError(std::move(message));
    }
}

inline void assert_equal(long long lhs, long long rhs, std::string message)
{
    if (lhs != rhs) {
        throw AssertionError(std::move(message));
    }
}

inline void assert_equal(const std::string &lhs, const std::string &rhs, std::string message)
{
    if (lhs != rhs) {
        throw AssertionError(std::move(message));
    }
}

} // namespace smart::test

#define SMART_CONCAT_INNER(a, b) a##b
#define SMART_CONCAT(a, b) SMART_CONCAT_INNER(a, b)

#define SMART_TEST_CASE(name)                                                                 \
    void name();                                                                               \
    static ::smart::test::TestRegistrar SMART_CONCAT(__smart_test_registrar_, __LINE__)(name, #name); \
    void name()

#define SMART_REQUIRE(cond)                                                                    \
    ::smart::test::assert_true(static_cast<bool>(cond), [&]() {                                \
        std::ostringstream oss;                                                               \
        oss << "Requirement failed: " << #cond << " in " << __FILE__ << ":" << __LINE__;         \
        return oss.str();                                                                      \
    }())

#define SMART_REQUIRE_EQ(lhs, rhs)                                                             \
    do {                                                                                       \
        const auto __smart_lhs = (lhs);                                                        \
        const auto __smart_rhs = (rhs);                                                        \
        ::smart::test::assert_true(__smart_lhs == __smart_rhs, [&]() {                         \
            std::ostringstream oss;                                                            \
            oss << "Expected equality: " << #lhs << " == " << #rhs << " ("                    \
                << __smart_lhs << ", " << __smart_rhs << ") at " << __FILE__ << ":"          \
                << __LINE__;                                                                   \
            return oss.str();                                                                  \
        }());                                                                                  \
    } while (false)

#define SMART_REQUIRE_NE(lhs, rhs)                                                             \
    do {                                                                                       \
        const auto __smart_lhs = (lhs);                                                        \
        const auto __smart_rhs = (rhs);                                                        \
        ::smart::test::assert_true(__smart_lhs != __smart_rhs, [&]() {                         \
            std::ostringstream oss;                                                            \
            oss << "Expected inequality: " << #lhs << " != " << #rhs << " at " << __FILE__    \
                << ":" << __LINE__;                                                            \
            return oss.str();                                                                  \
        }());                                                                                  \
    } while (false)
