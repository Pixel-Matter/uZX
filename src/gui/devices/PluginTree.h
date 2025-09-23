#pragma once

#include <JuceHeader.h>

namespace tracktion { inline namespace engine { 
    class Edit; 
    class Plugin; 
}}

namespace MoTool {

//==============================================================================
class PluginTreeBase {
public:
    virtual ~PluginTreeBase() = default;
    virtual String getUniqueName() const = 0;
    virtual String getName() const = 0;

    void addSubItem (PluginTreeBase* itm)   { subitems.add (itm);       }
    int getNumSubItems()                    { return subitems.size();   }
    PluginTreeBase* getSubItem (int idx)    { return subitems[idx];     }

    void display(int indent = 0) const;

private:
    OwnedArray<PluginTreeBase> subitems;
};

//==============================================================================
class PluginTreeItem : public PluginTreeBase {
public:
    PluginTreeItem(const PluginDescription&);
    PluginTreeItem(const String& uniqueId, const String& name, const String& xmlType, bool isSynth, bool isPlugin);

    tracktion::Plugin::Ptr create (tracktion::Edit&);

    String getUniqueName() const override;
    String getName() const override;

    PluginDescription desc;
    String xmlType;
    bool isPlugin = true;

    JUCE_LEAK_DETECTOR(PluginTreeItem)
};

//==============================================================================
class PluginTreeGroup : public PluginTreeBase {
public:
    PluginTreeGroup(tracktion::Edit&, KnownPluginList::PluginTree&, tracktion::Plugin::Type);
    PluginTreeGroup(const String&);

    String getUniqueName() const override           { return name; }
    String getName() const override;

    String name;
    bool hasDividerAbove = false;

private:
    void populateFrom(KnownPluginList::PluginTree&);
    void createUZXItems(int& num, tracktion::Plugin::Type);
    void createBuiltInItems(int& num, tracktion::Plugin::Type);

    JUCE_LEAK_DETECTOR(PluginTreeGroup)
};

//==============================================================================
class PluginMenu : public PopupMenu
{
public:
    PluginMenu() = default;
    PluginMenu(PluginTreeGroup& node);

    static PluginTreeItem* findType(PluginTreeGroup& node, int hash);
    PluginTreeItem* runMenu(PluginTreeGroup& node);
};

}  // namespace MoTool