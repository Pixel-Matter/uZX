#pragma once

#include <cstddef>
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

    ## TODO
    - Output to thee separate channels instead of mixing them to stereo panorama
*/

namespace {
    extern "C" {
        #include <ayumi.h>
    }
}

class AYInterface {
public:
    struct TypeEnum {
        enum Enum {
            AY,
            YM
        };
        static inline constexpr std::string_view labels[] {
            "AY-3-8910",
            "YM2149F"
        };
    };
    using ChipType = MoTool::Util::EnumChoice<TypeEnum>;

    struct LayoutEnum {
        enum Enum {
            ABC,
            ACB,
            BAC,
            BCA,
            CAB,
            CBA
        };
        static inline constexpr std::string_view labels[] {
            "ABC",
            "ACB",
            "BAC",
            "BCA",
            "CAB",
            "CBA"
        };
    };
    using ChannelsLayout = MoTool::Util::EnumChoice<LayoutEnum>;
    using PanValues = std::array<double, 3>;

    static inline constexpr std::array<std::array<double, 3>, 6> channelPans = {{
        {{0.0, 0.5, 1.0}},  // ABC
        {{0.0, 1.0, 0.5}},  // ACB
        {{0.5, 0.0, 1.0}},  // BAC
        {{1.0, 0.0, 0.5}},  // BCA
        {{0.5, 1.0, 0.0}},  // CAB
        {{1.0, 0.5, 0.0}}   // CBA
    }};

    struct EnvShapeEnum {
        enum Enum {
            DOWN_HOLD_BOTTOM_0,
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
        static inline constexpr std::string_view labels[] {
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
            "/|__",
        };
    };
    using EnvShape = MoTool::Util::EnumChoice<EnvShapeEnum>;

    // AYInterface(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::AY) ;

    AYInterface() {}
    virtual ~AYInterface() {}

    virtual auto reset(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::AY) -> void = 0;
    virtual auto resetSound() -> void = 0;
    virtual auto canChangeClock() const -> bool = 0;
    virtual auto canChangeClockContinously() const -> bool = 0;
    virtual auto getClockValues() const -> std::vector<float> = 0;
    virtual auto setSampleRate(int sampleRate) -> void = 0;
    virtual auto getSampleRate() const -> int = 0;
    virtual auto setType(ChipType type) -> void = 0;
    virtual auto getType() const -> ChipType = 0;
    virtual auto getClock() const -> double = 0;
    virtual auto setClock(double v) -> void = 0;
    virtual auto setPan(int chan, double pan, bool isEqp = false) -> void = 0;
    virtual auto getPan(int chan) const -> double = 0;
    virtual auto setMasterVolume(float volume) -> void = 0;
    virtual auto getMasterVolume() const -> float = 0;

    // TODO maybe just store registers in an ordinary byte array and after setting them update the chip?
    void setRegister(size_t index, unsigned char value) noexcept;

