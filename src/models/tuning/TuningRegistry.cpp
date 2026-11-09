#include "TuningRegistry.h"

#include "TemperamentSystem.h"
#include "TuningSystemBase.h"
#include "AutoTuning.h"
#include <memory>

namespace MoTool {

std::unique_ptr<TuningSystem> makeBuiltinTuning(TuningOptions& options) {
    switch (options.builtinTable) {
        case BuiltinTuningType::EqualTemperament: {
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;

            return std::make_unique<AutoTuning>(
                options.chipClockFreq, std::make_unique<EqualTemperamentTuning>(options.a4Frequency)
            );
        }

        case BuiltinTuningType::Just5Limit: {
            // Implement Just Intonation 5-limit tuning
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 433.0;
            options.tonic = Scale::Tonic::D;
            options.scaleType = Scale::ScaleType::Phrygian;
            options.tuningSystemType = TuningSystemType::Just5Limit;
            return std::make_unique<AutoTuning>(
                options.chipClockFreq, makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.scaleType,
                    options.a4Frequency
                )
            );
        }

        case BuiltinTuningType::Just5Limit2: {
            // Implement Just Intonation 5-limit T=45:64 tuning
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 433.0;
            options.tonic = Scale::Tonic::D;
            options.scaleType = Scale::ScaleType::Phrygian;
            options.tuningSystemType = TuningSystemType::Just5LimitT45_64;
            return std::make_unique<AutoTuning>(
                options.chipClockFreq, makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.scaleType,
                    options.a4Frequency
                )
            );
        }

        case BuiltinTuningType::Pythagorean: {
            // Implement Pythagorean tuning
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 433.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::Pythagorean;
            auto referenceTuning = makeReferenceTuningSystem(
                options.tuningSystemType,
                options.tonic,
                options.scaleType,
                options.a4Frequency
            );
            auto autoTuning = std::make_unique<AutoTuning>(
                options.chipClockFreq, std::move(referenceTuning)
            );
            return autoTuning;
        }

