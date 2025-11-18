# Project Management

Guide to Tracktion Engine's project management system for organizing Edits, media, and project metadata.

## Overview

The Project system provides:
- Project-level organization of Edits
- Media file management and reference tracking
- Project search and indexing
- Shared resources across multiple Edits

## Project Basics

### Creating and Loading Projects

```cpp
// Get project manager
auto& projectManager = engine.getProjectManager();

// Create new project
auto project = projectManager.createNewProject(projectFolder);
project->setName("My Project");

// Load existing project
auto project = projectManager.loadProject(projectFile);

// Get current project
auto currentProject = projectManager.getCurrentProject();
```

### Project Properties

```cpp
// Project metadata
project->setName("My Album");
project->setDescription("Collection of tracks");

// Project location
juce::File projectFolder = project->getProjectFolder();
juce::File projectFile = project->getProjectFile();

// Media folder (for audio files)
juce::File mediaFolder = project->getMediaFolder();
```

## ProjectItem System

### ProjectItemID

Unique identifier system for all project elements:

```cpp
// Get ID for Edit/Clip/Track
auto itemID = edit.getProjectItemID();

// Create new ID
auto newID = project->createNewItemID();

// Compare IDs
bool same = (itemID == otherItemID);

// Convert to/from string
juce::String idString = itemID.toString();
auto parsedID = te::ProjectItemID::fromString(idString);
```

### Managing Project Items

```cpp
// Add Edit to project
project->addEdit(edit);

// Remove Edit
project->removeEdit(edit);

// Get all Edits in project
auto edits = project->getAllEdits();

// Find Edit by ID
if (auto edit = project->getEditByID(itemID))
{
    // Use edit
}
```

## Media Management

### Project File References

```cpp
// Store file reference in project
te::SourceFileReference::setToProjectFileReference(
    state,        // ValueTree (e.g., clip state)
    audioFile,    // juce::File
    edit          // te::Edit
);

// Retrieve file from reference
if (auto file = te::SourceFileReference::findFileFromProjectReference(edit, state))
{
    // Use file
}

// Check if file is in project
bool isProjectFile = project->isProjectFile(file);
```

### Copying Files to Project

```cpp
// Import external file to project
juce::File externalFile("/path/to/audio.wav");
juce::File projectFile = project->importFile(externalFile);

// Use project-local copy
auto audioFile = te::AudioFile(engine, projectFile);
```

## Project Search Index

### Searching Project Content

```cpp
// Get search index
auto& searchIndex = project->getSearchIndex();

// Search for term
auto results = searchIndex.search("kick drum");

// Results contain ProjectItemIDs
for (auto& result : results)
{
    if (auto edit = project->getEditByID(result.itemID))
    {
        // Found matching edit
    }
}

// Update index (after adding content)
searchIndex.rebuild();
```

### Indexing Custom Content

```cpp
// Add custom searchable data
searchIndex.addItem(itemID, searchableText);

// Remove from index
searchIndex.removeItem(itemID);

// Clear index
searchIndex.clear();
```

## Project Organization Patterns

### Multi-Edit Project

```cpp
class AlbumProject
{
public:
    AlbumProject(te::Engine& engine, const juce::File& folder)
    {
        auto& pm = engine.getProjectManager();
        project = pm.createNewProject(folder);
        project->setName("Album");

        // Create Edit for each song
        for (int i = 0; i < numSongs; ++i)
        {
            auto editFile = project->getProjectFolder()
                .getChildFile("Song" + juce::String(i + 1) + ".tracktionedit");

            auto edit = te::createEmptyEdit(engine, editFile);
            edit->setName("Song " + juce::String(i + 1));

            project->addEdit(*edit);
            songs.add(edit);
        }
    }

    te::Edit* getSong(int index) { return songs[index]; }
    int getNumSongs() const { return songs.size(); }

private:
    te::Project::Ptr project;
    juce::ReferenceCountedArray<te::Edit> songs;
    int numSongs = 10;
};
```

### Template System

