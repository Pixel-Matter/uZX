#include <JuceHeader.h>

#include <model/Behavior.h>

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


int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    ScopedJuceInitialiser_GUI init;

    CoutLogger logger;
    Logger::setCurrentLogger (&logger);

    tracktion_engine::Engine engine {
        std::make_unique<TestPropertyStorage>(ProjectInfo::projectName),
        std::make_unique<TestUIBehaviour>(),
        std::make_unique<TestEngineBehaviour>()
    };
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory("MoTool");

    Logger::setCurrentLogger (nullptr);
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

    return 0;
}