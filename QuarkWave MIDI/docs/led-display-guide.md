# Reading the Uno LED display

The Uno R4 WiFi has a **12-column × 8-row red LED matrix**. It gives a quick view of connectivity and activity while you play. The green **ON** light and the small board **TX/RX** lights in the photograph are separate board LEDs, not pixels in the red matrix.

**Evidence:** the photograph below shows the owner's powered Uno on 2026-09-23. The meanings and examples on this page are **source-reviewed** against the current Uno firmware. A still photograph cannot establish which of the momentary activity or heartbeat pixels were on at a particular instant. A targeted two-board check confirmed view selection and internal envelope/VU values for a browser note. The owner then confirmed that a held key lights a Status voice bar, flashes column 10, and produces movement in VU and Scope. Recent two-board tests also confirmed Pico-forwarded wireless notes and disconnect releases; the physical DIN input and Uno audio output remain untested. [Matrix implementation](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [hardware test log](hardware-test-log.md).

![Live Uno R4 WiFi showing several illuminated red matrix pixels along the top row, with its separate green ON light](images/uno-led-live.jpg)

*Live photograph of the earlier Uno-network build. The current gateway build leaves Uno Wi-Fi and RTP-session pixels dark; the Pico handles those connections.*

## Choose a view

In the browser panel, open **Explore** or **All controls**, find **LED matrix**, and choose **Status**, **VU meter**, or **Scope**. These change only the Uno display; they do not change the sound. Status is the default after boot or a synth reset. The selector sends Pico-to-Uno Program Changes **10**, **11**, and **12** respectively. The Uno scrolls **Viz: Status**, **Viz: VU**, or **Viz: Scope** across the matrix while note processing continues, then returns to the selected display. These display commands are accepted only on the Pico UART, not on the separate DIN or RTP-MIDI inputs. [Browser control](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [Uno display selection](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

![Illustrative 12 by 8 LED matrices for Status, VU meter, and Scope](images/uno-led-modes.svg)

*Illustrative frames for the current gateway build, not captures. Lit pixels vary with Pico traffic, notes, waveform, and the heartbeat.*

| View | What the lower rows show | Useful when |
| --- | --- | --- |
| **Status** | Up to four horizontal bars, one for each synth voice. Each bar follows that voice's envelope level. | Checking which voices are active or releasing. |
| **VU meter** | Eleven vertical bars show recent mono output levels, oldest at left and newest beside the right-edge peak dot. The levels come from the synthesized signal before the DAC. | Seeing how loudness changes across roughly the last third of a second. |
| **Scope** | A moving, automatically scaled trace with one dot per column. It samples the synthesized output before the DAC. | Seeing the shape and movement of the generated waveform. |

The VU bars are **recent levels over time**. They are not frequency bands or stereo channels; QuarkWave currently produces one mono signal. Each settled VU frame adds one level about every 33 ms. The rightmost column holds the peak dot and leaves its bottom pixel free for the timing diagnostic. **VU** and **Scope** are visual guides, not calibrated level meters or measurements at the audio jack. Because the audio output stage is not assembled yet, their real-world relationship to a mixer or headphones remains untested. [Output sampling and display calculations](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [connection guide](connection-guide.md).

## The top row stays useful in every view

Count columns **1–12 from left to right as viewed on the board**. The firmware draws these indicators on the top row in all three settled views. Boot and scrolling mode labels temporarily replace the view, including its top row. A pixel marked “brief” is expected to flash and go dark; it does not represent a persistent fault.

![Numbered Uno LED top row showing disabled Uno network pixels, Pico link and receive, external sound activity, note, and heartbeat indicators](images/uno-led-top-row.svg)

- **1–6:** `1–3` Uno Wi-Fi (off) · `4` BLE-MIDI (off) · `5` Uno RTP session (off) · `6` Pico link
- **7–12:** `7` Pico MIDI RX · `8` unused · `9` external sound activity · `10` note · `11` unused · `12` heartbeat

| Column | Meaning in this build | When it lights |
| ---: | --- | --- |
| 1–3 | Uno Wi-Fi | Off: the Uno Wi-Fi stack is disabled; the optional Pico owns Wi-Fi. |
| 4 | BLE-MIDI | Never in this build; BLE-MIDI is disabled. |
| 5 | RTP-MIDI | Off: RTP sessions now terminate on the Pico, not the Uno. |
| 6 | Pico link | While the Uno has received a Pico readiness request within roughly 3.5 seconds. This is a handshake indicator, not merely a powered UART. |
| 7 | Pico MIDI receive, brief | For about 200 ms after the Uno handles a Pico UART note, sound control, Program Change, or SysEx. Pico readiness requests can flash this pixel even when nobody is playing. Clock and transport do not light it. |
| 8 | Unused | Off. |
| 9 | External sound activity, brief | For about 300 ms after a DIN note, CC, or pitch bend, or a Pico-forwarded wireless activity marker, note, or sustain message. This shows activity, not an RTP connection. MIDI Clock and transport do not light it. A DIN sound Program Change or sound SysEx can change the sound without flashing this pixel. |
| 10 | Note flash in Status view | For about 400 ms after a synth note starts. This flash is not drawn in VU or Scope. |
| 11 | Unused | Off. |
| 12 | Heartbeat | Toggles about every 250 ms; a regular blink shows the display loop is updating. |

The **bottom-right pixel** is a timing diagnostic. It can light for a frame if more than 200 ms elapsed between LED updates, such as after a blocking operation. An occasional flash alone does not identify its cause. [Top-row and diagnostic overlays](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## What happens at startup

The matrix first plays a boot animation and scrolls **QuarkWave**. The Uno no longer starts Wi-Fi or scrolls an IP address or RTP connection label; the optional Pico owns those network functions. Changing the display view scrolls **Viz: Status**, **Viz: VU**, or **Viz: Scope**. The selected view resumes after the text finishes crossing the matrix and a brief hold. Note processing continues during the scroll. [Boot animation and view labels](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Quick checks

- **Heartbeat blinks, Pico pixel is dark:** the display loop is running, but the Uno has not received a recent Pico readiness request. Check the browser's separate Pico and Uno connection labels and the inter-board wiring. The Pico-to-Uno link can be offline while the Uno remains playable from its separate DIN input.
- **Column 9 flashes:** a DIN sound message or Pico-forwarded wireless sound message arrived. Check the Pico's RTP connection label to distinguish wireless session status; column 9 by itself cannot do that.
- **Wi-Fi group is dark:** expected in this build. Check the Pico or browser for network status; the Uno can still play through its separate DIN input, subject to its wiring.
- **Voice bars move but VU seems still:** switch to VU and play again. Status follows individual voice envelopes; VU follows the final synthesized output. A new VU view starts with an empty history and fills as frames arrive.
- **A mode label is scrolling:** wait for the text to cross the matrix and the selected view to resume. Keys remain playable during the scroll.

These are reading aids, not a substitute for checking MIDI reception and audio on the assembled instrument. [Pico–Uno connection behavior](technical-guide.md#patch-lifecycle-and-storage), [hardware test log](hardware-test-log.md).
