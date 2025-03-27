/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#include "Components.h"
#include "LookAndFeel.h"

#include "../../plugins/uZX/aychip/AYPlugin.h"

namespace te = tracktion;
using namespace std::literals;

namespace MoTool {

//==============================================================================
class PluginTreeBase {
public:
    virtual ~PluginTreeBase() = default;
    virtual String getUniqueName() const = 0;

    void addSubItem (PluginTreeBase* itm)   { subitems.add (itm);       }
    int getNumSubItems()                    { return subitems.size();   }
    PluginTreeBase* getSubItem (int idx)    { return subitems[idx];     }

private:
    OwnedArray<PluginTreeBase> subitems;
};

//==============================================================================
class PluginTreeItem : public PluginTreeBase {
public:
    PluginTreeItem(const PluginDescription&);
    PluginTreeItem(const String& uniqueId, const String& name, const String& xmlType, bool isSynth, bool isPlugin);

    te::Plugin::Ptr create (te::Edit&);

    String getUniqueName() const override {
        if (desc.fileOrIdentifier.startsWith (te::RackType::getRackPresetPrefix()))
            return desc.fileOrIdentifier;

        return desc.createIdentifierString();
    }

    PluginDescription desc;
    String xmlType;
    bool isPlugin = true;

    JUCE_LEAK_DETECTOR(PluginTreeItem)
};

//==============================================================================
class PluginTreeGroup : public PluginTreeBase {
public:
    PluginTreeGroup(te::Edit&, KnownPluginList::PluginTree&, te::Plugin::Type);
    PluginTreeGroup(const String&);

    String getUniqueName() const override           { return name; }

    String name;

private:
    void populateFrom(KnownPluginList::PluginTree&);
    void createBuiltInItems(int& num, te::Plugin::Type);

    JUCE_LEAK_DETECTOR(PluginTreeGroup)
};

//==============================================================================
PluginTreeItem::PluginTreeItem (const juce::PluginDescription& d)
    : desc (d)
    , xmlType (te::ExternalPlugin::xmlTypeName)
    , isPlugin (true)
{
    jassert(xmlType.isNotEmpty());
}

PluginTreeItem::PluginTreeItem (const juce::String& uniqueId, const juce::String& name,
                                const juce::String& xmlType_, bool isSynth, bool isPlugin_)
    : xmlType (xmlType_)
    , isPlugin (isPlugin_)
{
    jassert (xmlType.isNotEmpty());
    desc.name = name;
    desc.fileOrIdentifier = uniqueId;
    desc.pluginFormatName = (uniqueId.endsWith ("_trkbuiltin") || xmlType == te::RackInstance::xmlTypeName)
                                ? juce::String (te::PluginManager::builtInPluginFormatName) : juce::String();
    desc.category = xmlType;
    desc.isInstrument = isSynth;
}

te::Plugin::Ptr PluginTreeItem::create(te::Edit& ed) {
    return ed.getPluginCache().createNewPlugin (xmlType, desc);
}

//==============================================================================
PluginTreeGroup::PluginTreeGroup (te::Edit& edit, KnownPluginList::PluginTree& tree, te::Plugin::Type types)
    : name ("Plugins")
{
    {
        int num = 1;

        auto builtinFolder = new PluginTreeGroup (TRANS("Builtin Plugins"));
        addSubItem (builtinFolder);
        builtinFolder->createBuiltInItems (num, types);
    }

    {
        auto racksFolder = new PluginTreeGroup (TRANS("Plugin Racks"));
        addSubItem (racksFolder);

        racksFolder->addSubItem (new PluginTreeItem (String (te::RackType::getRackPresetPrefix()) + "-1",
                                                     TRANS("Create New Empty Rack"),
                                                     te::RackInstance::xmlTypeName, false, false));

        int i = 0;
        for (auto rf : edit.getRackList().getTypes())
            racksFolder->addSubItem (new PluginTreeItem ("RACK__" + String (i++), rf->rackName,
                                                         te::RackInstance::xmlTypeName, false, false));
    }

    populateFrom (tree);
}

PluginTreeGroup::PluginTreeGroup(const String& s)
    : name (s)
{
    jassert (name.isNotEmpty());
}

void PluginTreeGroup::populateFrom(KnownPluginList::PluginTree& tree) {
    for (auto subTree : tree.subFolders)
    {
        if (subTree->plugins.size() > 0 || subTree->subFolders.size() > 0)
        {
            auto fs = new PluginTreeGroup (subTree->folder);
            addSubItem (fs);

            fs->populateFrom (*subTree);
        }
    }

    for (const auto& pd : tree.plugins)
        addSubItem (new PluginTreeItem (pd));
}


template<class FilterClass>
void addInternalPlugin(PluginTreeBase& item, int& num, bool synth = false) {
    item.addSubItem(new PluginTreeItem(String(num++) + "_trkbuiltin",
                                       TRANS(FilterClass::getPluginName()),
                                       FilterClass::xmlTypeName, synth, false));
}

void PluginTreeGroup::createBuiltInItems(int& num, te::Plugin::Type types) {
    addInternalPlugin<uZX::AYChipPlugin>(*this, num);
    addInternalPlugin<te::VolumeAndPanPlugin>(*this, num);
    addInternalPlugin<te::LevelMeterPlugin>(*this, num);
    addInternalPlugin<te::EqualiserPlugin>(*this, num);
    addInternalPlugin<te::ReverbPlugin>(*this, num);
    addInternalPlugin<te::DelayPlugin>(*this, num);
    addInternalPlugin<te::ChorusPlugin>(*this, num);
    addInternalPlugin<te::PhaserPlugin>(*this, num);
    addInternalPlugin<te::CompressorPlugin>(*this, num);
    addInternalPlugin<te::PitchShiftPlugin>(*this, num);
    addInternalPlugin<te::LowPassPlugin>(*this, num);
    addInternalPlugin<te::MidiModifierPlugin>(*this, num);
    addInternalPlugin<te::MidiPatchBayPlugin>(*this, num);
    addInternalPlugin<te::PatchBayPlugin>(*this, num);
    addInternalPlugin<te::AuxSendPlugin>(*this, num);
    addInternalPlugin<te::AuxReturnPlugin>(*this, num);
    addInternalPlugin<te::TextPlugin>(*this, num);
    addInternalPlugin<te::FreezePointPlugin>(*this, num);

   #if TRACKTION_ENABLE_REWIRE
    addInternalPlugin<te::ReWirePlugin> (*this, num, true);
   #endif

    if (types == te::Plugin::Type::allPlugins) {
        addInternalPlugin<te::SamplerPlugin>(*this, num, true);
        addInternalPlugin<te::FourOscPlugin>(*this, num, true);
    }

    addInternalPlugin<te::InsertPlugin>(*this, num);

   #if ENABLE_INTERNAL_PLUGINS
    for (auto& d : PluginTypeBase::getAllPluginDescriptions())
        if (isPluginAuthorised (d))
            addSubItem (new PluginTreeItem (d));
   #endif
}

//==============================================================================
class PluginMenu : public PopupMenu {
public:
    PluginMenu() = default;

