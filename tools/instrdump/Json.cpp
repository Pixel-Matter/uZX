#include "Model.h"

#include <cstdio>
#include <ostream>

namespace MoTool::InstrDump {

namespace {

void writeString(std::ostream& out, const std::string& text) {
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out << '\\' << c;
        } else if (c < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
            out << escaped;
        } else {
            out << c;
        }
    }
    out << '"';
}

/** One lane: the values of a step's field over the sequence. */
template <class Step, class Field>
void writeLane(std::ostream& out, const char* name, const std::vector<Step>& steps, Field Step::* field, bool first = false) {
    out << (first ? "" : ",") << '"' << name << "\":[";
    for (size_t i = 0; i != steps.size(); ++i) {
        out << (i ? "," : "") << static_cast<int>(steps[i].*field);
    }
    out << ']';
}

void writeLanes(std::ostream& out, const std::vector<SampleStep>& steps) {
    writeLane(out, "vol", steps, &SampleStep::vol, true);
    writeLane(out, "volSlide", steps, &SampleStep::volSlide);
    writeLane(out, "tone", steps, &SampleStep::tone);
    writeLane(out, "toneOfs", steps, &SampleStep::toneOfs);
    writeLane(out, "toneAcc", steps, &SampleStep::toneAcc);
    writeLane(out, "noise", steps, &SampleStep::noise);
    writeLane(out, "noiseOfs", steps, &SampleStep::noiseOfs);
    writeLane(out, "noiseAcc", steps, &SampleStep::noiseAcc);
    writeLane(out, "noiseAbs", steps, &SampleStep::noiseAbs);
    writeLane(out, "env", steps, &SampleStep::env);
    writeLane(out, "envOfs", steps, &SampleStep::envOfs);
    writeLane(out, "envAcc", steps, &SampleStep::envAcc);
}

void writeLanes(std::ostream& out, const std::vector<OrnamentStep>& steps) {
    writeLane(out, "noteOfs", steps, &OrnamentStep::noteOfs, true);
    writeLane(out, "noteAcc", steps, &OrnamentStep::noteAcc);
    writeLane(out, "noiseOfs", steps, &OrnamentStep::noiseOfs);
    writeLane(out, "noiseAcc", steps, &OrnamentStep::noiseAcc);
}

template <class Step>
void writeSequences(std::ostream& out, const std::vector<Sequence<Step>>& sequences) {
    out << '[';
    for (size_t i = 0; i != sequences.size(); ++i) {
        const auto& sequence = sequences[i];
        out << (i ? ",\n  " : "\n  ") << "{\"slot\":" << sequence.slot << ",\"len\":" << sequence.steps.size()
            << ",\"loop\":" << sequence.loop << ",\"end\":\"" << (sequence.stops ? "stop" : "loop") << "\",\"lanes\":{";
        writeLanes(out, sequence.steps);
        out << "}}";
    }
    out << ']';
}

void writeUsage(std::ostream& out, const Usage& usage) {
    out << '[';
    bool first = true;
    for (const auto& [key, count] : usage) {
        out << (first ? "\n  " : ",\n  ") << "{\"sample\":" << key.sample << ",\"ornament\":" << key.ornament
            << ",\"envelope\":" << key.envelope << ",\"note\":" << key.note << ",\"notes\":" << count.notes
            << ",\"rows\":" << count.rows << ",\"frames\":" << count.frames << '}';
        first = false;
    }
    out << ']';
}

void writeModule(std::ostream& out, const Module& module) {
    out << "{\"format\":";
    writeString(out, module.format);
    out << ",\"title\":";
    writeString(out, module.title);
    out << ",\"author\":";
    writeString(out, module.author);
    out << ",\"program\":";
    writeString(out, module.program);
    out << ",\"noteTable\":";
    writeString(out, module.noteTable);
    out << ",\n \"samples\":";
    writeSequences(out, module.samples);
    out << ",\n \"ornaments\":";
    writeSequences(out, module.ornaments);
    out << ",\n \"usage\":";
    writeUsage(out, module.usage);
    out << '}';
}

}  // namespace

void writeJson(std::ostream& out, const std::vector<Module>& modules) {
    out << "{\"modules\":[";
    for (size_t i = 0; i != modules.size(); ++i) {
        out << (i ? ",\n" : "\n");
        writeModule(out, modules[i]);
    }
    out << "\n]}\n";
}

}  // namespace MoTool::InstrDump
