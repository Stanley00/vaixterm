# VaixTerm

## Notes
A robust SDL2 terminal emulator for handheld and embedded systems. Features on-screen keyboard, gamepad support, 256-color palette, and configurable themes.

## Controls

### General
| Button | Action |
|--------|--------|
| D-Pad | Arrows / Navigate OSK |
| A | Select (OSK) |
| B | Backspace |
| X | Toggle OSK |
| Y | Space |
| Start | Enter |
| Select | Tab |
| Select+Start | Exit |

### OSK Off (Terminal)
| Button | Action |
|--------|--------|
| L1 | Scroll Up |
| R1 | Scroll Down |

### OSK On (Modifiers)
| Button | Action |
|--------|--------|
| L1 | Shift |
| R1 | Ctrl |
| L2 | Alt |
| R2 | GUI |

## Thanks
Thanks to the VaixTerm developer for creating this open source terminal emulator. Also thanks to Stanley00 for the packaging for PortMaster.

## Compile
```shell
git clone https://github.com/Stanley00/vaixterm.git
cd vaixterm
make clean
make all
```
