# ⚛🌊 Quarkwave

Quarkwave is a MIDI synthesizer built around two boards: an **Arduino Uno R4 WiFi** generates the sound, and a **Raspberry Pi Pico W** hosts the browser interface and stores patches. You can also play the Uno directly from an external MIDI controller. The MIDI version is the focus of current development.

![Simulated Quarkwave Perform panel with the shared keyboard and patch controls](QuarkWave%20MIDI/docs/images/perform-simulated.png)

*The panel image is a simulated connected view. The [hardware test log](QuarkWave%20MIDI/docs/hardware-test-log.md) records what has been verified on the assembled boards.*

## Current status

The browser panel, Pico–Uno MIDI handshake, and patch sync have been tested on the two-board setup. The Uno's `A0` audio output circuit has **not** been assembled, so audible sound and output levels remain unverified. The [connection guide](QuarkWave%20MIDI/docs/connection-guide.md) separates the current wiring from the proposed line and headphone circuits.

The Pico serves the **Perform**, **Shape**, **Explore**, and **All controls** views at `http://quarkwave.local/` when it is connected to the configured network. The Uno is the sound engine; the Pico is the main interface, patch store, and controller. External DIN and RTP-MIDI support is described in the [external MIDI guide](QuarkWave%20MIDI/docs/external-midi-guide.md). RTP-MIDI is enabled in this build; BLE-MIDI is disabled.

## Start here

| If you want to… | Read… |
| --- | --- |
| Find the controls and try a sound | [Quick start](QuarkWave%20MIDI/docs/quick-start.md) and [musician guide](QuarkWave%20MIDI/docs/user-guide.md) |
| Explore factory sounds and new recipes | [Patch book](QuarkWave%20MIDI/docs/patch-book.md) and [sound-design lessons](QuarkWave%20MIDI/docs/sound-design.md) |
| Understand the boards and MIDI messages | [Technical guide](QuarkWave%20MIDI/docs/technical-guide.md) |
| Inspect the wiring and audio plans | [Connection guide](QuarkWave%20MIDI/docs/connection-guide.md) |
| See test results and open questions | [Hardware test log](QuarkWave%20MIDI/docs/hardware-test-log.md) and [documentation gaps](QuarkWave%20MIDI/docs/documentation-gaps.md) |

The [documentation index](QuarkWave%20MIDI/docs/README.md) lists every guide and its firmware snapshot. PDF editions are in [`QuarkWave MIDI/output/pdf/`](QuarkWave%20MIDI/output/pdf/); they are regenerated on request, so Markdown is the latest working copy.

## Project layout

```text
QuarkWave MIDI/
  QuarkWave_MIDI/       Uno R4 WiFi sound engine
  QuarkWave_UI_MIDI/    Pico W browser panel and patch manager
  docs/                 Guides, diagrams, and test records
  output/pdf/           Manually generated PDF editions
QuarkWave/              Earlier non-MIDI sketches, kept for reference
```

## Local setup

Both sketches use a local `secrets.h` for Wi-Fi settings. Git excludes these files. For each sketch you intend to build, copy its `secrets.example.h` to `secrets.h` in the same folder and replace the placeholder network name and password. Do not commit your filled-in `secrets.h`.

The current development hardware uses separate USB power for the Uno and Pico, a shared ground, and level-shifted MIDI UART connections. The `A0` audio circuit, DIN input hardware, and headphone amplifier are not yet a completed build. Follow the [connection guide](QuarkWave%20MIDI/docs/connection-guide.md) and its verification notes before attaching audio equipment.

## Branches

- **`dev`** — ongoing development and documentation check-ins.
- **`main`** — retained baseline for changes promoted from `dev`.

The project is still defining its contribution process and license; neither is published yet.
