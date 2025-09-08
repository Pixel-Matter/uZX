#include <JuceHeader.h>

#include "PluginComponent.h"

#include "../../plugins/uZX/aychip/AYPlugin.h"
#include "../../plugins/uZX/instrument/ChipInstrumentPlugin.h"
#include "../../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"

#include <common/Utilities.h>  // from Tracktion

using namespace std::literals;

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

void PluginTreeBase::display(int indent) const {
    for (int i = 0; i < subitems.size(); ++i) {
        auto* itm = subitems[i];
        String s = " ";
        for (int j = 0; j < indent; ++j)
            s += "  ";
        s += itm->getName();
        DBG(s);

        itm->display(indent + 1);
    }
}

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

    String getName() const override {
        return desc.name;
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

    String getName() const override {
        return hasDividerAbove? ("--- " + name) : name;
    }

    String name;
    bool hasDividerAbove = false;


private:
    void populateFrom(KnownPluginList::PluginTree&);
    void createUZXItems(int& num, te::Plugin::Type);
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
PluginTreeGroup::PluginTreeGroup(te::Edit& edit, KnownPluginList::PluginTree& tree, te::Plugin::Type types)
    : name("Plugins") {
    {
        int num = 1;

        auto uZXFolder = new PluginTreeGroup(TRANS("uZX Plugins"));
        addSubItem(uZXFolder);
        uZXFolder->createUZXItems(num, types);

        auto builtinFolder = new PluginTreeGroup(TRANS("Builtin Plugins"));
        addSubItem(builtinFolder);
        builtinFolder->createBuiltInItems(num, types);
    }

    {
        auto racksFolder = new PluginTreeGroup(TRANS("Plugin Racks"));
        addSubItem(racksFolder);

        racksFolder->addSubItem(new PluginTreeItem(String(te::RackType::getRackPresetPrefix()) + "-1",
                                                   TRANS("Create New Empty Rack"),
                                                   te::RackInstance::xmlTypeName,
                                                   false,
                                                   false));

        int i = 0;
        for (auto rf : edit.getRackList().getTypes())
            racksFolder->addSubItem(
                new PluginTreeItem("RACK__" + String(i++), rf->rackName, te::RackInstance::xmlTypeName, false, false));
    }

    populateFrom(tree);

    display();
}

PluginTreeGroup::PluginTreeGroup(const String& s) : name(s) {
    jassert(name.isNotEmpty());
}

void PluginTreeGroup::populateFrom(KnownPluginList::PluginTree& tree) {
    bool first = true;
    for (auto subTree : tree.subFolders) {
        if (subTree->plugins.size() > 0 || subTree->subFolders.size() > 0) {
            auto fs = new PluginTreeGroup(subTree->folder);
            fs->hasDividerAbove = first;
            addSubItem(fs);
            first = false;

            fs->populateFrom(*subTree);
        }
    }

    for (const auto& pd : tree.plugins) {
        addSubItem(new PluginTreeItem(pd));
    }
}


template<class FilterClass>
void addInternalPlugin(PluginTreeBase& item, int& num, bool synth = false) {
    item.addSubItem(new PluginTreeItem(String(num++) + "_trkbuiltin",
                                       String::fromUTF8(FilterClass::getPluginName()),
                                       FilterClass::xmlTypeName, synth, false));
}

// TODO split to categories: synths, audio effects, midi effects, utilities
void PluginTreeGroup::createBuiltInItems(int& num, te::Plugin::Type types) {
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
    // addInternalPlugin<te::AuxSendPlugin>(*this, num);
    // addInternalPlugin<te::AuxReturnPlugin>(*this, num);
    // addInternalPlugin<te::TextPlugin>(*this, num);
    // addInternalPlugin<te::FreezePointPlugin>(*this, num);
    // addInternalPlugin<te::VolumeAndPanPlugin>(*this, num);
    // addInternalPlugin<te::LevelMeterPlugin>(*this, num);

   #if TRACKTION_ENABLE_REWIRE
    addInternalPlugin<te::ReWirePlugin> (*this, num, true);
   #endif

    if (types == te::Plugin::Type::allPlugins) {
        addInternalPlugin<te::SamplerPlugin>(*this, num, true);
        addInternalPlugin<te::FourOscPlugin>(*this, num, true);
    }

    // addInternalPlugin<te::InsertPlugin>(*this, num);

   #if ENABLE_INTERNAL_PLUGINS
    for (auto& d : PluginTypeBase::getAllPluginDescriptions())
        if (isPluginAuthorised (d))
            addSubItem (new PluginTreeItem (d));
   #endif
}

void PluginTreeGroup::createUZXItems(int& num, te::Plugin::Type /*types*/) {
    addInternalPlugin<uZX::AYChipPlugin>(*this, num);
    addInternalPlugin<uZX::ChipInstrumentPlugin>(*this, num);
    addInternalPlugin<uZX::NotesToPsgPlugin>(*this, num);
}

//==============================================================================
class PluginMenu : public PopupMenu
{
public:
    PluginMenu() = default;

    PluginMenu(PluginTreeGroup& node) {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*>(node.getSubItem(i))) {
                if (subNode->hasDividerAbove) {
                    addSeparator();
                }
                addSubMenu(subNode->name, PluginMenu(*subNode), true);
            }

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subType = dynamic_cast<PluginTreeItem*>(node.getSubItem(i))) {
                addItem(subType->getUniqueName().hashCode(), subType->desc.name, true, false);
            }
    }

    static PluginTreeItem* findType(PluginTreeGroup& node, int hash) {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*>(node.getSubItem(i)))
                if (auto* t = findType(*subNode, hash))
                    return t;

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto t = dynamic_cast<PluginTreeItem*>(node.getSubItem(i)))
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
// PluginComponent
//==============================================================================
PluginPlaceholderComponent::PluginPlaceholderComponent(EditViewState& evs, te::Plugin::Ptr p)
    : PluginDeviceUI(evs, p)
{
    jassert(plugin != nullptr);
    addAndMakeVisible(button);
    button.setButtonText(plugin->getName());
    button.onClick = [this] () {
        pluginClicked(ModifierKeys::getCurrentModifiers());
    };
    setSize(200, 400);
}

PluginPlaceholderComponent::~PluginPlaceholderComponent() {}

void PluginPlaceholderComponent::resized() {
    button.setBounds(getLocalBounds());
}

}  // namespace MoTool
