#pragma once

#include <JuceHeader.h>

#include "PsgData.h"


namespace MoTool::uZX {

namespace PsgFileHelpers {

namespace {

template <typename Integral>
struct ReadTrait;

template <>
struct ReadTrait<uint8> { static constexpr auto read = [](const uint8*& data) { return *data; }; };

template <>
struct ReadTrait<uint32> { static constexpr auto read = ByteOrder::bigEndianInt; };

template <>
struct ReadTrait<uint16> { static constexpr auto read = ByteOrder::bigEndianShort; };

template <typename Integral>
Optional<Integral> tryRead(const uint8*& data, size_t& remaining) {
    using Trait = ReadTrait<Integral>;
    constexpr auto size = sizeof(Integral);

    if (remaining < size)
        return {};

    const Optional<Integral> result { Trait::read (data) };

    data += size;
    remaining -= size;

    return result;
}

}  // namespace {}

struct PsgHeaderDetails {
    size_t bytesRead = 0;
    short version = 0;
    short interruptRate = 0;
    short numberOfTracks = 0;
};

//==============================================================================
/**
    AY registers dump data structure:

    The data consists of a list of frames, where each frame is a list of register-value pairs.

    Header:
    | Offset | Bytes | Description                       |
    |--------|-------|-----------------------------------|
    | +0     | 3     | Magic 'PSG'                       |
    | +3     | 1     | Marker '1Ah'                      |
    | +4     | 1     | Version number                    |
    | +5     | 1     | Interrupt rate (for versions 10+) |
    | +6     | 10    | Reserved                          |

    Data:
    - Byte 0xFF: Start of an interrupt.
    - Byte 0xFE: Indicates a pause. The following byte, multiplied by 4, gives the number of interrupts during which there was no output.
    - Byte 0x00-0x0F: AY register number. The next byte is the value to be written to this register.

    The sequence continues with register-value pairs until the next interrupt marker (0xFF), the end of the file, or a pause marker (0xFE).
*/
static Optional<PsgHeaderDetails> parsePsgHeader(const uint8* const initialData, const size_t maxSize) {

    auto* data = initialData;
    auto remaining = maxSize;

    auto ch = tryRead<uint32>(data, remaining);

    if (!ch.hasValue())
        return {};

    if (*ch != ByteOrder::bigEndianInt("PSG\x1A")) {
        return {};
    }

    const auto version = tryRead<uint8>(data, remaining);
    if (!version.hasValue())
        return {};

    if (*version == 0xFF) {  // some emulators write frames right after the magic (zxtune tolerates this too)
        PsgHeaderDetails result;
        result.bytesRead = 4;
        return {result};
    }

    const auto interruptRate = tryRead<uint8>(data, remaining);
    if (!interruptRate.hasValue())
        return {};

    static constexpr size_t headerSize = 10;
    if (remaining <= headerSize)
        return {};

    PsgHeaderDetails result;
    result.version = (short)*version;
    result.interruptRate = (short)*interruptRate;
    result.bytesRead = maxSize - remaining + headerSize;

    return {result};
}

static Optional<PsgRegsFrame> readNextFrame(const uint8*& data, size_t& remaining) {
    PsgRegsFrame currentFrame {};
    if (remaining == 0)
        return {};

    while (remaining > 0) {
        const auto byte = tryRead<uint8>(data, remaining);
        if (!byte.hasValue())
            return {};

        if (*byte <= 0x0F) {
            const auto reg = *byte;
            const auto val = tryRead<uint8>(data, remaining);
            if (!val.hasValue())
                return {};
            if (reg < PsgRegsFrame::size()) {  // R14/R15 are I/O ports, not sound
                currentFrame.registers[reg] = *val;
                currentFrame.mask[reg] = true;
            }
        } else {
            // not a register, rewind
            data -= 1;
            remaining += 1;
            return currentFrame;
        }
    }
    return currentFrame;
}

}

//==============================================================================
/**
 * PsgFile - Reader and writer for PSG (Programmable Sound Generator) files
 */
class PsgFile {
public:
    PsgFile(const juce::File& file) noexcept
        : file_(file)
    {}

    const juce::File& getFile() const noexcept {
        return file_;
    }

    /** Frames read at most, four hours at 50 Hz: a pause adds up to 1,020 empty frames for
        2 bytes, so a crafted file of a few MB would otherwise fill the memory. The rest is dropped. */
    static constexpr size_t maxFrames = 4 * 60 * 60 * 50;

    /** Frames a pause's count stands for: four, as the format has it. */
    static constexpr int standardPauseFrames = 4;

    /** Reads a PSG held in memory, such as one embedded in another file, in place of the file.
        `pauseFrames` is how many frames each count of a pause stands for: some writers mean one.
        Returns false if it holds nothing playable; a damaged tail is dropped as ensureRead() drops it. */
    bool readFrom(const void* data, size_t size, int pauseFrames = standardPauseFrames) {
        parse(static_cast<const uint8*>(data), size, pauseFrames);
        return ! psgData_.isEmpty();
    }

