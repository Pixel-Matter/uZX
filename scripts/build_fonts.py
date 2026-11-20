#!/usr/bin/env python3
"""Builds the fonts in resources/fonts that differ from their upstream files.

- IosevkaFixed-Regular.ttf: Iosevka Fixed 33.2.9 Regular, subset to the characters of the bundled
  Iosevka Aile plus the punctuation, Greek mu and accidentals the app draws. The PSG hover needs a
  monospaced font; Iosevka Aile is quasi-proportional.
- Inter-Regular.ttf, Inter-Bold.ttf: Inter 4.001 lacks the flat, natural and sharp signs, which the
  tonic and scale names use; they are copied from Iosevka Fixed Regular and Bold, scaled to Inter's em.

Iosevka Fixed comes from the unhinted TTF package of its release:
    gh release download v33.2.9 -R be5invis/Iosevka -p 'PkgTTF-Unhinted-IosevkaFixed-33.2.9.zip'

Usage: python3 -B scripts/build_fonts.py IosevkaFixed-Regular.ttf IosevkaFixed-Bold.ttf

Both fonts are under the SIL Open Font License 1.1 with no reserved font name; the licence stays in
each file's name table. Running it again leaves the Inter files as they are.
"""

import sys
from pathlib import Path

from fontTools import subset
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

FONTS = Path(__file__).resolve().parent.parent / "resources" / "fonts"
ACCIDENTALS = "♭♮♯"  # flat, natural, sharp
EXTRA = ACCIDENTALS + "μ" + "".join(chr(c) for c in range(0x2010, 0x2028))  # mu, dashes, quotes, ellipsis


def build_fixed(source: Path) -> None:
    keep = set(TTFont(FONTS / "IosevkaAile-Regular.ttf").getBestCmap()) | {ord(c) for c in EXTRA}
    font = TTFont(source)
    options = subset.Options()
    options.name_IDs = ["*"]  # keep the copyright and licence records
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=keep)
    subsetter.subset(font)
    font.save(FONTS / "IosevkaFixed-Regular.ttf")


def add_glyphs(target: Path, source: Path, chars: str) -> None:
    font, donor = TTFont(target), TTFont(source)
    have = font.getBestCmap()
    missing = [c for c in chars if ord(c) not in have]
    if not missing:
        return
    scale = font["head"].unitsPerEm / donor["head"].unitsPerEm
    donor_cmap, donor_glyphs = donor.getBestCmap(), donor.getGlyphSet()
    order = font.getGlyphOrder()
    for c in missing:
        src = donor_cmap[ord(c)]
        name = f"uni{ord(c):04X}"
        pen = TTGlyphPen(None)
        donor_glyphs[src].draw(TransformPen(pen, (scale, 0, 0, scale, 0, 0)))
        glyph = pen.glyph()
        order.append(name)
        font.setGlyphOrder(order)
        font["glyf"][name] = glyph
        glyph.recalcBounds(font["glyf"])
        font["hmtx"][name] = (round(donor["hmtx"][src][0] * scale), glyph.xMin)
        for table in font["cmap"].tables:
            if table.isUnicode():
                table.cmap[ord(c)] = name
    font.save(target)


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    regular, bold = Path(sys.argv[1]), Path(sys.argv[2])
    build_fixed(regular)
    add_glyphs(FONTS / "Inter-Regular.ttf", regular, ACCIDENTALS)
    add_glyphs(FONTS / "Inter-Bold.ttf", bold, ACCIDENTALS)


if __name__ == "__main__":
    main()
