#pragma once

#include <JuceHeader.h>

namespace MoTool {

//==============================================================================
/**
    Runs a program whose standard input is a pipe written from here, as juce::ChildProcess can't,
    with its output and errors going to a log file. Kills the program if it still runs when this goes.
*/
class PipedProcess
{
public:
    PipedProcess() = default;
    ~PipedProcess();

    /** command[0] is the program's full path; no shell is involved. False if it couldn't be started. */
    bool start(const juce::StringArray& command, const juce::File& log);

    /** Blocks until all is written; false once the program has stopped reading. */
    bool write(const void* data, size_t numBytes);

    /** Ends the program's input, as the end of its stream. */
    void closeInput();

    /** True until the program has exited. */
    bool isRunning();

    /** Its exit code once it has exited; -1 before, or if it was killed. */
    int getExitCode() const noexcept { return exitCode; }

private:
#if JUCE_WINDOWS
    void* process = nullptr;
    void* input = nullptr;
#else
    int pid = -1;
    int input = -1;
#endif
    int exitCode = -1;

    JUCE_DECLARE_NON_COPYABLE(PipedProcess)
};

}  // namespace MoTool
