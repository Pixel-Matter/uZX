# µZX -- Chiptune Music Tool

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENCE.md)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux%20%7C%20iOS%20%7C%20Android-lightgrey)]()

A modern tool for composing and playing back chiptune music for platforms using the AY-3-8910/YM2149 sound chips -- ZX Spectrum, Amstrad CPC, MSX, Atari ST, and others. While the name references ZX Spectrum, the architecture is chip-agnostic and designed to support additional sound chips in the future. Built with JUCE, targeting macOS, Windows, and Linux, with iOS and Android possible. Features a timeline-based DAW interface with precise chip emulation.

```text
        █▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌
        █▌  █▌    █▌      █▌    █▌█▌    █▌
  █▌█▌█▌█▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌█▌█▌█▌
        █▌
                        |   |
    ,---.---.  ,---.  --|---|--  ,---.   ,---
    |   |   |       |   |   |   |    |  |
        |   |   ,---|   |   |   |---'   |
            |  |    |   |   |   |       |
               `---'    |   |    `---'
                        `-- `--
         ,--------.   ,---.
         `---/ / / \ / / /
            / / / \ ' / /
    B8   88  / /   / / /    Copyright 2025-2026
    B8   88   /   / / . \     pixelmatter.org
    B8.  88. /---/ / / \ \
    B8"oo""8o --'---'   `-'
    BP
```

## Features

**Three applications from one codebase:**

- **µZX Studio** -- full-featured PSG music editor with timeline, instruments, and effects. Includes µZX Tuning.
- **µZX Player** -- lightweight playback for `.uzx` projects and `.psg` files
- **µZX Tuning** -- standalone tuning table editor for exploring chip tuning systems

**Music composition:**

- Timeline-based arranging with tracks, clips, and automation
- ChipInstrument plugin with ADSR envelopes for AY chip
- MIDI to PSG conversion with configurable tuning systems
- Real-time AY-3-8910/YM2149 emulation via [ayumi](https://github.com/true-grue/ayumi)

**Tuning systems:**

- 12-TET, 5-Limit Just Intonation, and custom tuning tables
- Visual tuning grid with note tooltips and scale highlighting
- Configurable chip clock frequency and A4 reference
- CSV export of tuning tables

**Visualization:**

- PSG clip display with notes, noise/envelope decoration, and note pitch scale
- Integrated oscilloscope displays per AY channel
- Standalone scope plugin

## Download

See [Releases](https://github.com/Pixel-Matter/uZX/releases) for pre-built binaries.

## Building from Source

```bash
git clone --recursive https://github.com/Pixel-Matter/uZX.git
cd uZX
cmake -S . -B build
cmake --build build
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed build instructions, prerequisites, and build targets.

## Documentation

- [Design](docs/Design.md) -- architecture overview
- [Tuning Systems](docs/Tuning%20Systems.md) -- tuning system design
- [µZX Player](docs/uzx-player.md) -- Player variant architecture
- [Parameter Binding](docs/Parameter%20binding.md) -- parameter binding system
- [OVM Design Pattern](docs/OVM%20Design%20pattern.md) -- state management
- [Roadmap](docs/ROADMAP.md) -- planned features and release milestones

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for build setup, code style, testing, and how to submit changes.

## Credits

- [Tracktion Engine](https://github.com/Tracktion/tracktion_engine/) -- audio engine foundation
- [JUCE](https://juce.com/) -- cross-platform C++ framework
- [ayumi](https://github.com/true-grue/ayumi) by Peter Sovietov (true-grue) -- AY-3-8910/YM2149 emulation

Greets to diver, spke, n1k-o, bfox, wbcbz7, Pator, Megus, Volutar and all ZX Spectrum musicians and demosceners!

![Powered by Tracktion Engine](third_party/tracktion_engine/tutorials/images/tracktion_engine_powered.png)

## Vision

µZX is the music subsystem of the larger **MoTool** project -- a demotool for retrocomputer demoscene production that will eventually combine music, graphics, effects, and machine code in a single timeline. See [docs/Vision.md](docs/Vision.md) for the full picture.

## About

See [AUTHOR.md](AUTHOR.md) for the story behind this project.

## License

[GPL-3.0](LICENCE.md)
