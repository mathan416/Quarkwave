# Inside QuarkWave

QuarkWave is one instrument built from two small computers. The **Uno R4 WiFi** is the synthesizer: it receives MIDI, keeps track of four voices, runs the sound engine, and produces mono audio. The **Pico W** is its companion: it serves the browser panel, stores patches, receives wireless MIDI, and keeps the Uno in step with the selected sound. You can leave the Pico out and play the Uno from another MIDI input; you simply lose the browser and the Pico's saved patches.

This guide follows a note from the player's hand to the output, then opens up the patch system, clock, display, and message formats. For playing instructions, see the [musician guide](user-guide.md). The [connection guide](connection-guide.md) distinguishes the wired breadboard from the audio and DIN circuits still to be built.

## System at a glance

![QuarkWave system from browser and MIDI inputs to Pico, Uno, and audio output](images/system-overview.svg)

There are three paths into the instrument:

1. The **browser keyboard and controls** reach the Pico over Wi-Fi. The Pico translates their requests into MIDI for the Uno.
2. A **network MIDI controller** joins the Pico's `QuarkWave` RTP-MIDI session. The Pico forwards its notes, controls, and clock over the same wired link.
3. A **DIN controller**, or the optional direct **USB MIDI** interface, reaches the Uno without needing the Pico.

The Uno combines those inputs in one sound engine. Its ordinary build writes samples to the 12-bit DAC on **A0**. The optional composite USB build also exposes **QuarkWave USB MIDI** and a mono **QuarkWave USB Audio** input to a computer. The physical A0 line-output stage shown in the connection guide has not been assembled on the photographed breadboard.

### Connections and build flags

| Part | Current design |
| --- | --- |
| Wired control link | Pico GPIO 0 TX → Uno RX0/D0; Uno TX1/D1 → level shifter → Pico GPIO 1 RX; common ground; MIDI UART at 31,250 baud. The owner confirmed the 5 V-to-3.3 V return path. |
| Power | Each board currently uses its own USB cable. The 5 V supply rails are separate. |
| Uno audio | A0 12-bit DAC, nominally 22,050 samples per second. The buffered line output and headphone circuits in the [connection guide](connection-guide.md) are proposed additions. |
| Standalone DIN | Uno software serial receives on D2; a compliant isolated DIN input circuit is still needed. D3 is configured as software-serial TX but no MIDI OUT jack is built. |
| Wireless MIDI | The Pico hosts one RTP-MIDI peer on UDP port 5004. The Uno's own Wi-Fi and RTP polling are disabled in this build (`USE_WIFI_STACK=0`, `USE_RTP_MIDI=0`). |
| Bluetooth MIDI | Disabled (`USE_BLE_MIDI=0`). |
| Pico storage | The deployed 2 MB Pico W layout reserves 1 MB for LittleFS patch and configuration files. |

