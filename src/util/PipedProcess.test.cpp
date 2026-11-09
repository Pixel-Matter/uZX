#include "PipedProcess.h"

#include <JuceHeader.h>

#include <vector>

namespace MoTool::Tests {

class PipedProcessTest : public juce::UnitTest
{
public:
    PipedProcessTest() : UnitTest("PipedProcess", "MoTool") {}

    void runTest() override {
#if JUCE_WINDOWS
        const auto system = juce::SystemStats::getEnvironmentVariable("SystemRoot", "C:\\Windows") + "\\System32\\";
        const juce::StringArray sort{system + "sort.exe"}, exit3{system + "cmd.exe", "/c", "exit 3"};
#else
        const juce::StringArray sort{"/usr/bin/sort"}, exit3{"/bin/sh", "-c", "exit 3"};
#endif

        beginTest("The program reads what is written, and what it prints goes to the log");
        {
            juce::TemporaryFile log{".log"};
            PipedProcess process;
            expect(process.start(sort, log.getFile()));
            const char text[] = "pear\napple\n";
            expect(process.write(text, sizeof(text) - 1));
            process.closeInput();
            expect(waitForExit(process));
            expectEquals(process.getExitCode(), 0);
            expectEquals(juce::StringArray::fromLines(log.getFile().loadFileAsString().trim()).joinIntoString(" "),
                         juce::String("apple pear"));
        }

        beginTest("Writing to a program that has quit fails, rather than taking the app down");
        {
            juce::TemporaryFile log{".log"};
            PipedProcess process;
            expect(process.start(exit3, log.getFile()));
            expect(waitForExit(process));
            expectEquals(process.getExitCode(), 3);
            std::vector<char> frame(1 << 20);
            expect(!process.write(frame.data(), frame.size()));
        }
    }

private:
    static bool waitForExit(PipedProcess& process) {
        for (int i = 0; i < 500; ++i) {
            if (!process.isRunning())
                return true;
            juce::Thread::sleep(10);
        }
        return false;
    }
};

static PipedProcessTest pipedProcessTest;

}  // namespace MoTool::Tests
