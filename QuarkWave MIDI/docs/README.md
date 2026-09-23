# QuarkWave MIDI documentation

**Audience:** development testers using an assembled instrument, and developers extending it.  
**Status:** source-reviewed, partial hardware check. The Pico RTP gateway has passed a live invitation, note, disconnect-release, standalone-state, and explicit-sync smoke check; clock timing and audio remain untested. These pages describe firmware behavior inspected on **2026-09-23**. Browser, USB, targeted Uno timing, and owner-observed LED matrix checks have run on the two-board build. The audio output circuit and audible results remain untested.

## Choose a guide

- [Quick start](quick-start.md) — first note, first factory sound, and first saved variation on an assembled instrument.
- [Musician guide](user-guide.md) — connect, play, shape sounds, use effects, and manage patches. Start here if you know synth controls but do not need firmware internals.
- [Patch book](patch-book.md) — eight built-in sounds, what shapes each one, and five new patch recipes to try.
- [Sound-design lessons](sound-design.md) — short listening exercises for oscillator, envelope, filter, movement, and effects.
- [Uno LED display](led-display-guide.md) — read the top-row connection lights, voice bars, output meter, and waveform view.
- [External MIDI controllers](external-midi-guide.md) — play the Uno alone, send clock, and use its supported sound commands.
- [Technical guide](technical-guide.md) — board roles, signal flow, WebSocket and MIDI messages, patch files, and firmware behavior.
- [Diagram style](diagram-style.md) — visual conventions for maintaining the guide figures.
- [Connection guide](connection-guide.md) — photographs of the current USB-powered build, known board wiring, and proposed isolated DIN, line, and TDA1308 headphone connections.
- [Hardware test log](hardware-test-log.md) — repeatable checks and space for measured results.
- [Documentation gaps](documentation-gaps.md) — source discrepancies and tasks that need a product decision or hardware check before the guides can make stronger promises.

The **panel** images are captures of the connected Pico page after the gateway upload; the Save As dialog was staged without writing a patch. They document layout and browser status, not audible output. The [connection guide](connection-guide.md) contains owner-supplied photographs of the actual build; its line-output diagram is a proposed circuit, not an as-built photo.

## Firmware snapshot

The two sketches and embedded browser page are the source of truth for these pages:

| Component | Source | SHA-256 |
| --- | --- | --- |
| Uno R4 sound engine | [QuarkWave_MIDI.ino](../QuarkWave_MIDI/QuarkWave_MIDI.ino) | `a6a5ab38a567c402e3a47a8135cef0d116a61ac4dae93a639321e20928d2ee7e` |
| Pico W web controller | [QuarkWave_UI_MIDI.ino](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino) | `d417836d7293f2a92310ca3f9d98572b8f6d8407242c1f2be2a5398b21a05646` |
| Embedded browser panel | [QuarkWave_UI.h](../QuarkWave_UI_MIDI/QuarkWave_UI.h) | `2e23d94cb0c828ba0870a6710a09e8f74fd561bf7a0a2439617bf6a43b0425e9` |

The connected controller identified over USB on 2026-09-23 is a **Pico W (RP2040, 2 MB flash)**. Its deployed build uses a 1 MB LittleFS region to match the existing patch storage. The Uno identified as **Arduino UNO R4 WiFi**. Both uploads verified; the current live panel shows **Pico: Connected** and **Uno: Connected to Pico · sound loaded** with the existing “Test Pluck” user patch. An automated browser pass exercised 50 control paths and 41 pointer keys, plus all 12 computer keys at three velocities. The Pico reported changes and the Uno handshake was initially connected, but the Uno does not acknowledge each sound command. A Pico return-message parser fix resolved the reproduced explicit-sync failure in repeated two-board tests. A temporary Uno diagnostic confirmed browser Note On, envelope movement, and a finite pre-DAC signal on one patch; audible output and per-control sound response remain unverified. See the [hardware test log](hardware-test-log.md).

The older sketches under `../QuarkWave/` are outside this documentation baseline. The `secrets.h` files contain local Wi-Fi settings and are deliberately excluded.

## Reading the status labels

**Source-reviewed** means the described path exists in this snapshot. **Hardware untested** on a specific procedure means that procedure still needs a two-board test. The handshake and browser paths with recorded results are identified in the [hardware test log](hardware-test-log.md); electrical levels and audio remain unmeasured. A procedure that cannot be established confidently from source is listed in [Documentation gaps](documentation-gaps.md) instead of being presented as a working feature.

## When firmware changes

Recheck the three files above, update the protocol and control tables, walk through the musician tasks on the assembled unit, then change the snapshot date and hashes. Record unresolved changes in the gaps page.
