# Reading the Uno LED display

The Uno R4 WiFi has a **12-column × 8-row red LED matrix**. It is both a tiny performance display and a set of status lights: three views show voices, recent output level, or the generated waveform, while the top row keeps the same twelve indicators. The green **ON** lamp and small board **TX/RX** lights are separate hardware LEDs, not matrix pixels.

![Live Uno R4 WiFi showing several illuminated red matrix pixels along the top row, with its separate green ON light](images/uno-led-live.jpg)

*The owner’s Uno in the development setup. The picture captures one instant; activity and heartbeat pixels normally blink.*

## Choose a view

In the browser panel, open **Explore** or **All controls**, find **LED matrix**, and choose **Status**, **VU meter**, or **Scope**. These change only the Uno display; they do not change the sound. Status is the default after boot or a synth reset. The Uno scrolls **Viz: Status**, **Viz: VU**, or **Viz: Scope** across the matrix while note processing continues, then returns to the selected display. The Pico sends Uno Program Changes 10, 11, and 12 for these views. A separate MIDI controller does not select the display.

![Illustrative 12 by 8 LED matrices for Status, VU meter, and Scope](images/uno-led-modes.svg)

*Example frames, drawn to show the three layouts. The real pixels move with notes and sound.*

| View | What the lower rows show | Useful when |
| --- | --- | --- |
| **Status** | Up to four horizontal bars, one for each synth voice. Each bar follows that voice's envelope level. | Checking which voices are active or releasing. |
| **VU meter** | Eleven vertical bars show recent mono output levels, oldest at left and newest beside the right-edge peak dot. The levels come from the synthesized signal before the DAC. | Seeing how loudness changes across roughly the last third of a second. |
| **Scope** | A moving, automatically scaled trace with one dot per column. It samples the synthesized output before the DAC. | Seeing the shape and movement of the generated waveform. |

The VU bars are **recent levels over time**. They are not frequency bands or stereo channels; QuarkWave currently produces one mono signal. Each settled VU frame adds one level about every 33 ms. The rightmost column holds the peak dot and leaves its bottom pixel free for the timing diagnostic. **VU** and **Scope** are visual guides, not calibrated level meters or measurements at the audio jack. They describe the signal inside the Uno, before a line jack or headphones. The photographed build has no physical line-output stage yet.

## The top row stays useful in every view

Count columns **1–12 from left to right as you face the board**. These indicators return in all three views after boot text or a scrolling **Viz:** label finishes. A “brief” light is meant to flash and go dark.

![Numbered Uno LED top row showing sound source, clock, sustain, arpeggiator, Pico link, MIDI activity, voice steal, audio timing, and heartbeat indicators](images/uno-led-top-row.svg)

- **1–6:** `1` Pico sound loaded · `2` external MIDI used · `3` external clock active · `4` sustain · `5` arpeggiator enabled · `6` Pico link
- **7–12:** `7` Pico MIDI RX · `8` voice stolen · `9` external sound activity · `10` note · `11` audio timing slip · `12` heartbeat

| Column | Meaning in this build | When it lights |
| ---: | --- | --- |
| 1 | Pico sound loaded | The Uno acknowledged applying a Pico patch. This does not prove the sound is unchanged afterward. |
| 2 | External MIDI used | A DIN, optional direct USB, or Pico-forwarded wireless note or sound control has been accepted since boot or the last Pico patch sync. Columns 1 and 2 can both be on. Clock alone does not light it. |
| 3 | External clock active | External tempo is selected and a valid, recent DIN, optional direct USB, or Pico-forwarded RTP clock stream is the selected source. It goes dark when pulses stop for about two seconds; the last tempo is retained. Transport Start alone is not a valid clock stream. |
| 4 | Sustain held | The Uno's sustain state is on. |
| 5 | Arpeggiator enabled | An arpeggiator mode other than Off is selected. This does not mean it is currently stepping: transport may be stopped or no keys may be held. |
| 6 | Pico link | While the Uno has received a Pico readiness request within roughly 3.5 seconds. This is a handshake indicator, not merely a powered UART. |
| 7 | Pico MIDI receive, brief | For about 200 ms after the Uno handles a Pico UART note, sound control, Program Change, or SysEx. Pico readiness requests can flash this pixel even when nobody is playing. Clock and transport do not light it. |
| 8 | Voice stolen, brief | For about 400 ms when a new note needs a voice and all four are occupied. The Uno reuses one voice according to its selected voice-steal rule. |
| 9 | External sound activity, brief | For about 300 ms after a DIN or optional USB note, CC, or pitch bend, or a Pico-forwarded wireless activity marker, note, or sustain message. This shows activity, not an RTP connection. MIDI Clock and transport do not light it. An external sound Program Change or sound SysEx can change the sound without flashing this pixel. |
| 10 | Note started, brief | For about 400 ms after a synth note starts, in all three settled views. |
| 11 | Audio timing slip, brief | For about 700 ms when the audio scheduler finds itself more than 2 ms behind and skips stale scheduled samples. This reports a scheduling event, not a measured DAC underrun count or an audible glitch. |
| 12 | Heartbeat | Toggles about every 250 ms; a regular blink shows the display loop is updating. |

The **bottom-right pixel** is a separate display timing diagnostic. It can light for a frame if more than 200 ms elapsed between LED updates, such as after a blocking operation. It does not measure the audio scheduler; column 11 does that. An occasional flash alone does not tell you what caused the delay.

## What happens at startup

The matrix first plays a boot animation and scrolls **QuarkWave**. The Uno no longer starts Wi-Fi or scrolls an IP address or RTP connection label; the optional Pico owns those network functions. Changing the display view scrolls **Viz: Status**, **Viz: VU**, or **Viz: Scope**. The selected view resumes after the text finishes crossing the matrix and a brief hold. You can keep playing while the label scrolls.

## Quick checks

- **Heartbeat blinks, Pico pixel is dark:** the display loop is running, but the Uno has not received a recent Pico readiness request. Check the browser's separate Pico and Uno connection labels and the inter-board wiring. The Pico-to-Uno link can be offline while the Uno remains playable from its separate DIN input.
- **Column 9 flashes:** a DIN, optional USB, or Pico-forwarded wireless sound message arrived. Check the Pico's RTP connection label to distinguish wireless session status; column 9 by itself cannot do that.
- **Column 8 flashes:** a fifth overlapping note needed one of the four voices; shorten releases or play fewer overlapping notes if this is unwanted.
- **Column 11 flashes repeatedly:** the audio loop is falling behind its schedule. Try fewer voices or lighter effects and record the patch and playing conditions for a performance test. An isolated flash does not prove an audible glitch.
- **Columns 1 and 2 are both lit:** the Pico loaded a sound, then external MIDI was used. Column 2 does not distinguish playing notes from changing sound controls.
- **Voice bars move but VU seems still:** switch to VU and play again. Status follows individual voice envelopes; VU follows the final synthesized output. A new VU view starts with an empty history and fills as frames arrive.
- **A mode label is scrolling:** wait for the text to cross the matrix and the selected view to resume. Keys remain playable during the scroll.

For how the Pico and Uno decide when a sound is loaded, see [Patch lifecycle and storage](technical-guide.md#patch-lifecycle-and-storage). The display code lives in the [Uno sketch](../QuarkWave_MIDI/QuarkWave_MIDI.ino).
