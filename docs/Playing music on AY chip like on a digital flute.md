# Playing music on AY chip like on a digital flute

*On just intonation tuning for AY-3-8910 music PSG chip*

The problem with equally-temperament tuning is that frequencies and periods are irrational, and given that period multipliers (clock cycle dividers) that control pitch of a tone in AY chip are integers 1-4095, there is no single perfect interval consistent through octaves, except for octave intervals itself on lower pitches.

And considering AY envelope pitches, which have periods even more discrete, divided by 16, you just can't generate tone sounds in sync with envelope.

But. Let's remember history of music. Before harpsichords and Bach and equally temperament we had "just intonation" tuning or even instruments in which pitches was generated from overtones of vibrating column of air, harmonic series. Column of air as a single wave or divided by 2, 3, 4, 5 waves and so on, is generating sounds of corresponding frequencies in inverse proportion to lengths of waves.

Somewhat similar in PSG chip we have very high fundamental frequency of the chip clock and can divide it by integer numbers N by using electronic counter that flips audio signal state every N-th clock. So this is not multiplying some low fundamental frequency by integers, but instead dividing some very high frequency. And chip clock counters is discrete. In flute or string any wave can be divided by 2, 3, 5 or 7.

Just intonation also uses integer ratios between tone frequencies and this tuning sound very natural except that you can't use all 12 steps while keeping all intervals perfect and natural, but if you can fix on some diatonic scale, and you are not modulating, you're good. European classical musicians didn't want to fix on one scale though, but that's another story.

### Just intonation 7 notes frequency ratios

```
   ┌──────────────────────1/2───────────────────────┐
   │      ┌─────5/6─────┐             ┌─────4/5─────┤
╔══╧═══╤══╧═══╤══════╤══╧═══╤══════╤══╧═══╤══════╗──┴───┬──────┐
║  A   │  B   │  C   │  D   │  E   │  F   │  G   ║  A   |  B   |
╚══╤═══╧══╤═══╧══╤═══╧══╤═══╧══╤═══╧══╤═══╧══╤═══╝──┬───┴──┬───┘
   ├─8/9──┴15/16─┼─8/9 ─┴─9/10─┼15/16─┴─8/9──┼─9/10─├─8/9──┤
   ├─────5/6─────┴─────4/5─────┼─────5/6─────┴─────4/5─────┤
   └────────────2/3────────────┴────────────2/3────────────┘
```

So it seems we can use just intonation tuning to get perfect intervals for chip "tuning"? Surely, and that was achieved long ago, for example see Ivan Roschin's work [1], it's table #4 is in Vortex tracker now.

How it's done: period (clock counters) ratios are inverses of frequency ratios. Then you find lowest common denominator and multiply all period ratios by it and voila, you've got minimal integer periods for given frequency ratios of the just intonation tuning.


### Diatonic major 5-limit tuning

|  C  |  D  |  E  |  F  |  G  |  A  |  B  |  C  | Comment                         |
|-----|-----|-----|-----|-----|-----|-----|-----|---------------------------------|
| 24  | 27  | 30  | 32  | 36  | 40  | 45  | 48  | harmonics of fundamental F 32Hz |
| 1:1 | 9:8 | 5:4 | 4:3 | 3:2 | 5:3 |15:8 | 2:1 | freq ratios                     |
| 1:1 | 8:9 | 4:5 | 3:4 | 2:3 | 3:5 | 8:15| 1:2 | period ratios                   |
| 180 | 160 | 144 | 135 | 120 | 108 | 96  | 90  | periods                         |

So to get proper ratios between 7 steps for major scale you will have this integers periods : 180, 160, 144, 135, 120, 108, 96. That's an octave number 4. But for higher three and a half more octaves (at least in MIDI) you have your notes not exact and intervals not perfect.

And more, there is the AY envelope used as a tone generator, producing sawtooth or triangle waveforms. Sounds very nice, but its clock counter can't get sounds as high as a normal square tone generators. At 16 times not as high, and that drastically worsen things even more. It's four octaves lower, you are left with octaves -2 to 0 with exact envelope tones. But also. If you want to play envelope modulated with in-sync square tone, you are limited in only single octave number 0, because square tone generator counters can not be higher than 4095 and so note can not be lower than B of octave -1.

Can we do better? Can we try to find another scales with different ratios in the hope they can get as slightly smaller values? Let's try:

### Diatonic minor 5-limit tuning