    /** Reads the file once. Returns false if it holds nothing playable: missing, not a
        PSG file, or no frames. A damaged tail (e.g. a 0xFD end marker) is dropped and the
        frames before it are kept, so read() failing alone doesn't make a file unreadable. */
    bool ensureRead() {
        if (psgData_.isEmpty())
            read();
        return ! psgData_.isEmpty();
    }

    PsgData& getData() noexcept {
        return psgData_;
    }

    const PsgData& getData() const noexcept {
        return psgData_;
    }

    /** Header of the last read; all zeros before a read or for a file without one. */
    const PsgFileHelpers::PsgHeaderDetails& getHeader() const noexcept {
        return header_;
    }

private:
    juce::File file_;
    PsgData psgData_;
    PsgFileHelpers::PsgHeaderDetails header_;

    /** Reads a PSG file format stream.

        After calling this, you can get the data that were read from the file by using the
        getData() method.

        @returns true if the stream was read successfully
    */
    bool read() {
        psgData_ = {};

        FileInputStream sourceStream(file_);
        MemoryBlock data;

        const int maxSensibleMidiFileSize = 200 * 1024 * 1024;
        // (put a sanity-check on the file size, as psg files are generally small)
        if (!sourceStream.readIntoMemoryBlock(data, maxSensibleMidiFileSize))
            return false;

        return parse(static_cast<const uint8*> (data.getData()), data.getSize());
    }

    /** Parses a PSG's bytes into the data. @returns true if all of them were read */
    bool parse(const uint8* d, size_t remaining, int pauseFrames = standardPauseFrames) {
        psgData_ = {};

        using namespace PsgFileHelpers;

        const auto header = parsePsgHeader(d, remaining);
        if (!header.hasValue())
            return false;

        header_ = *header;
        // Version 10 and later give the interrupt rate; older files play at 50 Hz
        if (header->version >= 10 && header->interruptRate > 0)
            psgData_.options.frameRate = header->interruptRate;
        d += header->bytesRead;
        remaining -= (size_t) header->bytesRead;

        bool expectFrame = false;
        while (remaining > 0 && psgData_.frames.size() < maxFrames) {
            if (expectFrame) {
                // auto oldRemaining = remaining;
                const auto frame = readNextFrame(d, remaining);
                if (!frame.hasValue())
                    return false;
                psgData_.frames.push_back(*frame);
                expectFrame = false;
                continue;
            }

            // expect marker
            const auto marker = tryRead<uint8>(d, remaining);
            if (!marker.hasValue())
                return false;

            if (*marker == 0xFE) {
                const auto pause = tryRead<uint8>(d, remaining);
                if (!pause.hasValue())
                    return false;
                // a pause of that many empty frames
                for (int i = 0; i < *pause * pauseFrames; ++i) {
                    psgData_.frames.push_back({});
                }
                // The next interrupt starts with its own 0xFF marker. Register pairs that
                // follow the pause directly belong to the last paused frame (as zxtune
                // reads them); starting a new frame here would shift everything after it.
                if (remaining > 0 && *d <= 0x0F) {
                    const auto frame = readNextFrame(d, remaining);
                    if (!frame.hasValue())
                        return false;
                    if (psgData_.frames.empty())
                        psgData_.frames.push_back(*frame);
                    else
                        psgData_.frames.back().update(*frame);
                }
            } else if (*marker == 0xFF) {
                expectFrame = true;
            } else {
                return false;
            }
        }
        psgData_.frames.resize(std::min(psgData_.frames.size(), maxFrames));  // a pause can overshoot
        return remaining == 0;
    }

public:
    /** Writes the data as a PSG file of version 10, which carries the frame rate: each frame as its
        marker and the registers it sets. Frames at the end that set nothing go as a marker each for up
        to three and a pause for the rest, so the file never ends in a marker, which not every reader
        takes for a frame.
        @returns true if the stream took everything.
    */
    bool writeTo(OutputStream& destStream) const {
        const auto& frames = psgData_.frames;
        size_t used = frames.size();
        while (used > 0 && frames[used - 1].isEmpty())
            --used;

        const auto put = [&](int byte) { return destStream.writeByte(static_cast<char>(byte)); };
        bool ok = destStream.write("PSG\x1A", 4) && put(10) && put(jlimit(1, 255, roundToInt(psgData_.options.frameRate)));
        for (int i = 0; i < 10; ++i)
            ok = ok && put(0);

        for (size_t f = 0; f < used; ++f) {
            ok = ok && put(0xFF);
            for (size_t reg = 0; reg < PsgRegsFrame::size(); ++reg)
                if (frames[f].mask[reg])
                    ok = ok && put(static_cast<int>(reg)) && put(frames[f].registers[reg]);
        }

        size_t empty = frames.size() - used;
        if (empty == 0)
            return ok;
        for (; empty % 4 != 0; --empty)
            ok = ok && put(0xFF);
        do {
            const auto pause = std::min(empty / 4, size_t(255));
            ok = ok && put(0xFE) && put(static_cast<int>(pause));
            empty -= pause * 4;
        } while (empty > 0);
        return ok;
    }


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgFile)
};


} // namespace MoTool::uZX