    PluginMenu(PluginTreeGroup& node) {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*> (node.getSubItem (i)))
                addSubMenu (subNode->name, PluginMenu (*subNode), true);

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subType = dynamic_cast<PluginTreeItem*> (node.getSubItem (i)))
                addItem (subType->getUniqueName().hashCode(), subType->desc.name, true, false);
    }

    static PluginTreeItem* findType(PluginTreeGroup& node, int hash) {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*>(node.getSubItem (i)))
                if (auto* t = findType(*subNode, hash))
                    return t;

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto t = dynamic_cast<PluginTreeItem*>(node.getSubItem (i)))
                if (t->getUniqueName().hashCode() == hash)
                    return t;

        return nullptr;
    }

    PluginTreeItem* runMenu(PluginTreeGroup& node) {
        int res = show();

        if (res == 0)
            return nullptr;

        return findType(node, res);
    }
};

//==============================================================================
te::Plugin::Ptr showMenuAndCreatePlugin(te::Edit& edit) {
    if (auto tree = EngineHelpers::createPluginTree(edit.engine)) {
        PluginTreeGroup root(edit, *tree, te::Plugin::Type::allPlugins);
        PluginMenu m(root);

        if (auto type = m.runMenu(root))
            return type->create(edit);
    }
    return {};
}

//==============================================================================
PluginComponent::PluginComponent (EditViewState& evs, te::Plugin::Ptr p)
    : editViewState (evs), plugin (p)
{
    setButtonText (plugin->getName().substring (0, 1));
}

PluginComponent::~PluginComponent()
{}

void PluginComponent::clicked(const ModifierKeys& modifiers) {
    editViewState.selectionManager.selectOnly (plugin.get());
    if (modifiers.isPopupMenu()) {
        PopupMenu m;
        m.addItem("Delete", [this] { plugin->deleteFromParent(); });
        m.showAt(this);
    } else {
        plugin->showWindowExplicitly();
    }
}

//==============================================================================
PlayheadComponent::PlayheadComponent(te::Edit& e, EditViewState& evs)
    : edit (e)
    , editViewState (evs)
{
    // TODO change to project framerate
    startTimerHz(60);
}

void PlayheadComponent::paint(Graphics& g) {
    g.setColour(Colors::Theme::success);
    g.drawRect(xPosition, 0, 2, getHeight());
}

bool PlayheadComponent::hitTest(int x, int) {
    if (std::abs(x - xPosition) <= 3)
        return true;

    return false;
}

void PlayheadComponent::mouseDown(const MouseEvent&) {
    edit.getTransport().setUserDragging(true);
}

void PlayheadComponent::mouseUp(const MouseEvent&) {
    edit.getTransport().setUserDragging(false);
}

void PlayheadComponent::mouseDrag(const MouseEvent& e) {
    auto t = editViewState.zoom.xToTime (e.x, getWidth());
    edit.getTransport().setPosition(t);
    timerCallback();
}

void PlayheadComponent::timerCallback() {
    if (firstTimer) {
        // On Linux, don't set the mouse cursor until after the Component has appeared
        firstTimer = false;
        setMouseCursor(MouseCursor::LeftRightResizeCursor);
    }

    int newX = editViewState.zoom.timeToX(edit.getTransport().getPosition(), getWidth());
    if (newX != xPosition) {
        repaint(jmin(newX, xPosition) - 1, 0, jmax(newX, xPosition) - jmin(newX, xPosition) + 3, getHeight());
        xPosition = newX;
    }
}

}  // namespace MoTool