|  A  |  B  |  C  |  D  |  E  |  F  |  G  |  A  | Comment                            |
|-----|-----|-----|-----|-----|-----|-----|-----|------------------------------------|
| 120 | 135 | 144 | 160 | 180 | 192 | 216 | 240 | harmonics of fundamental B♭ ~128Hz |
| 1:1 | 9:8 | 6:5 | 4:3 | 3:2 | 8:5 | 9:5 | 2:1 | freq ratios                        |
| 1:1 | 8:9 | 5:6 | 3:4 | 2:3 | 5:8 | 5:9 | 1:2 | period ratios                      |
|  72 |  64 |  60 |  54 |  48 |  45 |  40 |  36 | periods                            |

That's between octaves 5 and 6, much better! Tonic is not A, it's some other note. If you divide periods by 2, you still have exact int values for pentatonic CDEGA

But what if we base only on pentatonic, can we do even better?

### Major pentatonic

|  C  |  D  |  E  |  G  |  A  |  C  | Comment       |
|-----|-----|-----|-----|-----|-----|---------------|
|  24 |  27 | 30  | 36  | 40  |  48 | freqs         |
| 1:1 | 9:8 | 5:4 | 3:2 | 5:3 | 2:1 | freq ratios   |
| 1:1 | 8:9 | 4:5 | 2:3 | 3:5 | 1:2 | period ratios |
|  90 |  80 |  72 |  60 |  54 |  45 | periods       |


### Minor pentatonic 1

|  A  |  C  |  D  |  E  |  G  |  A  | Comment       |
|-----|-----|-----|-----|-----|-----|---------------|
|  30 |  36 |  40 |  45 |  54 |  60 | freqs         |
| 1:1 | 6:5 | 4:3 | 3:2 | 9:5 | 2:1 | freq ratios   |
| 1:1 | 5:6 | 3:4 | 2:3 | 5:9 | 1:2 | period ratios |
|  36 |  30 |  27 |  24 |  20 |  18 | periods       |

### Minor pentatonic 2 (shifted from major diatonic)

|  A  |  C  |  D  |  E  |  G  |  A  | Comment            |
|-----|-----|-----|-----|-----|-----|--------------------|
|  20 |  24 |  27 |  30 |  36 |  40 | freqs              |
| 5:6 | 1:1 | 9:8 | 5:4 | 3:2 | 5:3 | freq ratios (same) |
| 1:1 | 6:5 |27:20| 3:2 | 9:5 | 2:1 | freq ratios        |
| 6:5 | 1:1 | 8:9 | 4:5 | 2:3 | 3:5 | period ratios      |
|  54 |  45 |  40 |  36 |  30 |  27 | periods            |

### Some other pentatonic scales ratios for reference

Blues minor pentatonic: 15:18:20:24:27:30 freqs E G A C D E
Blues major pentatonic: 24:27:32:36:40:48 freqs G A C D E G
Egyptian, suspended: 24:27:32:36:42:48 freqs D E G A C D
Another one: 42:48:56:63:72 so freq ratios are 1:1, 8:7, 4:3, 3:2, 12:7
Some more: 16:19:21:24:28 so freq ratios are 1:1, 19:16, 21:16, 3:2, 7:4

### What we've got based on pentatonic

For Minor pentatonic 1 we've got periods as high as 36, 30, 27, 24, 20. Octave 6 and 7. Wow! That means AY envelope can walk on pentatonic across three and a half octaves and be in tune!

Lets build full period table for 12 steps filling all itermediate steps with rational numbers. On lower octaves that numbers can become integers.

## 12 tone scale 7-limit tuning

|  C  | C#  |  D  |  D# |  E  |  F  |  F# |  G  |  G# |  A  |  A# |  B  |  C  | Comment                  |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------------------------|
| 120 |128  | 135 | 144 | 150 | 160 | 168 | 180 | 192 | 200 | 216 | 255 | 240 | freqs                    |
| 1:1 |16:15| 9:8 | 6:5 | 5:4 | 4:3 | 7:5 | 3:2 | 8:5 | 5:3 | 9:5 |15:8 | 2:1 | freq ratios              |
| 1:1 |15:16| 8:9 | 5:6 | 4:5 | 3:4 | 5:7 | 2:3 | 5:8 | 3:5 | 5:9 |8:15 | 1:2 | period ratios            |
| 5040| 4725| 4480| 4200| 4032| 3780| 3600| 3360| 3150| 3024| 2800| 2688| 2520| periods                  |
| 180 |168.8| 160 | 150 | 144 | 135 |128.6| 120 |112.5| 108 | 100 |  96 |  90 | periods for diatonic     |
|  36 |33.75|  32 |  30 | 28.8|  27 |25.71|  24 | 22.5| 21.6|  20 | 19.2|  18 | periods for minor 5tonic |

