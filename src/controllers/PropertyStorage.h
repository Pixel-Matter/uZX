#pragma once

#include <JuceHeader.h>

namespace MoTool {

class PropertyStorage : public tracktion::PropertyStorage {
public:
    PropertyStorage(StringRef appName)
        : tracktion::PropertyStorage(appName)
    {}

    ~PropertyStorage() override = default;

    // Override to add custom property handling for UI settings
    var getProperty(tracktion::SettingID setting, const var& defaultValue = {}) override {
        return tracktion::PropertyStorage::getProperty(setting, defaultValue);
    }

    void setProperty(tracktion::SettingID setting, const var& value) override {
        tracktion::PropertyStorage::setProperty(setting, value);
    }

    // Custom methods for non-engine settings
    var getCustomProperty(const String& key, const var& defaultValue = {}) {
        auto value = getPropertiesFile().getValue(key, defaultValue);
        return value;
    }

    void setCustomProperty(const String& key, const var& value) {
        getPropertiesFile().setValue(key, value);
    }

    String getApplicationVersion() override {
        return ProjectInfo::versionString;
    }
};

} // namespace MoTool