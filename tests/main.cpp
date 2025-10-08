#include <JuceHeader.h>

#include <models/Behavior.h>

using namespace tracktion;
using namespace MoTool;

class TestUIBehaviour : public UIBehaviour {
public:
    TestUIBehaviour() = default;

    void runTaskWithProgressBar(ThreadPoolJobWithProgress& t) override {
        TaskRunner runner (t);

         while (runner.isThreadRunning())
             if (! MessageManager::getInstance()->runDispatchLoopUntil (10))
                 break;
    }

private:
    //==============================================================================
    struct TaskRunner : public Thread {
        TaskRunner (ThreadPoolJobWithProgress& t)
            : Thread (t.getJobName()), task (t)
        {
            startThread();
        }

        ~TaskRunner() override {
            task.signalJobShouldExit();
            waitForThreadToExit(10000);
        }

        void run() override {
            while (!threadShouldExit())
                if (task.runJob() == ThreadPoolJob::jobHasFinished)
                    break;
        }

        ThreadPoolJobWithProgress& task;
    };
};


//==============================================================================
//==============================================================================
class TestEngineBehaviour : public ExtEngineBehaviour {
public:
    TestEngineBehaviour() = default;

    bool autoInitialiseDeviceManager() override {
        return false;
    }

    bool addSystemAudioIODeviceTypes() override {
        return false;
    }
};


//==============================================================================
//==============================================================================
class TestPropertyStorage : public PropertyStorage
{
public:
    TestPropertyStorage (juce::StringRef appName_)
        : PropertyStorage (appName_)
    {
        getAppCacheFolder().deleteRecursively(false);
        getAppPrefsFolder().deleteRecursively(false);
    }

    ~TestPropertyStorage() override {
        getAppCacheFolder().deleteRecursively(false);
        getAppPrefsFolder().deleteRecursively(false);
    }

    //==============================================================================
    void removeProperty (SettingID) override {}
    juce::var getProperty (SettingID, const juce::var& defaultValue) override { return defaultValue; }
    void setProperty (SettingID, const juce::var&) override {}
    std::unique_ptr<juce::XmlElement> getXmlProperty (SettingID) override { return {}; }
    void setXmlProperty (SettingID, const juce::XmlElement&) override {}

    //==============================================================================
    void removePropertyItem (SettingID, juce::StringRef) override {}
    juce::var getPropertyItem (SettingID, juce::StringRef, const juce::var& defaultValue) override { return defaultValue; }
    void setPropertyItem (SettingID, juce::StringRef, const juce::var&) override {}
    std::unique_ptr<juce::XmlElement> getXmlPropertyItem (SettingID, juce::StringRef) override { return {}; }
    void setXmlPropertyItem (SettingID, juce::StringRef, const juce::XmlElement&) override {}
};


//==============================================================================
//==============================================================================
struct CoutLogger : public Logger {
    void logMessage (const String& message) override
    {
        static tracktion::RealTimeSpinLock mutex;

        const std::scoped_lock lock (mutex);
        std::cout << message << "\n";
    }
};

struct NullLogger : public Logger {
    void logMessage (const String&) override {}
};


static Array<UnitTest*> filterTestsByName(const Array<UnitTest*>& allTests, const String& testNameFilter) {
    Array<UnitTest*> filteredTests;

    for (auto* test : allTests) {
        if (test->getName().containsIgnoreCase(testNameFilter)) {
            filteredTests.add(test);
        }
    }

    return filteredTests;
}

static void printUsage() {
    std::cout << "Usage: uZXTests [test_name_filter]\n";
    std::cout << "  test_name_filter: Optional partial test name to filter tests (case-insensitive)\n";
    std::cout << "  If no filter provided, runs all tests in MoTool category\n";
    std::cout << "\nExamples:\n";
    std::cout << "  uZXTests                    # Run all tests\n";
    std::cout << "  uZXTests TuningViewModel    # Run tests containing 'TuningViewModel'\n";
    std::cout << "  uZXTests AYChip             # Run tests containing 'AYChip'\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        String arg = String(argv[1]);

        if (arg == "--list-tests") {
            NullLogger nullLogger;
            Logger::setCurrentLogger(&nullLogger);

            Array<UnitTest*> listedTests = UnitTest::getTestsInCategory("MoTool");

            for (auto* test : listedTests) {
                if (test != nullptr)
                    std::cout << "TEST:" << test->getName() << "\n";
            }

            Logger::setCurrentLogger(nullptr);
            return 0;
        }

        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    ScopedJuceInitialiser_GUI init;

    Array<UnitTest*> allTests = UnitTest::getTestsInCategory("MoTool");

    CoutLogger logger;
    Logger::setCurrentLogger (&logger);

    tracktion_engine::Engine engine {
        std::make_unique<TestPropertyStorage>(CharPointer_UTF8(ProjectInfo::projectName)),
        std::make_unique<TestUIBehaviour>(),
        std::make_unique<TestEngineBehaviour>()
    };
    auto& deviceManager = engine.getDeviceManager();
    auto& hostedInterface = deviceManager.getHostedAudioDeviceInterface();

    tracktion_engine::HostedAudioDeviceInterface::Parameters hostedParams;
    hostedParams.useMidiDevices = false;
    hostedInterface.initialise(hostedParams);

    deviceManager.setMidiDeviceScanIntervalSeconds(0);
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

    juce::UnitTestRunner testRunner;

    if (argc > 1) {
        String testNameFilter = String(argv[1]);
        auto filteredTests = filterTestsByName(allTests, testNameFilter);

        if (filteredTests.isEmpty()) {
            std::cout << "No tests found matching filter: '" << testNameFilter << "'\n";
            std::cout << "\nAvailable tests:\n";
            for (auto* test : allTests) {
                std::cout << "  " << test->getName() << "\n";
            }
            return 1;
        }

        std::cout << "Running " << filteredTests.size() << " test(s) matching filter: '" << testNameFilter << "'\n";
        for (auto* test : filteredTests) {
            std::cout << "  " << test->getName() << "\n";
        }
        std::cout << "\n";

        testRunner.runTests(filteredTests);
    } else {
        testRunner.runTestsInCategory("MoTool");
    }

    Logger::setCurrentLogger (nullptr);
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

    return 0;
}
