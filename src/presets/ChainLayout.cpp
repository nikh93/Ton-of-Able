#include "ChainLayout.h"

#include <algorithm>
#include <cmath>

namespace chain
{
const juce::StringArray& builtinKinds()
{
    static const juce::StringArray kinds { "pitch", "compressor", "wah", "drive", "pedal", "amp", "ir", "ampEq",
                                           "geq", "autoWah", "chorus", "delay", "reverb" };
    return kinds;
}

bool isBuiltin(const juce::String& kind) { return builtinKinds().contains(kind); }

bool Node::operator==(const Node& other) const
{
    return id == other.id && kind == other.kind && name == other.name && bypassed == other.bypassed
        && semitones == other.semitones && plugin.isEquivalentTo(other.plugin);
}

Layout Layout::standard(bool compressorBeforeAmp)
{
    Layout layout;
    for (const auto& kind : builtinKinds())
        layout.serial.push_back({ kind, kind, {}, {}, false, 0 });
    // As a pedal the compressor stays first after the pitch shifter; in the
    // studio position it follows the graphic EQ.
    if (! compressorBeforeAmp)
        layout.moveTo("compressor", layout.serialIndex("geq"));
    return layout;
}

const Node* Layout::find(const juce::String& id) const
{
    for (const auto* list : { &serial, &side })
        for (const auto& node : *list)
            if (node.id == id)
                return &node;
    return nullptr;
}

Node* Layout::find(const juce::String& id)
{
    return const_cast<Node*>(static_cast<const Layout*>(this)->find(id));
}

int Layout::serialIndex(const juce::String& id) const
{
    for (size_t index = 0; index < serial.size(); ++index)
        if (serial[index].id == id)
            return static_cast<int>(index);
    return -1;
}

bool Layout::isBefore(const juce::String& a, const juce::String& b) const
{
    const auto first = serialIndex(a), second = serialIndex(b);
    return first >= 0 && second >= 0 && first < second;
}

bool Layout::isValidEndpoint(const juce::String& id, bool asSource) const
{
    if (id == input || id == keys)
        return asSource;
    if (id == output)
        return ! asSource;
    return find(id) != nullptr;
}

void Layout::moveTo(const juce::String& id, int serialPosition)
{
    Node node;
    if (const auto index = serialIndex(id); index >= 0)
    {
        node = serial[static_cast<size_t>(index)];
        serial.erase(serial.begin() + index);
    }
    else
    {
        const auto it = std::find_if(side.begin(), side.end(), [&id](const Node& n) { return n.id == id; });
        if (it == side.end())
            return;
        node = *it;
        side.erase(it);
    }
    serialPosition = juce::jlimit(0, static_cast<int>(serial.size()), serialPosition);
    serial.insert(serial.begin() + serialPosition, node);
}

void Layout::moveBefore(const juce::String& id, const juce::String& other)
{
    const auto* node = find(id);
    if (id == other || node == nullptr || serialIndex(other) < 0)
        return;
    const auto copy = *node;
    for (auto* list : { &serial, &side })
        list->erase(std::remove_if(list->begin(), list->end(), [&id](const Node& n) { return n.id == id; }), list->end());
    serial.insert(serial.begin() + serialIndex(other), copy);
}

void Layout::moveAfter(const juce::String& id, const juce::String& other)
{
    const auto* node = find(id);
    if (id == other || node == nullptr || serialIndex(other) < 0)
        return;
    const auto copy = *node;
    for (auto* list : { &serial, &side })
        list->erase(std::remove_if(list->begin(), list->end(), [&id](const Node& n) { return n.id == id; }), list->end());
    serial.insert(serial.begin() + serialIndex(other) + 1, copy);
}

void Layout::moveToSide(const juce::String& id)
{
    const auto index = serialIndex(id);
    if (index < 0 || serial[static_cast<size_t>(index)].isBuiltin())
        return;
    side.push_back(serial[static_cast<size_t>(index)]);
    serial.erase(serial.begin() + index);
}

juce::String Layout::add(Node node, int serialPosition)
{
    if (node.isBuiltin())
        return {};
    if (node.id.isEmpty() || find(node.id) != nullptr)
        node.id = node.kind + "-" + juce::Uuid().toString().substring(0, 8);
    const auto id = node.id;
    if (serialPosition < 0)
        side.push_back(std::move(node));
    else
        serial.insert(serial.begin() + juce::jlimit(0, static_cast<int>(serial.size()), serialPosition), std::move(node));
    return id;
}

void Layout::remove(const juce::String& id)
{
    for (auto* list : { &serial, &side })
        list->erase(std::remove_if(list->begin(), list->end(), [&id](const Node& n) { return n.id == id && ! n.isBuiltin(); }), list->end());
    sanitise();
}

void Layout::addLink(Link link)
{
    if (link.from == link.to)
        return;
    removeLink(link); // one link per pair and type: a new level replaces the old one
    links.push_back(link);
    sanitise();
}

void Layout::removeLink(const Link& link)
{
    links.erase(std::remove_if(links.begin(), links.end(),
                               [&link](const Link& l) { return l.from == link.from && l.to == link.to && l.midi == link.midi; }),
                links.end());
}

void Layout::sanitise()
{
    const auto fits = [this](const Link& link)
    {
        if (! isValidEndpoint(link.from, true) || ! isValidEndpoint(link.to, false) || link.from == link.to)
            return false;
        if (! link.midi)
            return link.from != keys;
        // MIDI flows from keyboards or MIDI-capable blocks into MIDI-capable blocks.
        const auto* source = find(link.from);
        const auto* target = find(link.to);
        return (link.from == keys || (source != nullptr && source->handlesMidi())) && target != nullptr && target->handlesMidi();
    };
    links.erase(std::remove_if(links.begin(), links.end(), [&fits](const Link& l) { return ! fits(l); }), links.end());
}

juce::ValueTree Layout::toValueTree() const
{
    juce::ValueTree tree("chainLayout");
    const auto writeNodes = [&tree](const std::vector<Node>& nodes, const char* type)
    {
        juce::ValueTree list(type);
        for (const auto& node : nodes)
        {
            juce::ValueTree child("node");
            child.setProperty("id", node.id, nullptr);
            child.setProperty("kind", node.kind, nullptr);
            if (node.name.isNotEmpty()) child.setProperty("name", node.name, nullptr);
            if (node.bypassed) child.setProperty("bypassed", true, nullptr);
            if (node.semitones != 0) child.setProperty("semitones", node.semitones, nullptr);
            if (node.plugin.isValid()) child.addChild(node.plugin.createCopy(), -1, nullptr);
            list.addChild(child, -1, nullptr);
        }
        tree.addChild(list, -1, nullptr);
    };
    writeNodes(serial, "serial");
    writeNodes(side, "side");
    juce::ValueTree linkList("links");
    for (const auto& link : links)
    {
        juce::ValueTree child("link");
        child.setProperty("from", link.from, nullptr);
        child.setProperty("to", link.to, nullptr);
        child.setProperty("midi", link.midi, nullptr);
        child.setProperty("gainDb", link.gainDb, nullptr);
        linkList.addChild(child, -1, nullptr);
    }
    tree.addChild(linkList, -1, nullptr);
    if (chainLevelDb != 0.0f)
        tree.setProperty("chainLevelDb", chainLevelDb, nullptr);
    return tree;
}

Layout Layout::fromValueTree(const juce::ValueTree& tree)
{
    Layout layout;
    juce::StringArray seen;
    const auto readNodes = [&](const char* type, std::vector<Node>& into)
    {
        for (const auto& child : tree.getChildWithName(type))
        {
            Node node;
            node.id = child.getProperty("id").toString();
            node.kind = child.getProperty("kind").toString();
            node.name = child.getProperty("name").toString();
            node.bypassed = child.getProperty("bypassed", false);
            node.semitones = juce::jlimit(-36, 36, static_cast<int>(child.getProperty("semitones", 0)));
            node.plugin = child.getChild(0).createCopy();
            const auto known = node.isBuiltin() || node.kind == "vst3" || node.kind == "transpose";
            if (! known || node.id.isEmpty() || seen.contains(node.id) || (node.isBuiltin() && node.id != node.kind))
                continue;
            seen.add(node.id);
            if (node.isBuiltin())
                layout.serial.push_back(node); // built-ins always run in the chain
            else
                into.push_back(node);
        }
    };
    readNodes("serial", layout.serial);
    readNodes("side", layout.side);
    // Missing built-ins come back at their standard place.
    const auto& kinds = builtinKinds();
    for (int index = 0; index < kinds.size(); ++index)
        if (! seen.contains(kinds[index]))
        {
            int position = 0;
            for (int previous = index - 1; previous >= 0; --previous)
                if (const auto at = layout.serialIndex(kinds[previous]); at >= 0)
                {
                    position = at + 1;
                    break;
                }
            layout.serial.insert(layout.serial.begin() + position, Node { kinds[index], kinds[index], {}, {}, false, 0 });
        }
    for (const auto& child : tree.getChildWithName("links"))
    {
        Link link;
        link.from = child.getProperty("from").toString();
        link.to = child.getProperty("to").toString();
        link.midi = child.getProperty("midi", false);
        const auto gain = static_cast<float>(static_cast<double>(child.getProperty("gainDb", 0.0)));
        link.gainDb = std::isfinite(gain) ? juce::jlimit(-60.0f, 12.0f, gain) : 0.0f;
        layout.links.push_back(link);
    }
    const auto level = static_cast<float>(static_cast<double>(tree.getProperty("chainLevelDb", 0.0)));
    layout.chainLevelDb = std::isfinite(level) ? juce::jlimit(-60.0f, 12.0f, level) : 0.0f;
    layout.sanitise();
    return layout;
}
} // namespace chain