Major and minor are ok here and actually they tuned exactly by 5-limit.

### Full periods table for 12 tone scale 7-limit tuning

Based on minor pentatonic ACDEG
| [A]  |  A#  |  B   | [C]  |  C#  | [D]  |  D#  | [E]  |  F   |  F#  | [G]  |  G#  | Exact is in '[]' |
|------|------|------|------|------|------|------|------|------|------|------|------|------------------|
|      |      | 4095 | 3840!| 3686 | 3456!| 3291 | 3072!| 2880!| 2765 | 2560!| 2458 |                  |
| 2304!| 2160!| 2048!| 1920!| 1843 | 1728!| 1646 | 1536!| 1440!| 1382 | 1280!| 1229 | 12 steps         |
| 1152!| 1080 | 1024!|  960!|  922 |  864!|  823 |  768!|  720!|  691 |  640!|  614 | 12 steps         |
|  576!|  540 |  512!|  480!|  461 |  432!|  411~|  384!|  360 |  346~|  320!|  307 | 10 steps minor+  |
|  288!|  270 |  256!|  240!|  230~|  216 |  206~|  192!|  180 |  173~|  160!|  154~| 8 steps minor+   |
|  144!|  135 |  128!|  120 |  115~|  108 |  103~|   96!|   90 |   86~|   80!|   77~| 8 steps minor+   |
|   72 |   68~|   64!|   60 |   58~|   54 |   51~|   48!|   45 |   43~|   40 |   38~| 7 steps minor    |
|   36 |   34~|   32!|   30 |   29~|   27 |   26~|   24 |   22~|   22~|   20 |   19~| 6 steps pent+    |
|   18 |   17~|   16!|   15 |   14~|   14~|   13~|   12 |   11~|   11~|   10 |   10~| 4 steps pent-    |
|    9 |    8~|    8 |    8~|    7~|    7~|    6~|    6 |    6~|    5~|    5 |    5~| 4 steps ABEG     |
|    4~|    4~|    4 |    4~|    4~|    3~|    3~|    3 |    3~|    3~|    2~|    2~| 2 steps BE       |
|    2~|    2~|    2 |    2~|    2~|    2~|    2~|    2~|    1~|    1~|    1~|    1~| 1 step  B        |


### And finally... one more thing...

What if we take another approach, not from the scales, but from lowest period values? What if we construct achievable scale and tuning based on the lowest possible periods?

So the only interval we can get first is the octave.

| C   | C#  |  D  |  D# |  E  |  F  |  F# |  G  |  G# |  A  |  A# |  B  |  C  |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
| 1:1 |     |     |     |     |     |     |     |     |     |     |     | 1:2 |
|  2  |     |     |     |     |     |     |     |     |     |     |     |  1  |
|  1  |     |     |     |     |     |     |     |     |     |     |     |     |

Then we have to make a fifth, it's 2:3 in terms of periods, so our tonic must be divisible by 3 now on. Shifting to E. Why E? Because we will get exact steps without sharps in E.

|  E  |  F  | F#  |  G  |  G# |  A  | A#  |  B  |  C  | C#  |  D  | D#  |  E  | Chords            |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-------------------|
| 1:1 |     |     |     |     |     |     | 2:3 |     |     |     |     | 1:2 |                   |
|  6  |     |     |     |     |     |     |  4  |     |     |     |     |  3  | E5                |
|  3  |     |     |     |     |     |     |  2  |     |     |     |     |     | + stacked fifth B |
|     |     |     |     |     |     |     |  1  |     |     |     |     |     | + stacked fifth B |


Now lets make some real chord, a triad. Major third is 4:5, minor is 5:6. But the major third introduces factor of 5 that is not contained in the tonic. Lets start with minor. And lets change the tonic to A, it's relative anyway. True tonic label can be calculated from the frequency of the chip and period of 3.

|  E  |  F  | F#  |  G  |  G# |  A  | A#  |  B  |  C  | C#  |  D  | D#  |  E  | Chords            |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-------------------|
| 1:1 |     |     | 5:6 |     |     |     | 2:3 |     |     |     |     | 1:2 |                   |
|  6  |     |     |  5  |     |     |     |  4  |     |     |     |     |  3  | Em                |
|  3  |     |     |     |     |     |     |  2  |     |     |     |     |     | + stacked fifth B |
|     |     |     |     |     |     |     |  1  |     |     |     |     |     | + stacked fifth B |