    // Processing
    virtual auto processBlock(float* outLeft, float* outRight, size_t numSamples, bool removeDC = true, size_t stride = 1) -> void = 0;

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
    inline void setFineTonePeriod(int chan, unsigned char fine) noexcept {
        const int oldPeriod = getTonePeriod(chan);
        setTonePeriod(chan, (oldPeriod & 0xff00) | (fine & 0xff));
    }
    inline void setCoarseTonePeriod(int chan, unsigned char coarse) noexcept {
        const int oldPeriod = getTonePeriod(chan);
        setTonePeriod(chan, (oldPeriod & 0xff) | (coarse << 8));
    }
    // TODO Actually to be able to record and set R0-R13 registers
    // we need to do things in reverse: setPeriod, setMixer, setVolume,
    // setEnvelope, setNoise, setTone should work with R0-R13 registers
    // And AY implementation should implement functions setR0-R13 or
    // setFineTonePeriod, setCoarseTonePeriod, setNoisePeriod, setMixer, setVolume, setEnvelope
    inline void setR0(unsigned char finePeriodA)   noexcept { setFineTonePeriod  (0, finePeriodA); }
    inline void setR1(unsigned char coarsePeriodA) noexcept { setCoarseTonePeriod(0, coarsePeriodA); }
    inline void setR2(unsigned char finePeriodB)   noexcept { setFineTonePeriod  (1, finePeriodB); }
    inline void setR3(unsigned char coarsePeriodB) noexcept { setCoarseTonePeriod(1, coarsePeriodB); }
    inline void setR4(unsigned char finePeriodC)   noexcept { setFineTonePeriod  (2, finePeriodC); }
    inline void setR5(unsigned char coarsePeriodC) noexcept { setCoarseTonePeriod(2, coarsePeriodC); }
    inline void setR6(unsigned char noisePeriod)   noexcept { setNoisePeriod     (   noisePeriod); }
    inline void setR7(unsigned char mixer) noexcept {
        //   7   |   6   |    5    |    4    |    3    |   2    |   1    |   0
        // I/O B | I/O A | Noise C | Noise B | Noise A | Tone C | Tone B | Tone A
        const bool Atone = !(mixer & 1);
        const bool Btone = !((mixer >> 1) & 1);
        const bool Ctone = !((mixer >> 2) & 1);
        const bool Anoise = !((mixer >> 3) & 1);
        const bool Bnoise = !((mixer >> 4) & 1);
        const bool Cnoise = !((mixer >> 5) & 1);
        setToneOn(0, Atone);
        setToneOn(1, Btone);
        setToneOn(2, Ctone);
        setNoiseOn(0, Anoise);
        setNoiseOn(1, Bnoise);
        setNoiseOn(2, Cnoise);
    }
    inline void setR8 (unsigned char volumeA) noexcept { setVolume(0, volumeA & 0x0f); setEnvelopeOn(0, volumeA & 0x10); }
    inline void setR9 (unsigned char volumeB) noexcept { setVolume(1, volumeB & 0x0f); setEnvelopeOn(1, volumeB & 0x10); }
    inline void setR10(unsigned char volumeC) noexcept { setVolume(2, volumeC & 0x0f); setEnvelopeOn(2, volumeC & 0x10); }
    inline void setR11(unsigned char envFinePeriod) noexcept {
        const int oldPeriod = getEnvelopePeriod();
        setEnvelopePeriod((oldPeriod & 0xff00) | (envFinePeriod & 0xff));
    }
    inline void setR12(unsigned char envCoarsePeriod) noexcept {
        const int oldPeriod = getEnvelopePeriod();
        setEnvelopePeriod((oldPeriod & 0xff) | (envCoarsePeriod << 8));
    }
    inline void setR13(unsigned char envShape) noexcept {
        setEnvelopeShape(static_cast<EnvShape>(envShape));
    }
};


class AyumiEmulator : public AYInterface {
public:
    AyumiEmulator(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::YM);
    ~AyumiEmulator() override;

    auto reset(int sampleRate = 44100, double clock = 2000000, ChipType type = TypeEnum::YM) -> void override;
    auto resetSound() -> void override;

    auto canChangeClock() const -> bool override;
    auto canChangeClockContinously() const -> bool override;
    auto getClock() const -> double override;
    auto getClockValues() const -> std::vector<float> override;
    auto setSampleRate(int sampleRate) -> void override;
    auto getSampleRate() const -> int override;
    auto setType(ChipType type) -> void override;
    auto getType() const -> ChipType override;
    auto setClock(double v) -> void override;
    auto setPan(int chan, double pan, bool isEqp = false) -> void override;
    auto getPan(int chan) const -> double override;

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

    // Processing
    auto processBlock(float* outLeft, float* outRight, size_t numSamples, bool removeDC = true, size_t stride = 1) -> void override;

private:
    ayumi Ayumi_;
    ChipType Type_;
    double ClockRate_;
    int SampleRate_;
    double Pan_[TONE_CHANNELS];
    float MasterVolume_;
};

} // namespace MoTool::uZX
