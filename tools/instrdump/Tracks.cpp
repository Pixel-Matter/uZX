#include "Tracks.h"

#include "strings/encoding.h"

#include <algorithm>
#include <array>

namespace MoTool::InstrDump {

void Tracks::setInitialTempo(unsigned tempo) {
    initialTempo_ = tempo;
}

Formats::Chiptune::PatternBuilder& Tracks::startPattern(unsigned index) {
    pattern_ = &patterns_[index];
    *pattern_ = {};
    line_ = 0;
    channel_ = 0;
    return *this;
}

void Tracks::startChannel(unsigned index) {
    channel_ = static_cast<int>(index);
}

void Tracks::note(unsigned note) {
    add(Kind::Note, static_cast<int>(note));
}

void Tracks::rest() {
    add(Kind::Rest, 0);
}

void Tracks::sample(unsigned slot) {
    add(Kind::Sample, static_cast<int>(slot));
}

void Tracks::ornament(unsigned slot) {
    add(Kind::Ornament, static_cast<int>(slot));
}

void Tracks::envelope(int shape) {
    add(Kind::Envelope, shape);
}

void Tracks::envelopeOn() {
    add(Kind::EnvelopeOn, 0);
}

void Tracks::Finish(uint_t size) {
    if (pattern_) {
        pattern_->size = size;
    }
}

void Tracks::StartLine(uint_t index) {
    line_ = index;
    // Some parsers never finish a pattern that fills all its lines: it then ends at its last line, as in zxtune's players.
    if (pattern_) {
        pattern_->size = std::max(pattern_->size, index + 1);
    }
}

void Tracks::SetTempo(uint_t tempo) {
    add(Kind::Tempo, static_cast<int>(tempo));
}

void Tracks::add(Kind kind, int value) {
    if (pattern_) {
        pattern_->events.push_back({line_, channel_, kind, value});
    }
}

void Tracks::play(const std::vector<Position>& order, unsigned defaultSample, unsigned defaultOrnament, Usage& usage) const {
    struct Channel {
        UsageKey key;
        bool sounds = false;
        bool envelope = false;
    };
    std::array<Channel, channels> state;
    for (auto& channel : state) {
        channel.key.sample = defaultSample;
        channel.key.ornament = defaultOrnament;
    }
    unsigned tempo = initialTempo_;
    int envelopeShape = 0;  // the chip has one envelope: the shape a channel sets is every channel's

    for (const auto& position : order) {
        const auto found = patterns_.find(position.pattern);
        if (found == patterns_.end()) {
            continue;
        }
        const auto& pattern = found->second;
        auto event = pattern.events.begin();
        for (unsigned line = 0; line != pattern.size; ++line) {
            std::array<bool, channels> struck{};
            for (; event != pattern.events.end() && event->line <= line; ++event) {
                if (event->line != line) {
                    continue;
                }
                if (event->kind == Kind::Tempo) {
                    tempo = static_cast<unsigned>(event->value);
                    continue;
                }
                if (event->channel < 0 || event->channel >= channels) {
                    continue;
                }
                auto& channel = state[static_cast<size_t>(event->channel)];
                switch (event->kind) {
                    case Kind::Note:
                        channel.key.note = static_cast<unsigned>(std::max(0, event->value + position.transposition));
                        channel.sounds = true;
                        struck[static_cast<size_t>(event->channel)] = true;
                        break;
                    case Kind::Rest:
                        channel.sounds = false;
                        break;
                    case Kind::Sample:
                        channel.key.sample = static_cast<unsigned>(event->value);
                        break;
                    case Kind::Ornament:
                        channel.key.ornament = static_cast<unsigned>(event->value);
                        break;
                    case Kind::Envelope:
                        channel.envelope = event->value >= 0;
                        if (channel.envelope) {
                            envelopeShape = event->value;
                        }
                        break;
                    case Kind::EnvelopeOn:
                        channel.envelope = true;
                        break;
                    case Kind::Tempo:
                        break;
                }
            }
            for (size_t index = 0; index != state.size(); ++index) {
                if (state[index].sounds) {
                    state[index].key.envelope = state[index].envelope ? envelopeShape : -1;
                    auto& count = usage[state[index].key];
                    count.notes += struck[index];
                    count.rows += 1;
                    count.frames += tempo;
                }
            }
        }
    }
}

void Meta::SetProgram(StringView program) {
    module_.program = Strings::ToAutoUtf8(program);
}

void Meta::SetTitle(StringView title) {
    module_.title = Strings::ToAutoUtf8(title);
}

void Meta::SetAuthor(StringView author) {
    module_.author = Strings::ToAutoUtf8(author);
}

}  // namespace MoTool::InstrDump
