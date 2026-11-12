#pragma once

#include <cstddef>
#include <sstream>
#include <vector>
#include <array>

#include "../../../util/enumchoice.h"

namespace MoTool::uZX {

/*****************************************************************************/
/*  C++ wrapper for aychip struct and functions                              */
/*****************************************************************************/
/**
    We must divide AYChip into an interface and implementation, because implementation can vary or even be drivers for real hardware chips
    Ayumi AY/YM
    Other software emulated AY/YM
    Hardware AY/YM
    Realtime digital (maybe midi) link to a real hardware computer with some client
    Realtime software link to emulated computer with some client
*/

extern "C" {
    #include <ayumi.h>
}

constexpr double MHz = 1000000.0;

struct ChipClockEnum {
    enum Enum : uint8_t {
        NES_PAL_0_83_MHz,
        NES_NTSC_0_89_MHz,
        Amstrad_1_MHz,
        Vectrex_1_5_MHz,
        Pentagon_1_75_MHz,
        ZX_Spectrum_1_77_MHz,
        Atari_ST_2_MHz,
        Custom
    };

    static inline constexpr double clockValues[] {
         831303.5, // NES PAL
         894887,   // NES NTSC
        1000000,   // Amstrad
        1500000,   // Vectrex
        1750000,   // Pentagon
        1773400,   // ZX Spectrum 128
        2000000,   // Atari ST
              0    // Custom (user defined)
    };

    static inline constexpr std::string_view longLabels[] {
        "NES PAL 0.8313035 MHz",
        "NES NTSC 0.894887 MHz",
        "Amstrad 1 MHz",
        "Vectrex 1.5 MHz",
        "Pentagon 1.75 MHz",
        "ZX Spectrum 1.7734 MHz",
        "Atari ST 2 MHz",
        "Custom"
    };
};

class ChipClockChoice: public Util::EnumChoice<ChipClockEnum> {
public:
    using EnumChoice::EnumChoice;

    inline constexpr double getClockValue() const noexcept {
        return clockValues[asUnderlying()];
    }

    inline constexpr static auto getClockEntries() noexcept {
        return clockValues;
    }

};


struct TypeEnum {
    enum Enum : uint8_t {
        AY,
        YM
    };

    static inline constexpr std::string_view longLabels[] {
        "AY-3-8910",
        "YM2149F"
    };
};

struct LayoutEnum {
    enum Enum : uint8_t {
        ABC,
        ACB,
        BAC
    };
};

struct EnvShapeEnum {
    enum Enum : uint8_t {
        DOWN_HOLD_BOTTOM_0 = 0,
        DOWN_HOLD_BOTTOM_1,
        DOWN_HOLD_BOTTOM_2,
        DOWN_HOLD_BOTTOM_3,
        UP_HOLD_BOTTOM_4,
        UP_HOLD_BOTTOM_5,
        UP_HOLD_BOTTOM_6,
        UP_HOLD_BOTTOM_7,
        DOWN_DOWN_8,
        DOWN_HOLD_BOTTOM_9,
        DOWN_UP_A,
        DOWN_HOLD_TOP_B,
        UP_UP_C,
        UP_HOLD_TOP_D,
        UP_DOWN_E,
        UP_HOLD_BOTTOM_F,
    };

