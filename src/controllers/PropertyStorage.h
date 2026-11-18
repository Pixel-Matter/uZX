#pragma once

#include <JuceHeader.h>

#include "../../../modules/3rd_party/magic_enum/tracktion_magic_enum.hpp"

namespace MoTool {

class PropertyStorage : public tracktion::PropertyStorage {
public:
    PropertyStorage(StringRef appName_)
        : tracktion::PropertyStorage(appName_)
    {}

    ~PropertyStorage() override = default;

    /** The folder of the app's settings file, where the app keeps its own data. */
    static File getAppDataFolder(const String& appName) {
        return makeOptions(appName).getDefaultFile().getParentDirectory();
    }

    PropertiesFile& getPropertiesFile() override {
        if (propertiesFile == nullptr) {
            const auto options = makeOptions(getApplicationName());
            propertiesFile = std::make_unique<PropertiesFile>(options.getDefaultFile(), options);
            TRACKTION_LOG("Properties file: " + propertiesFile->getFile().getFullPathName());
        }

        return *propertiesFile;
    }

    //==============================================================================
    File getDefaultLoadSaveDirectory(StringRef category) override {
        auto defaultLoc = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(getApplicationName());
        return getCustomProperty("loadSaveDirectory " + category, defaultLoc.getFullPathName()).toString();
    }

    void setDefaultLoadSaveDirectory(StringRef category, const File& location) override {
        setCustomProperty("loadSaveDirectory " + category, location.getFullPathName());
    }

    File getDefaultLoadSaveDirectory(tracktion::ProjectItem::Category category) override {
        String stringCat = std::string(magic_enum::enum_name<tracktion::ProjectItem::Category>(category));
        return getDefaultLoadSaveDirectory(stringCat);
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
private:
    static PropertiesFile::Options makeOptions(const String& appName) {
        PropertiesFile::Options options;
        options.millisecondsBeforeSaving = 2000;
        options.storageFormat = PropertiesFile::storeAsXML;
        options.applicationName = appName;
        options.folderName = appName;
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }

    std::unique_ptr<PropertiesFile> propertiesFile;
};

} // namespace MoTool