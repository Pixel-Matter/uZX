# Complete PSG visualization atlas

Current renderer; 512 flag combinations and 2,192 pitch-equivalence states.

Channel masks: 0 none, 1 A, 2 B, 3 AB, 4 C, 5 AC, 6 BC, 7 ABC.

P digits assign equal pitches to the enabled tone sources (A, B, C) followed by shared E.
For example, T7 N0 E7 P0010 means A=B=E, with C at a different pitch.

Full flags, periods, expected pitch glyphs, and page/cell locations are in [states.json](../states.json).

- [Page 1](page-01.png): T0 N0 E0 P- through T0 N7 E7 P0
- [Page 2](page-02.png): T1 N0 E0 P0 through T1 N4 E2 P00
- [Page 3](page-03.png): T1 N4 E2 P01 through T2 N0 E4 P00
- [Page 4](page-04.png): T2 N0 E4 P01 through T2 N4 E6 P00
- [Page 5](page-05.png): T2 N4 E6 P01 through T3 N0 E3 P011
- [Page 6](page-06.png): T3 N0 E3 P012 through T3 N2 E1 P011
- [Page 7](page-07.png): T3 N2 E1 P012 through T3 N3 E7 P000
- [Page 8](page-08.png): T3 N3 E7 P001 through T3 N5 E5 P000
- [Page 9](page-09.png): T3 N5 E5 P001 through T3 N7 E3 P000
- [Page 10](page-10.png): T3 N7 E3 P001 through T4 N2 E5 P00
- [Page 11](page-11.png): T4 N2 E5 P01 through T4 N6 E7 P00
- [Page 12](page-12.png): T4 N6 E7 P01 through T5 N1 E2 P011
- [Page 13](page-13.png): T5 N1 E2 P012 through T5 N3 E0 P00
- [Page 14](page-14.png): T5 N3 E0 P01 through T5 N4 E6 P000
- [Page 15](page-15.png): T5 N4 E6 P001 through T5 N6 E4 P000
- [Page 16](page-16.png): T5 N6 E4 P001 through T6 N0 E2 P000
- [Page 17](page-17.png): T6 N0 E2 P001 through T6 N1 E7 P010
- [Page 18](page-18.png): T6 N1 E7 P011 through T6 N3 E5 P010
- [Page 19](page-19.png): T6 N3 E5 P011 through T6 N5 E3 P010
- [Page 20](page-20.png): T6 N5 E3 P011 through T6 N7 E1 P010
- [Page 21](page-21.png): T6 N7 E1 P011 through T7 N0 E2 P0120
- [Page 22](page-22.png): T7 N0 E2 P0121 through T7 N0 E7 P0000
- [Page 23](page-23.png): T7 N0 E7 P0001 through T7 N1 E3 P0123
- [Page 24](page-24.png): T7 N1 E4 P0000 through T7 N2 E0 P011
- [Page 25](page-25.png): T7 N2 E0 P012 through T7 N2 E5 P0010
- [Page 26](page-26.png): T7 N2 E5 P0011 through T7 N3 E2 P0001
- [Page 27](page-27.png): T7 N3 E2 P0010 through T7 N3 E6 P0100
- [Page 28](page-28.png): T7 N3 E6 P0101 through T7 N4 E3 P0012
- [Page 29](page-29.png): T7 N4 E3 P0100 through T7 N4 E7 P0110
- [Page 30](page-30.png): T7 N4 E7 P0111 through T7 N5 E4 P0102
- [Page 31](page-31.png): T7 N5 E4 P0110 through T7 N6 E1 P0101
- [Page 32](page-32.png): T7 N6 E1 P0102 through T7 N6 E5 P0112
- [Page 33](page-33.png): T7 N6 E5 P0120 through T7 N7 E2 P0111
- [Page 34](page-34.png): T7 N7 E2 P0112 through T7 N7 E6 P0122
- [Page 35](page-35.png): T7 N7 E6 P0123 through T7 N7 E7 P0123
