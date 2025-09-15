# VaixTerm Packages

This directory contains package configurations for different platforms and distributions.

## MUOS Package

Directory: `MUOS/`

A package for MUOS (MUX OS) that includes:
- Terminal executable
- Resource files (keyboard layouts, themes, etc.)
- Launch script

### Installation
1. Package the contents of `MUOS/Terminal` into a `.muxapp` zip file
2. Install on your MUOS device

## PortMaster Package

Directory: `PortMaster/`

A package for PortMaster that includes:
- Terminal executable
- Resource files (keyboard layouts, themes, etc.)
- Launch script

### Installation
1. Package the contents of `PortMaster/Terminal` according to PortMaster requirements
2. Install on your device via PortMaster

## Building Packages

### MUOS Package
```bash
cd packages/MUOS
zip -r VaixTerm.muxapp Terminal/
```

### PortMaster Package
```bash
cd packages/PortMaster
# Add PortMaster specific packaging commands here
```
