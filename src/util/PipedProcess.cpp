#include "PipedProcess.h"

#include <algorithm>
#include <string>
#include <vector>

#if JUCE_WINDOWS
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#else
#    include <cerrno>
#    include <csignal>

#    include <fcntl.h>
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>
extern char** environ;
#endif

namespace MoTool {

#if JUCE_WINDOWS

namespace {
// As the C runtime splits a command line: quotes around an argument with spaces, tabs or quotes,
// and the backslashes before a quote doubled
std::wstring quoted(const juce::String& arg) {
    const std::wstring s = arg.toWideCharPointer();
    if (!s.empty() && s.find_first_of(L" \t\"") == std::wstring::npos)
        return s;

    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (auto c : s) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        out.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, L'\\');
    return out + L"\"";
}
}  // namespace

bool PipedProcess::start(const juce::StringArray& command, const juce::File& log) {
    jassert(process == nullptr && !command.isEmpty());

    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &inheritable, 1 << 20))
        return false;
    SetHandleInformation(writeEnd, HANDLE_FLAG_INHERIT, 0);  // only the program's end goes to it

    HANDLE logFile = CreateFileW(log.getFullPathName().toWideCharPointer(),
                                 GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_DELETE,
                                 &inheritable,
                                 CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL,
                                 nullptr);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = readEnd;
    startup.hStdOutput = startup.hStdError = logFile;

    std::wstring commandLine;
    for (const auto& arg : command)
        commandLine += (commandLine.empty() ? L"" : L" ") + quoted(arg);

    PROCESS_INFORMATION info{};
    const bool started = CreateProcessW(
        nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);
    CloseHandle(readEnd);
    if (logFile != INVALID_HANDLE_VALUE)
        CloseHandle(logFile);

    if (!started) {
        CloseHandle(writeEnd);
        return false;
    }

    CloseHandle(info.hThread);
    process = info.hProcess;
    input = writeEnd;
    return true;
}

bool PipedProcess::write(const void* data, size_t numBytes) {
    auto* bytes = static_cast<const char*>(data);
    while (numBytes > 0) {
        DWORD written = 0;
        if (input == nullptr
            || !WriteFile(input, bytes, (DWORD) std::min(numBytes, (size_t) 1 << 30), &written, nullptr))
            return false;
        bytes += written;
        numBytes -= written;
    }
    return true;
}

void PipedProcess::closeInput() {
    if (input != nullptr) {
        CloseHandle(input);
        input = nullptr;
    }
}

bool PipedProcess::isRunning() {
    if (process == nullptr)
        return false;
    if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
        return true;

    DWORD code = 0;
    exitCode = GetExitCodeProcess(process, &code) ? (int) code : -1;
    CloseHandle(process);
    process = nullptr;
    return false;
}

PipedProcess::~PipedProcess() {
    closeInput();
    if (process != nullptr) {
        TerminateProcess(process, 1);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }
}

#else

bool PipedProcess::start(const juce::StringArray& command, const juce::File& log) {
    jassert(pid < 0 && !command.isEmpty());

    // Writing to a program that has quit would kill this app with SIGPIPE; ignored, write() fails instead
    std::signal(SIGPIPE, SIG_IGN);

    int fds[2];
    if (::pipe(fds) != 0)
        return false;
    ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);  // this end stays here, out of any program started later

    std::vector<std::string> strings;
    for (const auto& arg : command)
        strings.push_back(arg.toStdString());
    std::vector<char*> argv;
    for (auto& s : strings)
        argv.push_back(s.data());
    argv.push_back(nullptr);
    const auto logPath = log.getFullPathName().toStdString();

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[0], STDIN_FILENO);
    if (fds[0] != STDIN_FILENO)
        posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);

    pid_t child = -1;
    const int error = ::posix_spawn(&child, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(fds[0]);

    if (error != 0) {
        ::close(fds[1]);
        return false;
    }

    pid = child;
    input = fds[1];
    return true;
}

bool PipedProcess::write(const void* data, size_t numBytes) {
    auto* bytes = static_cast<const char*>(data);
    while (numBytes > 0) {
        const auto written = ::write(input, bytes, numBytes);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return false;
        bytes += written;
        numBytes -= (size_t) written;
    }
    return true;
}

void PipedProcess::closeInput() {
    if (input >= 0) {
        ::close(input);
        input = -1;
    }
}

bool PipedProcess::isRunning() {
    if (pid < 0)
        return false;

    int status = 0;
    pid_t result;
    do
        result = ::waitpid(pid, &status, WNOHANG);
    while (result < 0 && errno == EINTR);

    if (result == 0)
        return true;

    exitCode = result == pid && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    pid = -1;
    return false;
}

PipedProcess::~PipedProcess() {
    closeInput();
    if (pid > 0) {
        ::kill(pid, SIGKILL);
        ::waitpid(pid, nullptr, 0);
    }
}

#endif

}  // namespace MoTool
