# QuarkWave guides

QuarkWave is a four-voice digital synth with an **Uno R4** sound engine and an optional **Pico W** browser controller. These guides have two doors: one for playing and shaping sounds, and one for exploring how the instrument is built.

## If you want to play

1. [Start playing](quick-start.md) — power up, find the panel, play a note, and save a variation.
2. [Play and shape sounds](user-guide.md) — the complete musician guide to the five views, controls, effects, and patches.
3. [Sounds to start from](patch-book.md) — eight factory patches and five new recipes.
4. [Learn the sound by listening](sound-design.md) — short exercises that make each control easier to hear.
5. [Reading the Uno LED display](led-display-guide.md) — photographs and diagrams for the voice, VU, Scope, and top-row lights.

## If you want to connect more gear

- [Play from another controller](external-midi-guide.md) — DIN, direct USB MIDI, wireless MIDI, and clock.
- [Connect a computer](network-midi-setup.md) — macOS, Windows, and Logic Pro.
- [How QuarkWave is connected](connection-guide.md) — the photographed Pico–Uno breadboard and the proposed DIN, line, and headphone circuits.

## If you want to understand or extend it

- [Inside QuarkWave](technical-guide.md) — architecture, patches, input ownership, DSP path, clock, and exact MIDI/WebSocket messages.
- [Hardware test log](hardware-test-log.md) — measured results and the history of checks on the assembled boards.
- [Documentation gaps](documentation-gaps.md) — decisions and hardware details still to settle.
- [Diagram style](diagram-style.md) — conventions used by the circuit and system drawings.

The Pico's **Help** view carries the nine complete playing and technical guides above, with their photographs and diagrams. They load from the Pico as you select them, even without internet access. The Markdown files here are the editable source. PDFs are separate snapshots and are generated only when requested.

## What is on the breadboard

The owner-supplied photographs show both boards powered through their own USB cables and talking over a two-way, level-shifted MIDI UART. The Pico's browser and Uno LED display can be used now. The physical DIN jack, buffered A0 line output, and TDA1308 headphone connection in the connection guide are **proposed circuits**, not parts of that photographed wiring. The optional Uno USB audio/MIDI firmware provides a computer audio route while those circuits are being built.

## Keeping Help in step with the guides

After editing an onboard guide or its image, run `npm install` once from `QuarkWave MIDI`, then `npm run build:help`. The generator updates the navigation in `QuarkWave_UI.h` and creates compressed guide pages and images in `QuarkWave_Help.h`. Commit the Markdown, image thumbnails, and generated header together. Patch files remain in the Pico's LittleFS storage; Help lives in program flash.

## Firmware snapshot

This edition describes the MIDI sketches and browser panel as inspected on **2026-09-23**. The table identifies the source files behind the descriptions; the [test log](hardware-test-log.md) records which paths were exercised on hardware.

| Component | Source | SHA-256 |
| --- | --- | --- |
| Uno R4 sound engine | [QuarkWave_MIDI.ino](../QuarkWave_MIDI/QuarkWave_MIDI.ino) | `704520258e9bd99105b58392deb69ef9927337d3f8632a63528463bb78854361` |
| Pico W controller | [QuarkWave_UI_MIDI.ino](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino) | `14072f2cd847116681e545bd7dca2acf7f90a26460dcd6d5bc3ff0e5e0b7154e` |
| Browser panel | [QuarkWave_UI.h](../QuarkWave_UI_MIDI/QuarkWave_UI.h) | `0e4cfa3aba0f7606ec48e55e7ccff9c694e7a64b4160972e04be69b8db75a904` |

The older sketches under `../QuarkWave/` are outside this edition. Local `secrets.h` files contain Wi-Fi settings and are excluded from the repository.
