// Studio Voice Pro — micro-cadre de test maison.
// Aucune dépendance externe (pas de FetchContent, pas de Catch2/GoogleTest) :
// le même binaire sert de lanceur, filtré par suite, et ctest inscrit une
// entrée par suite. Un cas échoué fait sortir le binaire avec le code 1.
#pragma once

#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace svptest {

struct Case {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& cases()
{
    static std::vector<Case> registry;
    return registry;
}

inline int& failureCount()
{
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> fn)
    {
        cases().push_back({suite, name, std::move(fn)});
    }
};

inline void reportFailure(const char* file, int line, const std::string& text)
{
    ++failureCount();
    std::printf("      ECHEC %s:%d : %s\n", file, line, text.c_str());
}

inline int run(const std::string& suiteFilter)
{
    int executed = 0;
    for (const Case& testCase : cases()) {
        if (!suiteFilter.empty() && testCase.suite != suiteFilter)
            continue;
        ++executed;
        const int before = failureCount();
        std::printf("  cas %s :: %s\n", testCase.suite.c_str(), testCase.name.c_str());
        try {
            testCase.fn();
        } catch (const std::exception& e) {
            reportFailure("<exception>", 0, std::string("exception : ") + e.what());
        } catch (...) {
            reportFailure("<exception>", 0, "exception inconnue");
        }
        if (failureCount() == before)
            std::printf("      ok\n");
    }
    std::printf("suite « %s » : %d cas exécuté(s), %d échec(s)\n",
                suiteFilter.empty() ? "(toutes)" : suiteFilter.c_str(),
                executed,
                failureCount());
    return (failureCount() == 0) ? 0 : 1;
}

} // namespace svptest

#define SVP_TEST(suiteName, caseName)                                                             \
    static void svp_test_##suiteName##_##caseName();                                              \
    static const svptest::Registrar svp_registrar_##suiteName##_##caseName(                       \
        #suiteName, #caseName, svp_test_##suiteName##_##caseName);                                \
    static void svp_test_##suiteName##_##caseName()

#define SVP_CHECK(condition)                                                                      \
    do {                                                                                          \
        if (!(condition))                                                                         \
            svptest::reportFailure(__FILE__, __LINE__, "condition fausse : " #condition);         \
    } while (false)

#define SVP_CHECK_NEAR(actual, expected, tolerance)                                               \
    do {                                                                                          \
        const double svp_a = static_cast<double>(actual);                                         \
        const double svp_b = static_cast<double>(expected);                                       \
        const double svp_t = static_cast<double>(tolerance);                                      \
        if (!(std::fabs(svp_a - svp_b) <= svp_t)) {                                               \
            char svp_buffer[320];                                                                 \
            std::snprintf(svp_buffer, sizeof(svp_buffer),                                         \
                          "%s vaut %.10g, attendu %.10g (tolérance %.3g)",                        \
                          #actual, svp_a, svp_b, svp_t);                                          \
            svptest::reportFailure(__FILE__, __LINE__, svp_buffer);                               \
        }                                                                                         \
    } while (false)