The [current wiring drawing and photographs](connection-guide.md#the-current-setup) show what is actually on the table. [Pico setup](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno setup and build flags](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## From a key press to a voice

A browser Note On is JSON over the Pico's WebSocket. The Pico tracks which browser connection holds each pitch, turns the event into MIDI, and sends it through `Serial1`. The Uno's Pico receiver accepts MIDI on all channels and allocates one of four voices. Note Off releases that voice through its envelope. If a browser tab disconnects while holding a key, the Pico releases the notes owned by that tab.

Wireless RTP-MIDI takes a nearby path through the Pico, but the Pico tags forwarded notes and sustain so the Uno can keep them separate from browser, DIN, and direct USB notes. That ownership matters: when a wireless controller disconnects, only *its* held notes and pedal are released. A note held by another input survives. A silent network loss is recognized when the RTP session times out. DIN and direct USB feed the Uno through their own receivers. [Pico gateway](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno input ownership](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [USB receiver](../experiments/usb-audio/UsbMidiInput.h).

## Patch lifecycle and storage

The Pico owns patches; the Uno owns the sound that is currently playing. Eight factory presets are compiled into the Pico at indexes **100–107** and cannot be overwritten. Eight user slots, **0–7**, live as `/patch_0.json` through `/patch_7.json` in LittleFS. A new Pico leaves them empty until someone saves. Existing files, including older patches named `P0`–`P7`, remain occupied even when their names look like placeholders. A separate commit file holds a temporary snapshot, and `/config.json` remembers the selected patch for startup. [Patch definitions and files](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino).

On boot, the Pico restores the last loaded or saved patch, falling back to factory preset 0 if the remembered user file is missing or unreadable. It asks the Uno whether the sound engine is ready. This handshake happens without a browser connection. If the Uno has had no standalone sound activity, the Pico resets it, sends the selected patch as MIDI controls, then waits for a completion reply. The same rule applies after an Uno restart.

If another controller has already played or changed the Uno's sound, the Pico leaves it alone. MIDI Clock alone does not count as sound activity. The browser then shows **Uno: Connected to Pico · standalone sound**. **Sync Pico patch to Uno** is the deliberate takeover: it resets the sound and held notes, transfers the Pico patch, and waits for an acknowledgment. Loading a patch is also an explicit sound change. The Uno does not keep those patch files and starts with its own built-in defaults when used alone. [Handshake and transfer](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno reply](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

A patch file groups envelope, filter, tempo, mix, delay, LFO, effects, arpeggiator, and voice settings as JSON version 1. The Pico normalizes an older patch's out-of-range values in memory as it loads; the file changes only on an explicit save. User-slot writes are staged and verified before replacing the previous file. Keyboard velocity and the sustain pedal are live performance controls rather than patch fields.

## Browser ↔ Pico interface

The Pico serves the main panel at `GET /`, patch metadata at `/api/patches`, and its mDNS state at `/api/mdns`. A WebSocket server on port **8081** carries playing and editing messages. The page offers **Perform**, **Shape**, **Explore**, **All controls**, and **Help** views of one live patch. Switching views does not send MIDI or recreate the keyboard. The Pico serves the full Markdown-derived Help guides and images from compressed program-flash assets under `/help/`; PDFs remain separate files in the repository. [Browser source](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [Pico routes](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino).

| Browser → Pico | Fields | Pico action |
| --- | --- | --- |
| `getPatchList` | — | Broadcasts user and factory patch metadata. |
| `control` | `param` string, `value` number/Boolean | Converts to the effective MIDI step, updates Pico's current patch fields where applicable, then sends MIDI CC, Program Change, or SysEx. |
| `randomize` | — | Pico generates one complete live patch across the valid MIDI ranges, sends it to Uno, then returns `patchData`. Patch selection, sustain pedal, and browser keyboard velocity are excluded. |
| `noteOn` | `note` 0–127, optional `velocity` 1–127 | Tracks this browser client's ownership and sends Note On to Uno when the first client holds the note. |
| `noteOff` | `note` 0–127 | Releases ownership and sends Note Off when the last client releases it. Disconnect releases that client's notes. |
| `loadPatch` | `index` 0–7 or 100+ | Loads a user file or factory preset, broadcasts patch data, then sends its controls to Uno. |
| `savePatch` | `index` 0–7, `name`, `overwrite` Boolean | Saves the live patch to a user slot. Occupied slots require `overwrite: true`; factory indexes are rejected. Names are truncated to 16 characters. |
| `commitPatch` / `loadCommitPatch` | — | Saves or restores a separate snapshot; the save path also writes a backup. |
| `visualization` | `mode` 0–2 | Sends Uno Program Change 10–12 for Status, VU, or Scope. |
| `panic` | — | Sends CC120 All Sound Off and clears tracked browser-note ownership. |
| `syncUno` | — | Explicitly resets the Uno and sends the Pico's current patch, including when the Uno has been used standalone. The browser presents a warning first. |

| Pico → browser | Relevant fields | Purpose |
| --- | --- | --- |
| `patchList` | `userPatches`, `factoryPatches` | Populates the selector. A user slot's `exists` flag reflects physical file occupancy, including unreadable files. Factory indexes start at 100 and have `readonly: true`. |
| `patchData` | `patch` object | Repaints controls from the current Pico patch. |
| `currentPatch` | `index` | Updates the selected factory or user patch. |
| `patchSaveResult` | `success`, `index`, optional `error` | Gives the requesting browser the result of a save. Only a successful save changes the live name and selected slot. |
| `commitSaved` / `commitLoaded` | `name` | Confirms snapshot operations in the browser's patch action status. |
| `unoStatus` | `connected`, `syncing`, `syncFailed`, `mode`, `rtpConnected` | Reports the independent Uno link state and whether an RTP controller is connected to the Pico. `mode` is `offline`, `syncing`, `standalone`, `pico`, or `unsynced`. |

The `control.param` names are grouped here for controller authors. Use the units in the CC and SysEx tables, not a raw slider position, unless a field itself is defined as 0–127.

| Group | Parameter names |
| --- | --- |
| Oscillator and voice | `morph`, `detune`, `unison`, `noiseAmt`, `glide`, `velCurve`, `stealMode` |
| Envelope and filter | `attack`, `decay`, `sustain`, `release`, `filterOn`, `cutoff`, `resonance`, `velToCutoff`, `noiseToCutoff` |
| Motion | `lfoAmtHz`, `lfoRateHz`, `lfoSync`, `lfoToDetune`, `lfoToMorph`, `lfoToAmp`, `lfo2RateHz`, `lfo2AmtSemi`, `lfo2Wave` |
| Rhythm and effects | `arpMode`, `arpDiv`, `arpGate`, `delayTime`, `delayFeedback`, `delayMix`, `dlySync`, `chMix`, `chDepth`, `bitcrushMix`, `bitcrushBits`, `bitcrushRateDiv`, `tremDepth`, `tremRateHz`, `driveAmount`, `foldAmount` |
| Tempo and level | `tempoSrc`, `bpm`, `masterGain`; `sustainPedal` is live and is not part of a patch |

For example, `{"type":"noteOn","note":60,"velocity":100}` plays middle C, and `{"type":"noteOff","note":60}` releases it. A sound edit such as `{"type":"control","param":"cutoff","value":1400}` names a human-unit parameter, not a raw CC number. The Pico converts it to the effective MIDI step before storing it in the live patch. Factory edits can be saved only to a user slot. `savePatch` returns `patchSaveResult`; the panel changes its selected slot only after success.

## How the sound engine works

![Uno signal path from MIDI and oscillators through filter, effects, gain, and DAC](images/synth-signal-path.svg)

A **voice** begins with a MIDI pitch and velocity. Its oscillator advances through lookup tables using a 32-bit phase. **Morph** blends oscillator shapes; **Unison** adds up to three slightly detuned oscillators within that voice. The ADSR envelope gives each note its attack, decay, held level, and release. When all four voices are busy, the voice-steal setting decides which one is reused. Pitch bend, glide, detune, and LFO2 vibrato change pitch without adding another note voice. [Oscillator and voice code](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

The voices meet a two-pole, topology-preserving state-variable **low-pass filter**. This build uses one global filter (`PER_VOICE_FILTER=0`), rather than a separate filter for each voice. Cutoff can respond to velocity, random noise movement, and **LFO1**. LFO1 can also move morph, amplitude, and unison detune. Filter and pitch modulation are prepared on the 1 kHz control tick where possible, leaving the audio loop to do the work that must happen for every sample. The filter's effective cutoff is limited near the 22.05 kHz sample rate's Nyquist frequency. [Filter and modulation](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

After filtering, the mono signal follows this order:

1. **Chorus** mixes two moving short-delay taps with the dry sound.
2. **Drive** soft-saturates peaks; **fold** reflects peaks back into range.
3. **Bitcrush** reduces amplitude precision and holds samples for a chosen number of audio ticks.
4. **Tremolo** periodically changes level.
5. **Delay** adds feedback repeats. It uses an 8-bit ring buffer at 11,025 samples per second, half the main synthesis rate, to save memory and work.
6. **Master gain**, soft clipping, and clamping prepare a 12-bit value for the A0 DAC.

The main synthesis schedule targets **22,050 samples per second**. The delay's lower internal rate is an effect implementation detail, not the rate of the whole synth. The USB-audio build copies the generated mono signal into a separate ring buffer for the USB host while the DAC path remains active. Four voices with all effects and USB streaming can exceed the Uno's time budget in demanding cases; the measurements and remaining work live in the [hardware test log](hardware-test-log.md), rather than being a promise about every patch. [Audio path and effects](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [USB audio experiment](../experiments/usb-audio/README.md).

## External MIDI Clock and transport

The Uno keeps separate histories for **DIN**, optional direct **USB MIDI**, and **RTP-MIDI forwarded by the Pico**. MIDI Clock sends 24 pulses per quarter note. Once it has a valid recent stream between **40 and 240 BPM**, the Uno prefers DIN, then USB, then RTP. If the selected stream disappears for two seconds, it chooses another recent one; otherwise it holds the last valid tempo. Before it has measured any clock, external tempo begins at 120 BPM. Clock pulses do not change the selected tempo source. The panel or QuarkWave Program Change **100** must select external tempo; Program Change **101** selects internal tempo. [Clock selection](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

With external tempo selected, incoming pulses align the arpeggiator to the beat and the scheduler fills the spaces between them, including fractional divisions. **Start** returns the pattern to its first step, **Continue** resumes, and **Stop** releases the sounding arpeggiator note while preserving the keys you hold. If clock stops during a running pattern, it continues at the last valid BPM. The tempo-synced LFO and delay use the chosen BPM, but transport does not reset their phase. [Transport and arpeggiator](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Uno LED matrix

The 12×8 matrix is the Uno's local window into its state. **Status** draws four voice-envelope bars, **VU** draws a history of eleven recent mono output levels plus a peak dot, and **Scope** plots an automatically scaled trace. A common top row shows connection, MIDI activity, sound state, timing, and heartbeat. The values come from the generated signal before the physical output circuit; the VU view is a time history, not frequency bands or stereo channels. Changing modes scrolls a **Viz:** label before the view returns. The illustrated [LED display guide](led-display-guide.md) explains every pixel. [Matrix implementation](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## MIDI message reference

The tables below are the contract for someone writing a controller or extending the Pico. Standard MIDI notes, bend, sustain, Clock, and transport coexist with QuarkWave-specific commands. A device must deliberately send QuarkWave's command bytes; the synth does not broadcast them to other instruments.

### Panel ranges and stored patch values

The CC-backed browser sliders use seven-bit positions, **0–127**. The browser shows useful units such as hertz, seconds, and percent; the Pico converts to the Uno's effective MIDI step. A stored value may move slightly to the nearest step when loaded. The factory definitions are within these ranges, and Randomize can choose across the complete valid range. Randomize does not touch patch selection, sustain pedal, or browser keyboard velocity. [Pico conversion and factory patches](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino).

### Control Changes

| CC | Control | Uno behavior for value 0→127 |
| ---: | --- | --- |
| 1 | LFO1 cutoff amount | 0→3000 Hz depth |
| 2 | LFO1 rate | 0.1→12.1 Hz when unsynced |
| 3 | LFO1 sync | Off below 64; on at 64+ |
| 5 | Glide | 0→0.3 s target |
| 7 | Master gain | 0.2→1.0 before modulation and output limiting |
| 12 | Delay time | 0.02→0.166 s when unsynced |
| 13 | Delay feedback | 0.01→0.89 |
| 14 | Delay mix | 0→1 |
| 16 | Velocity curve | Four modes: linear, soft, hard, exponential |
| 17 | Voice steal | Quietest below 64; last at 64+ |
| 18 | Arpeggiator mode | Off, up, down, up/down, random in five bands |
| 19 | Arpeggiator division | Integer 0→7 |
| 20 | Arpeggiator gate | 5→95 percent |
| 21 | Chorus mix | 0→1 |
| 23 | Sustain level | 0.10→0.85 |
| 24 | Velocity → cutoff | 0→1 amount |
| 25 | Noise → cutoff | 0→1 amount |
| 26 | LFO1 → detune | 0→1 amount |
| 27 | LFO2 rate | 0.1→20.1 Hz |
| 28 | LFO2 vibrato amount | 0→2 semitones |
| 29 | Bitcrush mix | 0→1 |
| 30 | Bit depth | 4→16 bits |
| 31 | Bitcrush sample-hold divider | 1→16 samples |
| 64 | Sustain pedal | Off below 64; on at 64+ |
| 71 | Filter resonance | Q target 0.5→3.0 |
| 72 | Release | 0.02→1.5 s |
| 73 | Attack | 0.002→0.5 s |
| 74 | Filter cutoff | 40→10,000 Hz target; DSP clamps near Nyquist |
| 75 | Decay | 0.01→1.0 s |
| 76 | Oscillator morph | 0→1 |
| 77 | Tremolo depth | 0→1 |
| 78 | Tremolo rate | 0.1→12.1 Hz |
| 79 | Drive amount | 0→1 |
| 80 | Wavefold amount | 0→1 |
| 91 | Filter enabled | Off below 64; on at 64+ |
| 93 | Noise amount | 0→1 |
| 94 | Unison detune | 2→20 cents |
| 95 | Unison count | 1→3 oscillators per voice |
| 120 | All Sound Off | Immediately silences voices, clears sustain/delay and held-note state |
| 123 | All Notes Off | Releases voices and clears held-note state |

**Master gain** is deliberately limited to **0.2–1.0** in the Uno's CC7 mapping. The browser's 20–100% readout and the Pico conversion follow the same span. A setting may round by one MIDI step when it crosses the seven-bit link. Keyboard Velocity is a different 1–127 performance value and is not saved in the patch.

### Program Change and SysEx by input

| Program | Uno action | Inputs |
| ---: | --- | --- |
| 0 | Reset synth controls to defaults. | Pico only |
| 10, 11, 12 | LED matrix Status, VU, Scope. | Pico only |
| 100, 101 | Select external or internal tempo. | Pico, DIN, RTP, optional USB |
| 102, 103 | Turn delay sync on or off. | Pico, DIN, RTP, optional USB |
| 110, 111, 112 | LFO2 sine, triangle, square. | Pico, DIN, RTP, optional USB |

The custom SysEx envelope is `F0 7D 00 <command> ... F7`. Here `7D` is the non-commercial MIDI identifier used by the source; this is a project-local protocol. The source handles these commands:

| Command | Complete message | Meaning and inputs |
| --- | --- | --- |
| `01` | `F0 7D 00 01 <BPM low> <BPM high> F7` | Internal BPM 40–240, little endian seven-bit bytes; Pico, DIN, RTP, optional USB. |
| `02` | `F0 7D 00 02 <route> <amount> F7` | LFO1 routing depth 0–127; route 0 = morph, 1 = amplitude; Pico, DIN, RTP, optional USB. |
| `03` | `F0 7D 00 03 <depth> F7` | Chorus depth 0–20; Pico, DIN, RTP, optional USB. |
| `10` | `F0 7D 00 10 F7` | Pico readiness request only. |
| `11` | `F0 7D 00 11 <flags> F7` | Uno reply to the Pico. |
| `12` | `F0 7D 00 12 F7` | Pico patch completion marker only. |
| `13` | `F0 7D 00 13 <flags> F7` | Uno acknowledgment to the Pico. |
| `14` | `F0 7D 00 14 F7` | Pico-only marker before forwarded RTP sound activity, throttled to at most once per 250 ms, and after a clean Uno restart if wireless sound activity occurred; sets standalone activity and flashes the matrix activity pixel. Never accepted from an RTP peer. |
| `15` | `F0 7D 00 15 <action> <note> <velocity> F7` | Pico-only forwarded RTP note: action `00` = Note On, `01` = Note Off; note `0–127`, velocity `0–127`. An On with velocity `0` also releases. The Pico sends releases for held wireless notes when its RTP session disconnects. Never accepted as an external RTP sound command. |
| `16` | `F0 7D 00 16 <on> F7` | Pico-only forwarded RTP sustain: `01` = pedal down, `00` = pedal up. The Pico sends pedal up on wireless disconnect. Never accepted as an external RTP sound command. |

The Pico-only readiness request (`10`), Uno replies (`11` and `13`), and patch completion marker (`12`) make link detection and patch sync independent of a browser session. In status replies, flag bit 0 records standalone sound activity since boot or the last Pico patch; bit 1 records a completed Pico patch. A controller's electrical presence is not inferred from those flags. The Pico treats the Uno as disconnected after roughly **3.5 seconds** without a reply. [Pico handshake](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno handshake](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

DIN, RTP, and optional direct USB may use the sound-related Program Changes **100–103**, **110–112**, and SysEx commands **01–03**. Visualization, reset, source tags, and handshake messages belong to the Pico link. Pitch bend is a 14-bit MIDI value with a fixed **±2-semitone** range. [External handlers](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [Pico RTP filter](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino).

## Where to go next

Use the [connection guide](connection-guide.md) for the photographed UART wiring and proposed audio circuits, the [external MIDI guide](external-midi-guide.md) to configure another controller, and the [hardware test log](hardware-test-log.md) for measurements and acceptance results. This guide describes the source design; the test log separates what has been measured on the assembled instrument from what is still planned.
