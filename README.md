# ⚛🌊 Quarkwave

Quarkwave is a MIDI synthesizer built around two boards: an **Arduino Uno R4 WiFi** generates the sound, and a **Raspberry Pi Pico W** hosts the browser interface and stores patches. You can also play the Uno directly from an external MIDI controller. The MIDI version is the focus of current development.

![Quarkwave Perform panel simulation with the shared keyboard and patch controls](QuarkWave%20MIDI/docs/images/perform-simulated.png)

*The panel image is rendered from the current embedded page with Warm Pad and a simulated connected status. The [hardware test log](QuarkWave%20MIDI/docs/hardware-test-log.md) records live checks separately.*

## Current status

The browser panel, Pico–Uno MIDI handshake, and patch sync have been tested on the two-board setup. The optional USB-audio experiment worked in Logic Pro on the owner's Mac. A newer composite Uno experiment also received a USB MIDI note and returned recorded USB audio over the same cable; that combined route still needs a Logic Pro check. Its clean DAC update rate was near 22.05 kHz, while a four-voice, all-effects stress test exposed [USB audio underruns](QuarkWave%20MIDI/docs/hardware-test-log.md#usb-midi-audio-regression-suite--2026-09-23). The Uno's `A0` audio output circuit has **not** been assembled, so its physical jack output and levels remain unverified. The [connection guide](QuarkWave%20MIDI/docs/connection-guide.md) separates the current wiring from the proposed line and headphone circuits.

The Pico serves the **Perform**, **Shape**, **Explore**, and **All controls** views at `http://quarkwave.local/` when it is connected to the configured network. The Uno is the sound engine; the Pico is the main interface, patch store, and controller. The Uno accepts a separate physical MIDI input; the optional Pico receives RTP-MIDI over Wi-Fi and forwards it to the Uno. See the [external MIDI guide](QuarkWave%20MIDI/docs/external-midi-guide.md) and [Mac, Windows, and Logic setup](QuarkWave%20MIDI/docs/network-midi-setup.md). BLE-MIDI is disabled.

## Start here

| If you want to… | Read… |
| --- | --- |
| Find the controls and try a sound | [Quick start](QuarkWave%20MIDI/docs/quick-start.md) and [musician guide](QuarkWave%20MIDI/docs/user-guide.md) |
| Explore factory sounds and new recipes | [Patch book](QuarkWave%20MIDI/docs/patch-book.md) and [sound-design lessons](QuarkWave%20MIDI/docs/sound-design.md) |
| Understand the boards and MIDI messages | [Technical guide](QuarkWave%20MIDI/docs/technical-guide.md) |
| Inspect the wiring and audio plans | [Connection guide](QuarkWave%20MIDI/docs/connection-guide.md) |
| See test results and open questions | [Hardware test log](QuarkWave%20MIDI/docs/hardware-test-log.md) and [documentation gaps](QuarkWave%20MIDI/docs/documentation-gaps.md) |

The Pico panel includes a **Help** view with ten topic pages for playing, sound design, patches, MIDI, troubleshooting, connections, messages, and the sound engine. The [documentation index](QuarkWave%20MIDI/docs/README.md) lists the fuller guides and their firmware snapshot. PDF editions are in [`QuarkWave MIDI/output/pdf/`](QuarkWave%20MIDI/output/pdf/); they are regenerated on request, so Markdown is the latest working copy.

## All controls in one view

The **All controls** tab puts the sound engine, modulation, effects, and Uno LED controls on one scrollable page.

![Full-page All controls view, from the patch strip and keyboard through sound shaping, effects, and LED matrix controls](QuarkWave%20MIDI/docs/images/all-controls-simulated.png)

*Full-page source render with a simulated connected status. It shows the controls and labels, not a fresh hardware measurement.*

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

Both sketches include a local `secrets.h`; only the Pico uses Wi-Fi in the current build. Git excludes these files. For each sketch you intend to build, copy its `secrets.example.h` to `secrets.h` in the same folder. Set the Pico's network name and password, and do not commit either filled-in file.

The current development hardware uses separate USB power for the Uno and Pico, a shared ground, and level-shifted MIDI UART connections. The `A0` audio circuit, DIN input hardware, and headphone amplifier are not yet a completed build. Follow the [connection guide](QuarkWave%20MIDI/docs/connection-guide.md) and its verification notes before attaching audio equipment.

## Branches

- **`dev`** — ongoing development and documentation check-ins.
- **`main`** — retained baseline for changes promoted from `dev`.

The project is still defining its contribution process and license; neither is published yet.
