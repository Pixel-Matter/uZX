# PSG format

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

## AY regisers
| Register | Function                 | Range                   |
|----------|--------------------------|-------------------------|
|     0    | Channel A fine pitch     | 8-bit (0-255)           |
|     1    | Channel A coarse pitch   | 4-bit (0-15)            |
|     2    | Channel B fine pitch     | 8-bit (0-255)           |
|     3    | Channel B coarse pitch   | 4-bit (0-15)            |
|     4    | Channel C fine pitch     | 8-bit (0-255)           |
|     5    | Channel C coarse pitch   | 4-bit (0-15)            |
|     6    | Noise pitch              | 5-bit (0-31)            |
|     7    | Mixer                    | 8-bit (see below)       |
|     8    | Channel A volume + env   | 5-bit (see below)       |
|     9    | Channel B volume + env   | 5-bit (see below)       |
|    10    | Channel C volume + env   | 5-bit (see below)       |
|    11    | Envelope fine duration   | 8-bit (0-255)           |
|    12    | Envelope coarse duration | 8-bit (0-255)           |
|    13    | Envelope shape           | 4-bit (0-15)            |

### Mixer register bits
|   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
|-------|-------|-------|-------|-------|-------|-------|-------|
|  I/O  |  I/O  | Noise | Noise | Noise | Tone  | Tone  | Tone  |
|   B   |   A   |   C   |   B   |   A   |   C   |   B   |   A   |

### Volume register bits
|   4   |   3   |   2   |   1   |   0   |
|-------|-------|-------|-------|-------|
|  Env  |  Vol  |  Vol  |  Vol  |  Vol  |


## Design of classes interaction

### JUCE helper classes

`juce::File` — PSG file in a file system.

`juce::InputStream`, `juce::OutputStream` — for reading/writing from/to file system.

`juce::MemoryBlock` — raw data in memory to hold while parsing.

### Parser helpers

`HeaderDetails parsePsgHeader(const uint8* const initialData, const size_t maxSize)`
Parses and returns header. Now only version number and interrput rate is meaningful.

### PSG API

`PSGFile` — PSG file in memory, parsed, in the form of vector of frames with register and mask arrays. Empty frames takes space too, but can be easily indexed. Alse contains frameRate.

`PSGFile::readFrom(juce::InputStream, ...)` — For reading.

`PSGFile::writeTo(juce::OutputStream, ...)` — For writing.

`juce::MidiMessageSequence PSGFile::convertToMidi()` — Convert to MIDI stream.

`juce::MidiMessageSequence` — Result of converson to MIDI.

`juce::MidiFile` — For saving the result of conversion to MIDI.

`PSGConverter` — class for converting PSG register data to MIDI and back.

`PSGClip : public MidiClip` — Clip with PSG, can be played with AY processor or plugin.