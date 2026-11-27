# NexPlay

A native multisystem emulator for Aurora OS on Nintendo 3DS.

## Current target

- **Frontend:** native Aurora OS C SDK application.
- **First playable emulation core:** Nintendo Game Boy (DMG), powered by Peanut-GB.
- **Multi-system target:** easy-to-medium systems are grouped in this single application.
- **Application format:** Aurora C app container (`AURC`, `.bin`).
- **Interface language:** English.

## Features in this build

- Futuristic graphite, violet and cyan dual-screen interface.
- Console selector: Game Boy, Game Boy Color, Game Boy Advance, NES, SNES, Mega Drive, Master System and PlayStation 1.
- Circle Pad and D-pad navigation.
- Only Game Boy DMG is playable in this build; all other listed platforms are roadmap entries marked Coming Soon.
- Game Boy ROM browser at `/NexPlay/roms/gb`.
- Game Boy DMG core integration.
- D-pad, A/B, X for Start and the physical SELECT key for Select.
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

Create `SD:/NexPlay/roms/gb/` and place legally obtained `.gb` ROM files there.

## Controls

| Nintendo 3DS | Game Boy |
|---|---|
| D-Pad | D-Pad |
| A | A |
| B | B |
| X | Start |
| SELECT | Select |
| X + Y while playing | Return to Game Boy library |
| B in library | Return to console menu |

## Core attribution

The Game Boy DMG core is [Peanut-GB](https://github.com/deltabeard/Peanut-GB), Copyright (c) 2018-2023 Mahyar Koshkouei, distributed under the MIT License. Its original license notice is retained in `core/gb/peanut_gb.h`.

## Current limitations

- Game Boy Color mode is not implemented; this first core targets original DMG games.
- Audio output is not integrated yet.
- The current build writes cartridge RAM when leaving a game; loading existing save files and full battery-backed save compatibility remain follow-up work.
- Game compatibility depends on Peanut-GB's documented hardware coverage and accuracy.


## Multi-system roadmap

NexPlay keeps every supported system in one application and one repository. The initial target list is:

| System | Short name | Status |
|---|---|---|
| Game Boy | GB | Playable: DMG core |
| Game Boy Color | GBC | Planned |
| Game Boy Advance | GBA | Planned |
| Nintendo Entertainment System | NES | Planned |
| Super Nintendo | SNES | Planned |
| Sega Mega Drive / Genesis | MD | Planned |
| Sega Master System | SMS | Planned |
| PlayStation 1 | PS1 | Planned |

Only original Game Boy DMG emulation is playable in the current build. The other entries are roadmap targets, not working emulators yet.

## ROM folder layout

Place legally obtained game files in the folder for their system:

- `/NexPlay/roms/gb/`
- `/NexPlay/roms/gbc/`
- `/NexPlay/roms/gba/`
- `/NexPlay/roms/nes/`
- `/NexPlay/roms/snes/`
- `/NexPlay/roms/megadrive/`
- `/NexPlay/roms/mastersystem/`
- `/NexPlay/roms/ps1/`

The current Game Boy browser reads only `/NexPlay/roms/gb/`. The other folders are reserved for their future cores.
