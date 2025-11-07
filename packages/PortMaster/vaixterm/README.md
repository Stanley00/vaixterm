# VaixTerm for PortMaster

## About
VaixTerm is a feature-rich terminal emulator designed specifically for handheld gaming devices, with built-in support for on-screen keyboard and gamepad controls.

## Controls

| Button | Action |
|--------|---------|
| **D-Pad** | Move cursor |
| **L1/R1** | Scroll up/down |
| **X** | Toggle on-screen keyboard |
| **Y** | Space |
| **Start** | Enter |
| **Select** | Tab |


### On-Screen Keyboard Controls

| Button | Action |
|--------|---------|
| **D-Pad** | Navigate between keys |
| **A** | Input selected character |
| **B** | Delete/Backspace |
| **X** | Switch keyboard layouts |


## Customization
Custom key mappings and themes can be modified in the `res/` directory:
- Edit `.kb` files for keyboard layouts
- Modify `.theme` files for color schemes
- Adjust `.keys` files for custom key bindings

## Building from Source
```bash
git clone https://github.com/Stanley00/vaixterm.git
cd vaixterm
make clean
make all
```