Forth is 3:4

|  E  |  F  | F#  |  G  |  G# |  A  | A#  |  B  |  C  | C#  |  D  | D#  |  E  | Chords            |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-------------------|
| 1:1 |     |     | 5:6 |     | 3:4 |     | 2:3 |     |     |     |     | 1:2 |                   |
| 12  |     |     | 10  |     |  9  |     |  8  |     |     |     |     |  6  | Gdim              |
|  6  |     |     |  5  |     |     |     |  4  |     |     |     |     |  3  | Em                |
|  3  |     |     |     |     |     |     |  2  |     |     |     |     |     | + stacked fifth B |
|     |     |     |     |     |     |     |  1  |     |     |     |     |     | + stacked fifth B |

Move on to the next octave. What else can we have here? 15 to 24 is 5:8, minor sixth. Also we can have 7:8, we can account this as a major second, why not?

|  E  |  F  | F#  |  G  |  G# |  A  | A#  |  B  |  C  | C#  |  D  | D#  |  E  | Chords            |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-------------------|
| 1:1 |     | 7:8 | 5:6 |     | 3:4 |     | 2:3 | 5:8 |     |     |     | 1:2 |                   |
| 24  |     |     | 20  |     | 18  |     | 16  | 15  |     |     |     | 12  | Am, C             |
| 12  |     |     | 10  |     |  9  |     |  8  |     |     |     |     |  6  | G˚                |
|  6  |     |     |  5  |     |     |     |  4  |     |     |     |     |  3  | Em                |
|  3  |     |     |     |     |     |     |  2  |     |     |     |     |     | + stacked fifth B |
|     |     |     |     |     |     |     |  1  |     |     |     |     |     | + stacked fifth B |

Moving on. We would get the minor seventh 5:9, but 27 to 48 is not in the exact ratio of 5:9. So is for the major second B, 8:9. But can we have some more steps? Let's see: minor second at 15:16 and minor seventh at 9:16

So it's a minor scale with lowered second step: sTTTsTT. Phrygian!

|  E  |  F  | F#  |  G  |  G# |  A  | A#  |  B  |  C  | C#  |  D  | D#  |  E  | Chords              |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|---------------------|
| 1:1 |15:16|     | 5:6 |     | 3:4 |     | 2:3 | 5:8 |     | 9:16|     | 1:2 | B˚                  |
| 48  | 45  |     | 40  |     | 36  |     | 32  | 30  |     | 27  |     | 24  | Dm, F, G (wolf 5th) |
| 24  |     |     | 20  |     | 18  |     | 16  | 15  |     |     |     | 12  | Am, C               |
| 12  |     |     | 10  |     |  9  |     |  8  |     |     |     |     |  6  | G˚                  |
|  6  |     |     |  5  |     |     |     |  4  |     |     |     |     |  3  | Em                  |
|  3  |     |     |     |     |     |     |  2  |     |     |     |     |     | + stacked fifth B   |
|     |     |     |     |     |     |     |  1  |     |     |     |     |     | + stacked fifth B   |

Do we have wolf fifth? It's a just intonation tuning, so yeah. And happily, only one: G-D!


```
         ┌──────────2:3──────────┬───────────2:3─────────┬──────────27:40────────┐
   ┌──────────2:3──────────┐     │           ┌───────2:3─────────────┐           │
   │     │                 │     │           │           │           │           │
╔══╧══╤══╧══╤═════╤═════╤══╧══╤══╧══╤═════╗──┴──┬─────┬──┴──┬─────┬──┴──┬─────┬──┴──┬─────┐
║  E  │  F  │  G  │  A  │  B  │  C  │  D  ║  E  │  F  │  G  │  A  │  B  │  C  │  D  │  E  │
║ 1:1 │15:16│ 5:6 │ 3:4 │ 2:3 │ 5:8 │ 9:16║ 1:2 │15:32│ 5:12│ 3:8 │ 1:3 │ 5:16│ 9:32│ 1:4 │
╚═════╧═════╧══╤══╧══╤══╧═════╧═════╧══╤══╝──┬──┴─────┴─────┴──┬──┴──┬──┴─────┴─────┴──┬──┘
               │     │                 │     │                 │     │                 │
               │     └──────────2:3──────────┴──────────2:3──────────┘                 │
               └─────────27:40─────────┴──────────2:3──────────┴──────────2:3──────────┘
```

