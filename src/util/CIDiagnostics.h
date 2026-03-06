#pragma once

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <exception>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <crtdbg.h>
#endif

namespace MoTool::TestHelpers {

namespace detail {

inline void ciAtexitHandler() {
    fprintf(stderr, "[CI] atexit handler called\n");
    fflush(stderr);
}

inline void ciAbortHandler(int sig) {
    fprintf(stderr, "[CI] Signal %d (SIGABRT) received\n", sig);
    fflush(stderr);
}

#ifdef _MSC_VER
inline LONG WINAPI ciUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
    fprintf(stderr, "[CI] Unhandled SEH exception! Code: 0x%08lX\n", code);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

}  // namespace detail

inline void installCIDiagnostics() {
    std::atexit(detail::ciAtexitHandler);
    std::signal(SIGABRT, detail::ciAbortHandler);

    std::set_terminate([]() {
        fprintf(stderr, "[CI] std::terminate called!\n");
        if (auto eptr = std::current_exception()) {
            try { std::rethrow_exception(eptr); }
            catch (const std::exception& e) {
                fprintf(stderr, "[CI] exception: %s\n", e.what());
            }
            catch (...) {
                fprintf(stderr, "[CI] unknown exception\n");
            }
        }
        fflush(stderr);
        std::abort();
    });

#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);

    SetUnhandledExceptionFilter(detail::ciUnhandledExceptionFilter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}

}  // namespace MoTool::TestHelpers
