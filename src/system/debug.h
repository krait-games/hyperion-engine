//
// Created by emd22 on 2022-02-14.
//

#ifndef HYPERION_DEBUG_H
#define HYPERION_DEBUG_H

#include <ostream>
#include <string>
#include <iostream>

#include <csignal>


#ifndef HYPERION_BUILD_RELEASE

#define HYP_DEBUG_MODE 1

#if HYP_DEBUG_MODE
#define HYP_ENABLE_BREAKPOINTS 1
#endif

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
#define HYP_DEBUG_FUNC_SHORT __FUNCTION__
#define HYP_DEBUG_FUNC       __PRETTY_FUNCTION__
#define HYP_DEBUG_LINE       (__LINE__)
#ifdef HYP_ENABLE_BREAKPOINTS
#define HYP_BREAKPOINT       (raise(SIGTERM))
#endif

#elif defined(_MSC_VER)
#define HYP_DEBUG_FUNC_SHORT __FUNCTION__
#define HYP_DEBUG_FUNC       __FUNCSIG__
#define HYP_DEBUG_LINE       (__LINE__)
#ifdef HYP_ENABLE_BREAKPOINTS
#define HYP_BREAKPOINT       (__debugbreak())
#endif
#else
#define HYP_DEBUG_FUNC_SHORT ""
#define HYP_DEBUG_FUNC ""
#define HYP_DEBUG_LINE (0)

#endif
#endif /* HYPERION_BUILD_RELEASE */

#if !HYP_ENABLE_BREAKPOINTS
#define HYP_BREAKPOINT       (void(0))
#endif

enum class LogType : int {
    Info,
    Warn,
    Error,
    Fatal,
    Debug,

    RenInfo,
    RenWarn,
    RenError,
    RenDebug,
};

void DebugSetOutputStream(FILE *stream);

/* To keep final executable size down and avoid extra calculations
 * the release build will have function naming disabled. */

#define DebugLogRaw(type, ...) \
    DebugLog_(type, nullptr, 0, __VA_ARGS__)

#ifdef HYPERION_BUILD_RELEASE
#define DebugLog(type, ...) \
    DebugLog_(type, __VA_ARGS__)

void DebugLog_(LogType type, const char *fmt, ...);
#else
//#define DebugLog(type, fmt) DebugLog(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, fmt)
#define DebugLog(type, ...) \
    DebugLog_(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, __VA_ARGS__)

void DebugLog_(LogType type, const char *callee, uint32_t line, const char *fmt, ...);
#endif

#define DebugLogAssertion(level, cond) \
    do { \
        const char *s = "*** assertion failed: (" #cond ") ***"; \
        DebugLog(level, "%s", s); \
    } while (0)

#define DebugLogAssertionMsg(level, cond, msg, ...) \
    do { \
        DebugLog(level, "*** assertion failed: (" #cond ") ***\n\t" msg "\n", __VA_ARGS__); \
    } while (0)

#define AssertOrElse(level, cond, stmt) \
    do { \
        if (!(cond)) { \
            DebugLogAssertion(level, cond); \
            { stmt; } \
        } \
    } while (0)

#define AssertOrElseMsg(level, cond, stmt, msg, ...) \
    do { \
        if (!(cond)) { \
            DebugLogAssertionMsg(level, cond, msg, __VA_ARGS__); \
            { stmt; } \
        } \
    } while (0)

#define AssertThrow(cond) AssertOrElse(LogType::Error, cond, throw ::std::runtime_error("Assertion failed"))
#define AssertThrowMsg(cond, msg, ...) AssertOrElseMsg(LogType::Error, cond, throw ::std::runtime_error("Assertion failed"), msg, __VA_ARGS__)
#define AssertSoft(cond) AssertOrElse(LogType::Warn, cond, return)
#define AssertSoftMsg(cond, msg, ...) AssertOrElseMsg(LogType::Warn, cond, return, msg, __VA_ARGS__)
#define AssertReturn(cond, value) AssertOrElse(LogType::Warn, cond, return (value))
#define AssertReturnMsg(cond, value, msg, ...) AssertOrElseMsg(LogType::Warn, cond, return value, msg, __VA_ARGS__)
#define AssertBreak(cond) AssertOrElse(LogType::Warn, cond, break)
#define AssertBreakMsg(cond, msg, ...) AssertOrElseMsg(LogType::Warn, cond, break, msg, __VA_ARGS__)
#define AssertContinue(cond) AssertOrElse(LogType::Warn, cond, continue)
#define AssertContinueMsg(cond, msg, ...) AssertOrElseMsg(LogType::Warn, cond, continue, msg, __VA_ARGS__)
#define AssertExit(cond) AssertOrElse(LogType::Fatal, cond, exit(1))
#define AssertExitMsg(cond, msg, ...) AssertOrElseMsg(LogType::Fatal, cond, exit(1), msg, __VA_ARGS__)

/* Deprecated */
#define not_implemented AssertExitMsg(0, "Not implemented")
#define function_body_not_implemented { not_implemented; }
#define unexpected_value(value) AssertExitMsg(0, "%s", #value ": unexpected value")
#define unexpected_value_msg(value, msg) AssertExitMsg(0, "%s", #value ": " #msg)

#endif //HYPERION_DEBUG_H