What other modes of C major could we play? All of them but one, if you are not supposed to play G.
You can play chords: C, Dm, Em, F, G˚, Am, B˚.

* C Eonian (C major scale)
* D Dorian
* E Phrygian
* F Lydian
* no G Mixolydian
* A Aeolian (A minor scale)
* B Locrian

Lets complete the whole table by filling empty cells with non-integer, rational periods

|  E  |  F  | F#  |  G  | G#  |  A  | A#  |  B  |  C  | C#  |  D  | D#  |  E  |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
| 1:1 |15:16| 8:9 | 5:6 | 4:5 | 3:4 | 5:7 | 2:3 | 5:8 | 3:5 | 9:16| 8:15| 1:2 |
| 48  | 45  |42.66| 40  | 38.4| 36  |34.29| 32  | 30  | 28.8| 27  | 25.6| 24  |

Note: 34.29 is 34.285714(285714) = 240/7, but 34.29 has enough precision anyway

### All octaves table for 12 steps of achievable tone frequencies

Assuming clock rate of 1.773MHz for ZX Spectrum 128 we get 288.64Hz for period of 384.

|  [E] |  [F] |  F#  |  [G] |  G#  |  [A] |  A#  |  [B] |  [C] |  C#  |  [D] |  D#  |
|------|------|------|------|------|------|------|------|------|------|------|------|
|      |      |      |      |      |      |      | 4095 | 3840!| 3686 | 3456!| 3277 |
| 3072!| 2880!| 2730 | 2560!| 2458 | 2304!| 2194 | 2048!| 1920!| 1843 | 1728!| 1638 |
| 1536!| 1440!| 1365 | 1280!| 1229 | 1152!| 1097 | 1024!|  960!|  922 |  864!|  819 |
|  768!|  720!|  683 |  640!|  614 |  576!|  549 |  512!|  480!|  461 |  432!|  410 |
|  384!|  360 |  341 |  320!|  307 |  288!|  274 |  256!|  240!|  230 |  216 |  205 |
|  192!|  180 |  171 |  160!|  154 |  144!|  137 |  128!|  120 |  115 |  108 |  102 |
|   96!|   90 |   85 |   80!|   77 |   72 |   69~|   64!|   60 |   58~|   54 |   51 |
|   48!|   45 |   43~|   40 |   38~|   36 |   34~|   32!|   30 |   29~|   27 |   26~|
|   24 |   22~|   21~|   20 |   19~|   18 |   17~|   16!|   15 |   14~|   14~|   13~|
|   12 |   11~|   11~|   10 |   10~|    9 |    9~|    8 |    8~|    7~|    7~|    6~|
|    6 |    6~|    5~|    5 |    5~|    4~|    4~|    4 |    4~|    4~|    3~|    3~|
|    3 |    3~|    3~|    2~|    2~|    2~|    2~|    2 |    2~|    2~|    2~|    2~|
|    2~|    1~|    1~|    1~|    1~|    1~|    1~|    1 |    1~|    1~|    1~|    1~|

> ~ — value of the period is not exact in relation to natural tuning
> ! — value of the period is exactly in sync with AY envelope for this pitch (divisible by 16)

In this E Phrygian scale with such period values you can play some chords and melodies with perfect intervals up to the last 8th MIDI octave. And use envelope with perfect intervals up to 4th octave with a half! And even some more, if you are careful not to step on inexact notes.

### References

* https://zxpress.ru/zxnet/music.zx/2503
* https://en.wikipedia.org/wiki/Five-limit_tuning


### Helper code

```python
import numpy as np
periods = np.array([48, 45, 42.66, 40, 38.4, 36, 240/7, 32, 30, 28.8, 27, 25.6])

def fmt_period(x):
    res = "{:d}".format(int(round(x)))
    if abs(x / int(round(x)) - 1) > 5/1200:  # octave is 1200 cents
        return res + "~"
    elif int(round(x)) % 16 == 0:
        return res + "!"
    else:
        return res + " "

def fmt_pad_period(x):
    return "{:>5s}".format(fmt_period(x))

def print_table(o1, o2):
    for octave in range(o1, o2, -1):
        print("| ", end='')
        print("| ".join(map(fmt_pad_period, periods * 2 ** octave)), end='')
        print("| ")

print_table(7, -6)
```
