#include "TuningRegistry.h"

#include "TemperamentSystem.h"
#include "TuningSystemBase.h"
#include "AutoTuning.h"
#include <memory>

namespace MoTool {

std::unique_ptr<TuningSystem> makeBuiltinTuning(TuningOptions& options) {
    switch (options.builtinTable) {
        case BuiltinTuningType::EqualTemperament: {
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;

            return std::make_unique<AutoTuning>(
                options.chipClock, std::make_unique<EqualTemperamentTuning>(options.a4Frequency)
            );
        }

        case BuiltinTuningType::Just5Limit: {
            // Implement Just Intonation 5-limit tuning
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 433.0;
            options.tonic = Scale::Tonic::D;
            options.scaleType = Scale::ScaleType::Phrygian;
            options.tuningSystemType = TuningSystemType::Just5Limit;
            return std::make_unique<AutoTuning>(
                options.chipClock, makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.a4Frequency
                )
            );
        }

        case BuiltinTuningType::Just5Limit2: {
            // Implement Just Intonation 5-limit T=45:64 tuning
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 433.0;
            options.tonic = Scale::Tonic::D;
            options.scaleType = Scale::ScaleType::Phrygian;
            options.tuningSystemType = TuningSystemType::Just5LimitT45_64;
            return std::make_unique<AutoTuning>(
                options.chipClock, makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.a4Frequency
                )
            );
        }

        case BuiltinTuningType::Pythagorean: {
            // Implement Pythagorean tuning
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 433.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::Pythagorean;
            auto referenceTuning = makeReferenceTuningSystem(
                options.tuningSystemType,
                options.tonic,
                options.a4Frequency
            );
            auto autoTuning = std::make_unique<AutoTuning>(
                options.chipClock, std::move(referenceTuning)
            );
            return autoTuning;
        }

