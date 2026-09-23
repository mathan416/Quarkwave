# Play the Uno from an external MIDI controller

**For:** musicians and testers using QuarkWave's Uno R4 sound module without the Pico, or alongside it. **Status:** source-reviewed, hardware untested (firmware snapshot: 2026-09-22). The physical DIN connector and wiring are not yet documented; use only the tested connection provided with your build.

## Choose an input

| Input | What this build supports | What to check on the assembled instrument |
| --- | --- | --- |
| Separate DIN MIDI | The Uno listens for MIDI through a software serial receiver on pin 2. It accepts all MIDI channels. | Confirm the installed DIN connector and interface circuit before connecting a controller. Source pin assignments alone are not a wiring guide. |
| RTP-MIDI | Enabled. The Uno starts a network MIDI session named `QuarkWave` on port `5004` after joining its configured Wi-Fi network; it listens on all MIDI channels. | Confirm the Uno's network address and session availability in your RTP-MIDI software. |
| BLE-MIDI | Disabled in this build. | Do not expect a Bluetooth MIDI device to discover QuarkWave. |

[Uno MIDI instances and build flags](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [network setup](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [physical connection notes](connection-guide.md).

## Play without the Pico

1. Power the assembled Uno and connect its tested audio output to your listening setup at a low level. Connect your controller through the build's verified DIN input, or establish an RTP-MIDI session with `QuarkWave` on the same reachable network.
2. Send Note On and Note Off messages. **Expected from source:** the Uno plays its built-in startup sound and remains playable without a Pico or browser. This startup sound is the Uno's firmware default, **not** the Pico's Warm Pad factory preset. [Uno defaults](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [external input handlers](../QuarkWave_MIDI/QuarkWave_MIDI.ino).
3. Try pitch bend (fixed ±2 semitones), sustain pedal CC64, or a sound CC from the [technical CC table](technical-guide.md#control-changes). These are live changes in the Uno; the Uno does not save patch files. [Uno bend and CC handlers](../QuarkWave_MIDI/QuarkWave_MIDI.ino).
4. Release notes when finished. If a note remains on, send All Notes Off CC123 or All Sound Off CC120 from a controller that supports it, then stop and inspect the MIDI stream. These messages are described by the Uno's CC handler; their behavior needs hardware confirmation. [CC map](technical-guide.md#control-changes).

## Use an external controller while the Pico is present

An external note, CC, bend, or accepted QuarkWave sound command marks standalone activity. If that happens **before** the Pico's boot patch is applied, the Pico keeps the Uno's current sound and the browser shows **Uno: Connected · standalone sound**. The browser's **Sync Pico patch to Uno** explicitly resets and replaces that sound and held notes. Once a Pico patch has been applied, external MIDI can still play and adjust the Uno, but those changes may not appear in the Pico's patch controls or saved file. [Sync rule](technical-guide.md#patch-lifecycle-and-storage), [musician takeover steps](user-guide.md#take-over-a-standalone-sound).

## Follow MIDI Clock

1. In the Pico's Explore view, enable **Use external MIDI clock**; or send QuarkWave Program Change `100` to the Uno. Clock pulses alone do not select external tempo. Program Change `101` selects internal tempo. Some controller menus display program numbers one higher than the MIDI data byte; verify the transmitted byte with a MIDI monitor. [Tempo commands](technical-guide.md#program-change-and-sysex-by-input).
2. Set your controller or sequencer to send MIDI Clock and, for transport-controlled arpeggiation, Start, Continue, and Stop. Hold notes with the arpeggiator enabled. **Expected from source:** Start begins at the first step, Continue resumes, and Stop releases the current arp note while retaining held keys. Direct notes remain playable when the arpeggiator is off. [Clock behavior](technical-guide.md#external-midi-clock-and-transport).
3. Test at 40–240 BPM. When both DIN and RTP send valid recent clock, DIN has priority. After two seconds without pulses from the selected source, the Uno switches to the other recent source or holds the last valid tempo. It starts at 120 BPM before receiving a valid clock. These timing outcomes are hardware untested. [Clock implementation](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## QuarkWave-specific sound commands

External DIN and RTP devices may send the documented sound Program Changes `100–103`, `110–112`, and SysEx commands `01–03`; see the [exact byte table](technical-guide.md#program-change-and-sysex-by-input). Configure a controller to transmit those bytes if you need tempo source, delay sync, vibrato waveform, internal BPM, LFO routing, or chorus depth. Factory-preset selection is a Pico patch operation; generic Program Change numbers do not select the Pico's eight factory presets. Visualization, reset, and handshake commands are restricted to the Pico wire. QuarkWave does not forward its custom messages to other synths. [External handlers](../QuarkWave_MIDI/QuarkWave_MIDI.ino).