        // Tracker tables, as the PT3 format description v3.7x by Alone Coder with notes by
        // S.V. Bulba (PTDOC3.7) and Vortex Tracker's ToneTables.csv give them, with the clock
        // and A4 each was computed for as far as those sources say. The clock and A4 only set
        // the reference the Tuning window measures notes against; the periods are the table.
        // PT 3.3 and older play tables #0 and #2 from earlier lists (Bulba's PTxPlay picks them
        // for a version digit below 4); #1 is the same in every version, and the older #3
        // differs from the newer one in a single period.
        case BuiltinTuningType::CustomPT_0_PT: {
            // ProTracker #0, the original table: 12TET A4 = 440 Hz at 1.625 MHz, rounded to the
            // nearest period. PTDOC3.7: "0=Pro Tracker (aka 1625000)", "Doesn't fit good to any
            // AY frequency": no machine runs at 1.625 MHz.
            options.chipClockFreq = 1625000.0;
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
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
            // ProTracker #1, Sound Tracker's table by way of Pro Tracker 2. PTDOC3.7: "fairly
            // good for 1773400 Hz, but has offset by 2 half-tones down. So C-2 sounds as
            // standart A#1", so its A4 sounds as G4 of 12TET at 440 Hz.
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 392.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
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
            // ProTracker #2, "ASM or PSC", as PT 3.4x and later play it: 12TET A4 = 440 Hz at the
            // Pentagon's 1.75 MHz. PTDOC3.7: "Modern table #2 fits perfectly for 1750000".
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::Pentagon_1_75_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
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
            // ProTracker #3, "RealSound": table #0 a semitone down, a "bad attempt to move table 0
            // to 1.75MHz" (PTDOC3.7). It is 12TET A4 = 440 Hz at 1.625 MHz * 2^(1/12), which
            // puts A4 at 447.25 Hz on the Pentagon clock it was meant for.
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::Pentagon_1_75_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 447.25;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
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
            // Vortex Tracker's table #4 by Ivan Roschin: 5-limit just intonation on C, made for
            // 1520640 Hz, where A4 is exactly 440 Hz. ZXTune: "Natural scaled frequency table
            // (best for 1520640Hz)".
            options.chipClockFreq = 1520640.0;
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::Just5LimitT45_64;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
                makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.scaleType,
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
            // Ivan Roschin's fixed natural table, 5-limit just intonation on D#. No source for
            // the clock it was made for; A4 = 433 Hz at the ZX clock fits its periods.
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 433.0; // A4 frequency for Natural D#maj
            options.tonic = Scale::Tonic::DSharp;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::Just5Limit;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
                makeReferenceTuningSystem(
                    options.tuningSystemType,
                    options.tonic,
                    options.scaleType,
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

        case BuiltinTuningType::CustomPT_0_PT33: {
            // ProTracker #0 as PT 3.3 and older play it: the same 12TET A4 = 440 Hz at 1.625 MHz,
            // rounded down instead of to the nearest period (ToneTables.csv, column 0_33_34r).
            options.chipClockFreq = 1625000.0;
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1)
                    3105, 2931, 2766, 2611, 2464, 2326, 2195, 2072, 1956, 1846, 1742, 1645,
                    // Octave 2 (C2-B2)
                    1552, 1465, 1383, 1305, 1232, 1163, 1097, 1036, 978, 923, 871, 822,
                    // Octave 3 (C3-B3)
                    776, 732, 691, 652, 616, 581, 548, 518, 489, 461, 435, 411,
                    // Octave 4 (C4-B4)
                    388, 366, 345, 326, 308, 290, 274, 259, 244, 230, 217, 205,
                    // Octave 5 (C5-B5)
                    194, 183, 172, 163, 154, 145, 137, 129, 122, 115, 108, 102,
                    // Octave 6 (C6-B6)
                    97, 91, 86, 81, 77, 72, 68, 64, 61, 57, 54, 51,
                    // Octave 7 (C7-B7)
                    48, 45, 43, 40, 38, 36, 34, 32, 30, 28, 27, 25,
                    // Octave 8 (C8-B8)
                    24, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::CustomPT_2_ASM33: {
            // ProTracker #2 as PT 3.3 and older play it: the ASM / PSC table "layed along perfect
            // values for 1773400 Hz" (PTDOC3.7), 12TET A4 = 440 Hz at the ZX Spectrum 128 clock
            // to within a period (ToneTables.csv, column 2_34r).
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 440.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1)
                    3390, 3200, 3020, 2850, 2690, 2540, 2396, 2262, 2136, 2016, 1902, 1796,
                    // Octave 2 (C2-B2)
                    1695, 1600, 1510, 1425, 1345, 1270, 1198, 1131, 1068, 1008, 951, 898,
                    // Octave 3 (C3-B3)
                    847, 800, 755, 712, 673, 635, 599, 566, 534, 504, 476, 449,
                    // Octave 4 (C4-B4)
                    424, 400, 377, 356, 336, 317, 300, 283, 267, 252, 238, 224,
                    // Octave 5 (C5-B5)
                    212, 200, 189, 178, 168, 159, 150, 141, 133, 126, 119, 112,
                    // Octave 6 (C6-B6)
                    106, 100, 94, 89, 84, 80, 75, 71, 67, 63, 60, 56,
                    // Octave 7 (C7-B7)
                    53, 50, 47, 45, 42, 40, 38, 36, 34, 32, 30, 29,
                    // Octave 8 (C8-B8)
                    27, 26, 25, 24, 21, 20, 19, 18, 17, 16, 15, 14
                },
                options.builtinTable.getLongLabel().data()
            );
        }

        case BuiltinTuningType::CustomST: {
            // Sound Tracker's own table, as STC and STP modules play it (zxtune's "SoundTracker").
            // PT #1 is its copy in Pro Tracker 2 and 3, with B2 and A#4 changed (1021, 266); the
            // reference is PT #1's.
            uZX::ChipClockChoice chipChoice = uZX::ChipClockChoice::ZX_Spectrum_1_77_MHz;
            options.chipClockFreq = chipChoice.getClockValue();
            options.a4Frequency = 392.0;
            options.tonic = Scale::Tonic::C;
            options.scaleType = Scale::ScaleType::IonianOrMajor;
            options.tuningSystemType = TuningSystemType::EqualTemperament;
            return std::make_unique<TuningTable>(
                options.chipClockFreq,
                std::make_unique<EqualTemperamentTuning>(options.a4Frequency),
                24, // Starting at MIDI note 24 (C1)
                std::vector<int> {
                    // Octave 1 (C1-B1)
                    3832, 3600, 3424, 3200, 3032, 2856, 2696, 2544, 2400, 2272, 2136, 2016,
                    // Octave 2 (C2-B2)
                    1916, 1800, 1712, 1600, 1516, 1428, 1348, 1272, 1200, 1136, 1068, 1008,
                    // Octave 3 (C3-B3)
                    958, 900, 856, 800, 758, 714, 674, 636, 600, 568, 534, 504,
                    // Octave 4 (C4-B4)
                    479, 450, 428, 400, 379, 357, 337, 318, 300, 284, 267, 252,
                    // Octave 5 (C5-B5)
                    239, 225, 214, 200, 189, 178, 168, 159, 150, 142, 133, 126,
                    // Octave 6 (C6-B6)
                    119, 112, 107, 100, 94, 89, 84, 79, 75, 71, 66, 63,
                    // Octave 7 (C7-B7)
                    59, 56, 53, 50, 47, 44, 42, 39, 37, 35, 33, 31,
                    // Octave 8 (C8-B8)
                    29, 28, 26, 25, 23, 22, 21, 19, 18, 17, 16, 15
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