    static inline constexpr std::string_view shortLabels[] {
        "\\___",
        "\\___",
        "\\___",
        "\\___",
        "/|__",
        "/|__",
        "/|__",
        "/|__",
        "\\|\\|",
        "\\___",
        "\\/\\/",
        "\\|~~",
        "/|/|",
        "/~~~~",
        "/\\/\\",
        "/|__"
    };
};

using ChipType = MoTool::Util::EnumChoice<TypeEnum>;
using ChannelsLayout = MoTool::Util::EnumChoice<LayoutEnum>;
using EnvShape = MoTool::Util::EnumChoice<EnvShapeEnum>;

/** Where a layout puts the chip's channels A, B and C: 0 left, 1 right, at full stereo width. */
inline std::array<double, 3> channelPans(ChannelsLayout layout) {
    static constexpr std::array<std::array<double, 3>, 3> pans = {{
        {{0.0, 0.5, 1.0}},  // ABC
        {{0.0, 1.0, 0.5}},  // ACB
        {{0.5, 0.0, 1.0}}   // BAC
    }};
    return pans[static_cast<size_t>(layout)];
}


/** What channels A, B and C show on their scopes; each lane, if given, takes a pointer per channel. */
struct ScopeLanes {
    /** The chip's DAC levels 0..1: unfiltered, before the mix and the volume. */
    float* const* levels = nullptr;
    /** 1 or 0: the tone if on, else the envelope's segment if on, else 0, so noise and the envelope don't
        move the phase a scope triggers on. */
    float* const* sync = nullptr;
    /** The highest level the channel's volume reaches: 1 when the envelope drives it. */
    float* const* amplitude = nullptr;
};

class AYInterface {
public:

    // AYInterface(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::AY) ;

    AYInterface() {}
    virtual ~AYInterface() {}

    virtual auto reset(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::AY) -> void = 0;
    virtual auto muteSound() -> void = 0;
    virtual auto canChangeClock() const -> bool = 0;
    virtual auto canChangeClockContinously() const -> bool = 0;
    virtual auto getClockValues() const -> std::vector<float> = 0;
    virtual auto setSampleRate(int sampleRate) -> void = 0;
    virtual auto getSampleRate() const -> int = 0;
    virtual auto setType(ChipType type) -> void = 0;
    virtual auto getType() const -> ChipType = 0;
    virtual auto getClock() const -> double = 0;
    virtual auto setClock(double v) -> void = 0;
    virtual auto setLayoutAndStereoWidth(ChannelsLayout layout, double stereoWidth) -> void = 0;
    virtual auto getLayout() -> ChannelsLayout = 0;
    virtual auto getStereoWidth() -> double = 0;
    virtual auto setChannelPan(int chan, double pan, bool isEqp = false) -> void = 0;
    virtual auto getChannelPan(int chan) const -> double = 0;
    virtual auto setMasterVolume(float volume) -> void = 0;
    virtual auto getMasterVolume() const -> float = 0;

    static constexpr size_t numRegisters = 14;

    /** Sets R0-R13 as the real chip does: a period's fine and coarse halves are kept apart. */
    void setRegister(size_t index, unsigned char value) noexcept;
    /** R0-R13 as last written, before the channel mutes. */
    unsigned char getRegister(size_t index) const noexcept;

    // Channel muting interface (public access for ChannelMuter)
    void applyChannelMute(int chan, bool toneEnabled, bool noiseEnabled, bool envelopeEnabled, bool channelEnabled) noexcept;

    /**
        Renders the stereo mix, or channels A, B and C apart for mixing them elsewhere: filtered, DC-removed if asked,
        at master volume, and the channels' scope lanes.
    */
    virtual auto processBlockStereo(float* outLeft, float* outRight, size_t numSamples, bool removeDC = true,
                                    const ScopeLanes& scope = {}) -> void = 0;
    virtual auto processBlockSeparate(float* outA, float* outB, float* outC, size_t numSamples, bool removeDC = true,
                                      const ScopeLanes& scope = {}) -> void = 0;

protected:
    // AY functions
    virtual auto setMixer(int chan, bool tOn, bool nOn, bool eOn) -> void = 0;
    virtual auto getMixer(int chan) const -> std::tuple<bool, bool, bool> = 0;
    virtual auto setEnvelopeOn(int chan, bool on) -> void = 0;
    virtual auto setNoiseOn(int chan, bool on) -> void = 0;
    virtual auto setVolume(int chan, int volume) -> void = 0;
    virtual auto getVolume(int chan) const -> int = 0;
    virtual auto setToneOn(int chan, bool on) -> void = 0;
    virtual auto setTonePeriod(int chan, int period) -> void = 0;
    virtual auto getTonePeriod(int chan) const -> int = 0;
    virtual auto setNoisePeriod(int period) -> void = 0;
    virtual auto getNoisePeriod() const -> int = 0;
    virtual auto setEnvelopeShape(EnvShape shape) -> void = 0;
    virtual auto getEnvelopeShape() const -> EnvShape = 0;
    virtual auto setEnvelopePeriod(int period) -> void = 0;
    virtual auto getEnvelopePeriod() const -> int = 0;

private:
    // The chip plays a zero period as 1, so a period read back from it can't be composed with a new half
    std::array<unsigned char, numRegisters> regs_ {};

