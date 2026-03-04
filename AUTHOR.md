# About the Author

I'm a C++ and Python developer with a background spanning video editing, 3D graphics, motion design (early 2000s), web design and UX (mid-2000s), backend development (2010s), and ML/data engineering (2020s).

## The ZX Spectrum Connection

I had a Soviet ZX Spectrum clone in 1990 and immediately fell in love with it. Games, BASIC, writing my first simple games. Then assembly language, self-taught. In the early 90s I moved to PCs for school and university -- VGA graphics, some 256-color effects in Pascal, wireframe 3D.

Then I saw *Second Reality* and it blew my mind. Music and effects! Impossible colorful plasma! But I didn't have the time to dive deep into demo coding back then. Life pulled me toward the web -- design, programming, the early internet.

## Return to the Demoscene

In 2017, I realized the web had become something very complex and not fun. I remembered the good old days when you knew the machine from top to bottom and could push it to its limits. I discovered that the ZX Spectrum demoscene was still alive.

I started drawing pictures in the 6912-byte ZX screen format, posting them on zxart.ee, and joined ZX Spectrum discussion groups. I made friends from the scene, first online, then in person at the DI:Halt demoparty, where my ["Little PRINC-E"](https://zxart.ee/eng/authors/r/rugrantez/little-princ-e/) ZX Spectrum graphics took 1st place. Then we coded the 256-byte intro [Stellarator](https://demozoo.org/productions/269469/), which also took 1st place at [CAFEParty 2019](https://demozoo.org/parties/3727/#competition_15561).

## Why This Tool

I wanted to make a full-featured demo production, but to start with music, I needed a good instrument -- and I don't like oldschool trackers. Over the years I accumulated prototypes: Python tools, a C++ JUCE VST plugin for AY chip, machine code timeline experiments with state caching, FUSE emulator bridging via pybind11, conversion tools involving AI. The vision for a modern composing tool came naturally from all of this.

Python taught me rapid development but lacked good UI frameworks. JUCE with C++ turned out to be the right foundation -- the ValueTree state management is robust, and it's unmatched for cross-platform audio development. When my AY chip VST3 plugin grew too complex, I needed to break it apart and inspect intermediate MIDI streams separately. That's when uZX was born.

## Contact

- Website: [pixelmatter.org](https://pixelmatter.org)
- GitHub: [Pixel-Matter](https://github.com/Pixel-Matter)
- GitHub: [RuGrantez](https://github.com/ruguevara)
- DemoZoo: [RuGrantez](https://demozoo.org/sceners/84339/)
- ZxArt: [RuGrantez](https://zxart.ee/eng/authors/r/rugrantez/)