        case BuiltinTuningType::CustomPT_0_PT: {
            // ProTracker #0 (Original PT3 table)
            options.chipChoice = uZX::ChipClockChoice::Pentagon_1_75_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 474.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClock,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1): $0C22-$066D
                    3106, 2931, 2767, 2611, 2465, 2327, 2196, 2073, 1956, 1847, 1743, 1645,
                    // Octave 2 (C2-B2): $0611-$0337
                    1553, 1466, 1383, 1306, 1232, 1163, 1098, 1036, 978, 923, 871, 823,
                    // Octave 3 (C3-B3): $0308-$019B
                    776, 733, 692, 653, 616, 582, 549, 518, 489, 462, 436, 411,
                    // Octave 4 (C4-B4): $0184-$00CE
                    388, 366, 346, 326, 308, 291, 274, 259, 245, 231, 218, 206,
                    // Octave 5 (C5-B5): $00C2-$0067
                    194, 183, 173, 163, 154, 145, 137, 130, 122, 115, 109, 103,
                    // Octave 6 (C6-B6): $0061-$0033
                    97, 92, 86, 82, 77, 73, 69, 65, 61, 58, 54, 51,
                    // Octave 7 (C7-B7): $0031-$001A
                    49, 46, 43, 41, 39, 36, 34, 32, 31, 29, 27, 26,
                    // Octave 8 (C8-B8): $0018-$000C
                    24, 23, 22, 20, 19, 18, 17, 16, 15, 14, 13, 12
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::CustomPT_1_ST: {
            // ProTracker #1 (SoundTracker)
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 390.5;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClock,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1): $0EF8-$07E0
                    3832, 3600, 3424, 3200, 3032, 2856, 2696, 2544, 2400, 2272, 2136, 2016,
                    // Octave 2 (C2-B2): $077C-$03FD
                    1916, 1800, 1712, 1600, 1516, 1428, 1348, 1272, 1200, 1136, 1068, 1021,
                    // Octave 3 (C3-B3): $03BE-$01F8
                    958, 900, 856, 800, 758, 714, 674, 636, 600, 568, 534, 504,
                    // Octave 4 (C4-B4): $01DF-$00FC
                    479, 450, 428, 400, 379, 357, 337, 318, 300, 284, 266, 252,
                    // Octave 5 (C5-B5): $00EF-$007E
                    239, 225, 214, 200, 189, 178, 168, 159, 150, 142, 133, 126,
                    // Octave 6 (C6-B6): $0077-$003F
                    119, 112, 107, 100, 94, 89, 84, 79, 75, 71, 66, 63,
                    // Octave 7 (C7-B7): $003B-$001F
                    59, 56, 53, 50, 47, 44, 42, 39, 37, 35, 33, 31,
                    // Octave 8 (C8-B8): $001D-$000F
                    29, 28, 26, 25, 23, 22, 21, 19, 18, 17, 16, 15
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::CustomPT_2_ASM: {
            // ProTracker #2 (ASM)
            options.chipChoice = uZX::ChipClockChoice::Pentagon_1_75_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClock,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1): $0D10-$06EC
                    3344, 3157, 2980, 2812, 2655, 2506, 2365, 2232, 2107, 1989, 1877, 1772,
                    // Octave 2 (C2-B2): $0688-$0376
                    1672, 1578, 1490, 1406, 1327, 1253, 1182, 1116, 1053, 994, 939, 886,
                    // Octave 3 (C3-B3): $0344-$01BB
                    836, 789, 745, 703, 664, 626, 591, 558, 527, 497, 469, 443,
                    // Octave 4 (C4-B4): $01A2-$00DD
                    418, 395, 372, 352, 332, 313, 296, 279, 263, 249, 235, 221,
                    // Octave 5 (C5-B5): $00D1-$006F
                    209, 197, 186, 176, 166, 157, 148, 140, 132, 124, 117, 111,
                    // Octave 6 (C6-B6): $0069-$0037
                    105, 99, 93, 88, 83, 78, 74, 70, 66, 62, 59, 55,
                    // Octave 7 (C7-B7): $0034-$001C
                    52, 49, 47, 44, 41, 39, 37, 35, 33, 31, 29, 28,
                    // Octave 8 (C8-B8): $001A-$000D
                    26, 25, 23, 22, 21, 20, 18, 17, 16, 15, 14, 13
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::CustomPT_3_REAL: {
            // ProTracker #3 (REAL)
            options.chipChoice = uZX::ChipClockChoice::Pentagon_1_75_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 447.0; // A4 frequency for ProTracker #3
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClock,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1): $0CDA-$06CF
                    3290, 3106, 2931, 2767, 2611, 2465, 2327, 2196, 2073, 1956, 1847, 1743,
                    // Octave 2 (C2-B2): $066D-$0367
                    1645, 1553, 1466, 1383, 1306, 1232, 1163, 1098, 1036, 978, 923, 871,
                    // Octave 3 (C3-B3): $0337-$01B4
                    823, 776, 733, 692, 653, 616, 582, 549, 518, 489, 462, 436,
                    // Octave 4 (C4-B4): $019B-$00DA
                    411, 388, 366, 346, 326, 308, 291, 274, 259, 245, 231, 218,
                    // Octave 5 (C5-B5): $00CE-$006D
                    206, 194, 183, 173, 163, 154, 145, 137, 130, 122, 115, 109,
                    // Octave 6 (C6-B6): $0067-$0036
                    103, 97, 92, 86, 82, 77, 73, 69, 65, 61, 58, 54,
                    // Octave 7 (C7-B7): $0033-$001B
                    51, 49, 46, 43, 41, 39, 36, 34, 32, 31, 29, 27,
                    // Octave 8 (C8-B8): $001A-$000D
                    26, 24, 23, 22, 20, 19, 18, 17, 16, 15, 14, 13
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::CustomVT_4_NATURAL: {
            // ProTracker #4 (Natural Cmaj/Am)
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 513.15; // A4 frequency for Natural Cmaj/Am
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::Just5LimitT45_64;
            return std::make_unique<TuningTable>(
                options.chipClock,
                makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.a4Frequency
                ),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1)
                    2880, 2700, 2560, 2400, 2304, 2160, 2025, 1920, 1800, 1728, 1620, 1536,
                    // Octave 2 (C2-B2)
                    1440, 1350, 1280, 1200, 1152, 1080, 1013, 960, 900, 864, 810, 768,
                    // Octave 3 (C3-B3)
                    720, 675, 640, 600, 576, 540, 506, 480, 450, 432, 405, 384,
                    // Octave 4 (C4-B4)
                    360, 338, 320, 300, 288, 270, 253, 240, 225, 216, 203, 192,
                    // Octave 5 (C5-B5)
                    180, 169, 160, 150, 144, 135, 127, 120, 113, 108, 101, 96,
                    // Octave 6 (C6-B6)
                    90, 84, 80, 75, 72, 68, 63, 60, 56, 54, 51, 48,
                    // Octave 7 (C7-B7)
                    45, 42, 40, 38, 36, 34, 32, 30, 28, 27, 25, 24,
                    // Octave 8 (C8-B8)
                    23, 21, 20, 19, 18, 17, 16, 15, 14, 14, 13, 12
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::RoschinFixed: {
            options.chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClock = options.chipChoice.getClockValue();
            options.a4Frequency = 433.0; // A4 frequency for Natural D#maj
            options.tonic = Scale::Tonic::DSharp;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::Just5Limit;
            return std::make_unique<TuningTable>(
                options.chipClock,
                makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.a4Frequency
                ),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    3456, 3240, 3072, 2880, 2700, 2560, 2400, 2304, 2160, 2048, 1920, 1800, // Octave 1 (C1-B1)
                    1728, 1620, 1536, 1440, 1350, 1280, 1200, 1152, 1080, 1024,  960,  900, // Octave 2 (C2-B2)
                     864,  810,  768,  720,  675,  640,  600,  576,  540,  512,  480,  450, // Octave 3 (C3-B3)
                     432,  405,  384,  360,  338,  320,  300,  288,  270,  256,  240,  225, // Octave 4 (C4-B4)
                     216,  203,  192,  180,  169,  160,  150,  144,  135,  128,  120,  113, // Octave 5 (C5-B5)
                     108,  101,   96,   90,   84,   80,  75,    72,   68,   64,   60,   56, // Octave 6 (C6-B6)
                      54,   51,   48,   45,   42,   40,  38,    36,   34,   32,   30,   28, // Octave 7 (C7-B7)
                      27,   25,   24,   23,   21,   20,  19,    18,   17,   16,   15,   14, // Octave 8 (C8-B8)
                },
                options.builtinTable.getLongLabel().data()
            );
        }
    }

    return nullptr; // Should never reach here
}

std::unique_ptr<TuningSystem> makeBuiltinTuning(BuiltinTuningType type) {
    TuningOptions options {.builtinTable = type};
    return makeBuiltinTuning(options);
}

const std::string_view getTuningTableName(BuiltinTuningType tableType) {
    return tableType.getLongLabel();
}

}