    int periodAt(size_t fine) const noexcept { return regs_[fine] | (regs_[fine + 1] << 8); }
    /** Sets a channel's tone, noise, volume and envelope from R7 and its volume register. */
    void setChannelFromRegisters(int chan) noexcept;
};


class AyumiEmulator : public AYInterface {
public:
    AyumiEmulator(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::YM);
    ~AyumiEmulator() override;

    auto reset(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::YM) -> void override;
    auto muteSound() -> void override;

    auto canChangeClock() const -> bool override;
    auto canChangeClockContinously() const -> bool override;
    auto getClock() const -> double override;
    auto getClockValues() const -> std::vector<float> override;
    auto setSampleRate(int sampleRate) -> void override;
    auto getSampleRate() const -> int override;
    auto setType(ChipType type) -> void override;
    auto getType() const -> ChipType override;
    auto setClock(double v) -> void override;
    auto setLayoutAndStereoWidth(ChannelsLayout layout, double stereoWidth) -> void override;
    auto getLayout() -> ChannelsLayout override;
    auto getStereoWidth() -> double override;
    auto setChannelPan(int chan, double pan, bool isEqp = false) -> void override;
    auto getChannelPan(int chan) const -> double override;

    // Chip functions
    auto setMixer(int chan, bool tOn, bool nOn, bool eOn) -> void override;
    auto getMixer(int chan) const -> std::tuple<bool, bool, bool> override;
    auto setEnvelopeOn(int chan, bool on) -> void override;
    auto setNoiseOn(int chan, bool on) -> void override;
    auto setVolume(int chan, int volume) -> void override;
    auto getVolume(int chan) const -> int override;
    auto setToneOn(int chan, bool on) -> void override;
    auto setTonePeriod(int chan, int period) -> void override;
    auto getTonePeriod(int chan) const -> int override;
    auto setNoisePeriod(int period) -> void override;
    auto getNoisePeriod() const -> int override;
    auto setEnvelopeShape(EnvShape shape) -> void override;
    auto getEnvelopeShape() const -> EnvShape override;
    auto setEnvelopePeriod(int period) -> void override;
    auto getEnvelopePeriod() const -> int override;
    auto setMasterVolume(float volume) -> void override;
    auto getMasterVolume() const -> float override;

    auto processBlockStereo(float* outLeft, float* outRight, size_t numSamples, bool removeDC = true,
                            const ScopeLanes& scope = {}) -> void override;
    auto processBlockSeparate(float* outA, float* outB, float* outC, size_t numSamples, bool removeDC = true,
                              const ScopeLanes& scope = {}) -> void override;

  private:
    /** Resets the chip with new settings and sets its registers again, so it keeps playing what it played. */
    void reconfigure(int sampleRate, double clock, ChipType type);
    /** The mode follows the call, so a reset can't leave the chip rendering fewer outputs than asked for. */
    void render(ayumi_output_mode mode, float* const* out, size_t numSamples, bool removeDC, const ScopeLanes& scope);

    ayumi Ayumi_;
    ChipType Type_;
    double ClockRate_;
    int SampleRate_;
    double Pan_[TONE_CHANNELS];
    float MasterVolume_;
    ChannelsLayout ChannelsLayout_;
    double StereoWidth_;
};

} // namespace MoTool::uZX

namespace juce {

using namespace MoTool::uZX;
using namespace MoTool::Util;

template <>
struct VariantConverter<ChipType> : public EnumVariantConverter<ChipType> {};

template <>
struct VariantConverter<ChannelsLayout> : public EnumVariantConverter<ChannelsLayout> {};

template <>
struct VariantConverter<ChipClockChoice> : public EnumVariantConverter<ChipClockChoice> {};

}