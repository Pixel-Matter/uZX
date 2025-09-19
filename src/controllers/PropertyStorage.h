#pragma once

#include <JuceHeader.h>

#include "../../../modules/3rd_party/magic_enum/tracktion_magic_enum.hpp"

namespace MoTool {

class PropertyStorage : public tracktion::PropertyStorage {
public:
    PropertyStorage(StringRef appName)
        : tracktion::PropertyStorage(appName)
    {}

    ~PropertyStorage() override = default;

    PropertiesFile& getPropertiesFile() override {
        if (propertiesFile == nullptr) {
            PropertiesFile::Options options;
            options.millisecondsBeforeSaving = 2000;
            options.storageFormat = PropertiesFile::storeAsXML;
            options.applicationName = getApplicationName();
            options.folderName = getApplicationName();
            options.filenameSuffix = ".settings";
            options.osxLibrarySubFolder = "Application Support";

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
    std::unique_ptr<PropertiesFile> propertiesFile;
};

} // namespace MoTool