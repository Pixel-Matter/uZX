#include <JuceHeader.h>

// This is equivalent to JUCE_MAIN_FUNCTION from juce_Application.h
// but for console application
int main([[maybe_unused]] int argc, char* argv[]) {
    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory("MoToolPlugins");
    return 0;
}