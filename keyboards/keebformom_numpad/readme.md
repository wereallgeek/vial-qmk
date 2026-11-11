# USB numeric pad for the geeks

A 20-key USB numpad built on a single Keeb For Mom (KFM) 4x5 PCB, mounted rotated 90 degrees counter-clockwise (-90 deg). The old spacebar row ends up on the right side, which gives room for a 2u "+" and a 2u Enter. The firmware is Vial, so every key and layer can be remapped live from the Vial app.

* Uses PCB from: [Keeb For Mom](https://github.com/nendezkombet/Keeb-For-Mom/tree/main/Raw%20firmware/keyboards/keebformom) by sandipratama (nendezkombet)
* Hardware Supported: one KFM PCB (4x5), Arduino Pro Micro (atmega32u4)
* Hardware Availability: https://github.com/nendezkombet/Keeb-For-Mom

## Thanks

Many thanks to [sandipratama / nendezkombet](https://github.com/nendezkombet) for designing the KFM PCB and for sharing the PCB files and the original firmware as open source. This keyboard is only a rotated layout on top of their work, and it keeps the original GPL license.

## Layout

The PCB is rotated -90 deg, so the USB side of the board is at the left. The right column uses the 2u footprints that sit in the bottom row of the unrotated PCB.

```
,---------------------------.
| NUM  |  /   |  *   |  +   |
|------+------+------+  2u  |
|  7   |  8   |  9   |      |
|------+------+------+------|
|  4   |  5   |  6   |  -   |
|------+------+------+------|
|  1   |  2   |  3   |Enter |
|------+------+------+  2u  |
|  0   |  .   |BSPC/ |      |
|      |      | FN   |      |
`---------------------------'
```

* The key at the bottom of the third column is Backspace on tap, FN layer on hold (RGB and media keys). This can be remapped in vial.
* A 2u key only has one switch. Both PCB footprints of a 2u key have the same keycode, so it works whichever footprint the switch uses. In Vial, set both footprints to the same key.
* Instead of 2u Enter and/or + keys, two keys can be mounted. If so, they can be remapped separately in vial.

## Build

Make in your QMK build environment:

    make keebformom_numpad:vial

Flashing can be done from the QMK environment as well:

    make keebformom_numpad:vial:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Physical reset**: Press the reset button on the PCB if you soldered one, or briefly short the RST and GND pins of the Pro Micro

