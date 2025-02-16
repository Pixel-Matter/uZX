#include <JuceHeader.h>

// This is equivalent to JUCE_MAIN_FUNCTION from juce_Application.h
// but for console application
int main(int argc, char* argv[]) {
    DBG("WTF!");
    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory("MoToolPlugins");
    DBG("WTF this!");
    return 0;
}