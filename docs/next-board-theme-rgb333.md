# Next board theme colours (RGB333)

Conversion to the nearest RGB333 level:

```text
round(channel * 7 / 255)
```

| Theme | Colour | Source RGB | RGB333 | Bits `RRR GGG BBB` | 9-bit value | Next `0x44` bytes | Representable RGB |
|---:|---|---|---|---|---:|---|---|
| Glass Ice | COLOR1 | `#6DFFFF` | `(3,7,7)` | `011 111 111` | `0x0FF` | `0x7F, 0x01` | `#6DFFFF` |
| Glass Ice | COLOR2 | `#496D92` | `(2,3,4)` | `010 011 100` | `0x09C` | `0x4E, 0x00` | `#496D92` |
| Glass Aurora | COLOR1 | `#49FFDB` | `(2,7,6)` | `010 111 110` | `0x0BE` | `0x5F, 0x00` | `#49FFDB` |
| Glass Aurora | COLOR2 | `#244949` | `(1,2,2)` | `001 010 010` | `0x052` | `0x29, 0x00` | `#244949` |
| Glass Violet | COLOR1 | `#DBB6FF` | `(6,5,7)` | `110 101 111` | `0x1AF` | `0xD7, 0x01` | `#DBB6FF` |
| Glass Violet | COLOR2 | `#49246D` | `(2,1,3)` | `010 001 011` | `0x08B` | `0x45, 0x01` | `#49246D` |
| Glass Ember | COLOR1 | `#FFB692` | `(7,5,4)` | `111 101 100` | `0x1EC` | `0xF6, 0x00` | `#FFB692` |
| Glass Ember | COLOR2 | `#492424` | `(2,1,1)` | `010 001 001` | `0x089` | `0x44, 0x01` | `#492424` |

The representable RGB column is authoritative for hardware. For themes 2-5,
COLOR1 is the unselected coordinate ink, selected coordinate paper and 1 px
board frame; COLOR2 is the selected coordinate ink. ULA+ stays enabled for the
whole Next application: theme 1 keeps the classic appearance through the
standard colours mirrored in palette groups 0/1, while themes 2-5 use the
private group-2 entries above.

The setup selector renders each theme as a full 8x8 diagonal swatch: the lit
upper-left half uses the dominant colour of the editable light tile and the
lower-right half uses the dominant colour of its dark tile. Normal swatches
occupy ULA+ group 3. Focus preserves both colours and moves to one-step brighter
copies in group-2 indices 3-7; coordinate colours retain indices 0-2.

## ZEsarUX 13.0

Stock ZEsarUX 13.0 can render the valid private attributes `0x81` and `0x8A`
as blue with FLASH because its TBBlue path does not distinguish ULA+ enable
(`NextReg 0x68` bit 3) from ULANext enable (`NextReg 0x43` bit 0).

The prepared sibling `../ZXESPEmu` applies
`patches/zesarux-13-tbblue-ulaplus.patch`, which selects the private ULA+
palette group from attribute bits 6-7. The patch is reported upstream at
<https://github.com/chernandezba/zesarux/pull/12>. Emulator output remains
supporting evidence; real Next hardware is authoritative.
