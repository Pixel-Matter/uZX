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
            currentFrame.registers[reg] = *val;
            currentFrame.mask[reg] = true;
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
 * PsgFile - Reader for PSG (Programmable Sound Generator) files
 */
class PsgFile {
public:
    PsgFile(const juce::File& file) noexcept
        : file_(file)
    {}

    const juce::File& getFile() const noexcept {
        return file_;
    }

    void ensureRead() {
        if (psgData_.isEmpty())
            read();
    }

    PsgData& getData() noexcept {
        return psgData_;
    }

    const PsgData& getData() const noexcept {
        return psgData_;
    }

private:
    juce::File file_;
    PsgData psgData_;

    /** Reads a PSG file format stream.

        After calling this, you can get the data that were read from the file by using the
        getData() method.

        @returns true if the stream was read successfully
    */
    bool read() {
        psgData_.clear();

        using namespace PsgFileHelpers;

        FileInputStream sourceStream(file_);
        MemoryBlock data;

        const int maxSensibleMidiFileSize = 200 * 1024 * 1024;
        // (put a sanity-check on the file size, as psg files are generally small)
        if (!sourceStream.readIntoMemoryBlock(data, maxSensibleMidiFileSize))
            return false;

        auto remaining = data.getSize();
        auto d = static_cast<const uint8*> (data.getData());

        const auto header = parsePsgHeader(d, remaining);
        if (!header.hasValue())
            return false;

        d += header->bytesRead;
        remaining -= (size_t) header->bytesRead;

        bool expectFrame = false;
        while (remaining > 0) {
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
                // add pause * 4 empty frames
                for (int i = 0; i < *pause * 4; ++i) {
                    psgData_.frames.push_back({});
                }
                expectFrame = true;
            } else if (*marker == 0xFF) {
                expectFrame = true;
            } else {
                return false;
            }
        }
        return true;
    }

    /** Writes the PSG data as a PSG file.
        @param destStream        the destination stream
        @returns true if the operation succeeded.
    */
    // TODO implement
    bool writeTo(OutputStream& destStream) const;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgFile)
};


} // namespace MoTool::uZX