```cpp
// Save Edit as template
void saveAsTemplate(te::Edit& edit, const juce::File& templateFile)
{
    // Save Edit
    edit.saveAs(templateFile, true);

    // Mark as template
    edit.state.setProperty("isTemplate", true, nullptr);
}

// Load from template
te::Edit::Ptr loadFromTemplate(te::Engine& engine,
                               const juce::File& templateFile)
{
    auto edit = te::loadEditFromFile(engine, templateFile);

    // Clear template flag
    edit->state.removeProperty("isTemplate", nullptr);

    // Generate new IDs for all items
    edit->regenerateIDs();

    return edit;
}
```

## Project Archiving

### Collect Project Media

```cpp
// Collect all referenced files
void collectProjectMedia(te::Project& project,
                        const juce::File& destinationFolder)
{
    juce::Array<juce::File> referencedFiles;

    // Scan all Edits for file references
    for (auto edit : project.getAllEdits())
    {
        for (auto track : edit->getAllTracks())
        {
            for (auto clip : track->getClips())
            {
                if (auto audioClip = dynamic_cast<te::WaveAudioClip*>(clip))
                {
                    if (auto file = audioClip->getAudioFile().getFile())
                        referencedFiles.addIfNotAlreadyThere(*file);
                }
            }
        }
    }

    // Copy to destination
    for (auto& file : referencedFiles)
    {
        auto dest = destinationFolder.getChildFile(file.getFileName());
        file.copyFileTo(dest);
    }
}
```

### Project Archive Export

```cpp
// Create self-contained project archive
bool exportProjectArchive(te::Project& project,
                         const juce::File& archiveFile)
{
    using namespace te;

    // Create archive
    ArchiveFile archive;
    archive.setFile(archiveFile);

    // Add all Edits
    for (auto edit : project.getAllEdits())
    {
        archive.addEdit(*edit);

        // Add referenced media
        ReferencedMaterialList materials(*edit);
        for (auto& file : materials.getFiles())
            archive.addFile(file);
    }

    // Write archive
    return archive.write();
}
```

## Best Practices

1. **Project Folder**: Keep all media in project folder for portability
2. **File References**: Use SourceFileReference for automatic path management
3. **IDs**: Use ProjectItemID for all cross-references
4. **Search Index**: Rebuild index after bulk changes
5. **Templates**: Use regenerateIDs() when cloning Edits
6. **Archiving**: Include all referenced media in archives
7. **Organization**: Use subfolders for media types (audio/, MIDI/, etc.)

## Common Patterns

### Auto-Save System

```cpp
class ProjectAutoSaver : private juce::Timer
{
public:
    ProjectAutoSaver(te::Project& p)
        : project(p)
    {
        startTimer(60000);  // Auto-save every minute
    }

    void timerCallback() override
    {
        // Save all Edits
        for (auto edit : project.getAllEdits())
        {
            if (edit->hasChanged())
                edit->flushState();
        }
    }

private:
    te::Project& project;
};
```

### Project Validation

```cpp
struct ValidationResult
{
    bool isValid = true;
    juce::StringArray errors;
    juce::StringArray warnings;
};

ValidationResult validateProject(te::Project& project)
{
    ValidationResult result;

    // Check all Edits
    for (auto edit : project.getAllEdits())
    {
        // Check for missing audio files
        for (auto track : edit->getAllTracks())
        {
            for (auto clip : track->getClips())
            {
                if (auto audioClip = dynamic_cast<te::WaveAudioClip*>(clip))
                {
                    if (!audioClip->getAudioFile().isValid())
                    {
                        result.isValid = false;
                        result.errors.add("Missing audio file: " +
                                        audioClip->getName());
                    }
                }
            }
        }

        // Check for offline plugins
        for (auto track : edit->getAllTracks())
        {
            for (auto plugin : track->pluginList)
            {
                if (!plugin->isEnabled())
                {
                    result.warnings.add("Plugin offline: " +
                                      plugin->getName());
                }
            }
        }
    }

    return result;
}
```
