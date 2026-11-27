# NexPlay

A native multisystem emulator for Aurora OS on Nintendo 3DS.

## Current target

- **Frontend:** native Aurora OS C SDK application.
- **First emulation core:** Nintendo Game Boy (DMG), powered by Peanut-GB.
- **Application format:** Aurora C app container (`AURC`, `.bin`).
- **Interface language:** English.

## Features in this build

- Dual-screen library interface.
- Game Boy ROM browser at `/roms/gb`.
- Game Boy DMG core integration.
- D-pad, A/B, Start and Select input mapping.
- Cartridge RAM allocation and save-file writing.
- Native Aurora drawing and input APIs.

## Build requirements

- Aurora OS SDK source tree.
- Python 3.8+.
- devkitARM (`arm-none-eabi-gcc`) on PATH.

Build from the Aurora OS repository root:

```sh
python3 sdk/aurcc.py build /path/to/NexPlay -o NexPlay.bin
```

Copy `NexPlay.bin` to:

```
SD:/Aurora/Apps/C/NexPlay.bin
```

Create `SD:/roms/gb/` and place legally obtained `.gb` ROM files there.

## Controls

| Nintendo 3DS | Game Boy |
|---|---|
| D-Pad | D-Pad |
| A | A |
| B | B |
| X | Start |
| Y | Select |
| B while playing | Return to library |

## Core attribution

The Game Boy DMG core is [Peanut-GB](https://github.com/deltabeard/Peanut-GB), Copyright (c) 2018-2023 Mahyar Koshkouei, distributed under the MIT License. Its original license notice is retained in `core/gb/peanut_gb.h`.

## Current limitations

- Game Boy Color mode is not implemented; this first core targets original DMG games.
- Audio output is not integrated yet.
- The current build writes cartridge RAM when leaving a game; loading existing save files and full battery-backed save compatibility remain follow-up work.
- Game compatibility depends on Peanut-GB's documented hardware coverage and accuracy.
