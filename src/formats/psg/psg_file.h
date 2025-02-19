#pragma once

#include "psg.h"

#include <JuceHeader.h>

namespace MoTool::uZX {

namespace PsgFileHelpers {

template <>
struct ReadTrait<uint8> { static constexpr auto read = ByteOrder::bigEndianShort; };

struct HeaderDetails {
    size_t bytesRead = 0;
    short version = 0;
    short interruptRate = 0;
    short numberOfTracks = 0;
};

//==============================================================================
/**
    AY registers dump data structure is simple:
    list of frames, and each frame is a list of pairs (register, value)

    | Offset | Bytes | Description                       |
    |--------|-------|-----------------------------------|
    | +0     | 3     | Magic 'PSG'                       |
    | +3     | 1     | Marker '1Ah'                      |
    | +4     | 1     | Version number                    |
    | +5     | 1     | Interrupt rate (for versions 10+) |
    | +6     | 10    | Unknown                           |

    Далее следуют строки байтов, начинающиеся с 0FFh или 0FEh.
    Байт, следующий за 0FEh, помноженный на 4 даст количество прерываний, в течении которых не было вывода на сопроцессор.
    Байт 0FFh – маркёр начала прерывания.
    Если вслед за ним идёт байт от 0 до 15, то это номер регистра АY, в который произошёл вывод значения, следующего за этим байтом.
    Далее идёт следующая двойка байт, первый байт которой – номер регистра, а второй – значение.
    И так пока не встретится маркер следующего прерывания, конец файла или байт 0FEh.

    header = stream.read(16)  # Read enough bytes for header + unused
    if not header.startswith(b'PSG\x1A'):
        raise ValueError('Not a PSG file or wrong file format')
*/
static Optional<HeaderDetails> parsePsgHeader(const uint8* const initialData, const size_t maxSize) {
    using namespace juce::MidiFileHelpers;

    auto* data = initialData;
    auto remaining = maxSize;

    auto ch = tryRead<uint32>(data, remaining);

    if (!ch.hasValue())
        return {};

    if (*ch != ByteOrder::bigEndianInt("PSG\x1A")) {
        return {};
    }

    const auto bytesRemaining = tryRead<uint32>(data, remaining);

    if (! bytesRemaining.hasValue() || *bytesRemaining > remaining)
        return {};

    const auto optFileType = tryRead<uint16> (data, remaining);

    if (! optFileType.hasValue() || 2 < *optFileType)
        return {};

    const auto optNumTracks = tryRead<uint16> (data, remaining);

    if (! optNumTracks.hasValue() || (*optFileType == 0 && *optNumTracks != 1))
        return {};

    const auto optTimeFormat = tryRead<uint16> (data, remaining);

    if (! optTimeFormat.hasValue())
        return {};

    HeaderDetails result;

    result.fileType = (short) *optFileType;
    result.timeFormat = (short) *optTimeFormat;
    result.numberOfTracks = (short) *optNumTracks;
    result.bytesRead = maxSize - remaining;

    return { result };
}

}

//==============================================================================
class PSGFile {
public:
    //==============================================================================
    PSGFile() = default;
    PSGFile(const PSGFile&) = default;
    PSGFile& operator= (const PSGFile&) = default;
    PSGFile (PSGFile&&) = default;
    PSGFile& operator= (PSGFile&&) = default;

    //==============================================================================
    PSGData& getData() noexcept {
        return psgData_;
    }

    const PSGData& getData() const noexcept {
        return psgData_;
    }

    void clear() {
        psgData_.frames.clear();
    }

    size_t getMachineFramesSize() const {
        return psgData_.frames.size() * psgData_.frameStep;
    }

    //==============================================================================
    /** Reads a PSG file format stream.

        After calling this, you can get the data that were read from the file by using the
        getData() method.

        @returns true if the stream was read successfully
    */
    bool readFrom(InputStream& sourceStream /* TODO int* psgFileType = nullptr */ ) {
        clear();
        MemoryBlock data;

        const int maxSensibleMidiFileSize = 200 * 1024 * 1024;
        // (put a sanity-check on the file size, as psg files are generally small)
        if (!sourceStream.readIntoMemoryBlock(data, maxSensibleMidiFileSize))
            return false;

        

        return true;
    }

    /** Writes the PSG data as a PSG file.
        @param destStream        the destination stream
        @returns true if the operation succeeded.
    */
    bool writeTo(OutputStream& destStream /*TODO int psgFileType = 1*/) const;

private:
    //==============================================================================
    PSGData psgData_;

    JUCE_LEAK_DETECTOR(PSGFile)
};


} // namespace MoTool::uZX