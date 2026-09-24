# Play and shape sounds

QuarkWave is a four-voice synth built around an Arduino Uno R4. Its Pico W companion gives you a keyboard, sound controls, patches, and Help in any browser on the same network. You can also play the Uno from an external MIDI controller. This guide stays close to the things you do while making music; the [technical guide](technical-guide.md) explains what happens inside.

If this is your first session, take the five-minute [quick start](quick-start.md) first.

## Get connected and play a note

1. Power both boards. Open `http://quarkwave.local/` on a phone or computer connected to the Pico's network. If the name does not work, use the Pico's IP address.
2. Look for **Pico: Connected** and **Uno: Connected to Pico**. The first confirms the browser can reach the Pico; the second confirms the Pico and Uno are exchanging MIDI messages. The separate RTP-MIDI label tells you whether a wireless MIDI controller has joined.
3. Play the on-screen keyboard, or use **A W S E D F T G Y H U J** on a computer keyboard. Use **−** and **+** to move by octaves.
4. Set **Velocity** beside the keyboard. A low number plays a gentler new note; 127 is the highest MIDI velocity. The setting starts at 100 when the page opens. Changing it while a note is held affects the *next* note, not the one already playing.

The Uno's LED matrix offers a quick visual check when a note arrives. To hear the current breadboard build, use the Uno's **QuarkWave USB Audio** input with a music app such as Logic Pro; the [connection guide](connection-guide.md) explains why the proposed physical A0 line jack is not yet an option.

If the Uno says **Connected to Pico · standalone sound**, it has already been played or controlled through another MIDI input. QuarkWave leaves that sound in place. You can play it as it is, load a different patch, or deliberately [sync the Pico sound](#take-over-a-standalone-sound).

## Find your way around the panel

The patch strip and keyboard stay in place as you change views. Switching views does not interrupt a held key or reset a control.

| View | Reach for it when you want to… |
| --- | --- |
| **Perform** | Play with volume, cutoff, morph, sustain, and the arpeggiator within easy reach. |
| **Shape** | Work from oscillator and noise through envelope, filter, glide, and voice response. |
| **Explore** | Add LFO motion, vibrato, rhythm, effects, and choose an Uno LED view. |
| **All controls** | See the full synth panel together while programming a sound. |
| **Help** | Read the complete guides and illustrations without leaving the instrument. |

![Perform view with patch strip, keyboard, and quick controls](images/perform-simulated.png)

*Perform keeps the controls you are most likely to touch during a song beside the keyboard.*

![All controls view with sound, modulation, and effects in one place](images/all-controls-simulated.png)

*All controls is the wide workbench. The same controls return to Shape and Explore when you change views.*

The pictures of the browser panel are illustrations made from the UI; the connection labels and patch names in them are examples.

![Help view with the Uno LED guide open below the shared keyboard](images/help-simulated.png)

*The Help view loads the full guide and its photographs and diagrams from the Pico.*

## Build a sound

A good way to learn QuarkWave is to load a factory patch and change one thing at a time. **Pluck** makes envelope changes easy to hear; **Warm Pad** is useful for slow filter and modulation changes. The [patch book](patch-book.md#factory-presets) introduces all eight starting sounds.

![Shape view with oscillator, envelope, filter, and voice controls](images/shape-simulated.png)

1. Load **Pluck** and play the same note several times. In **Shape**, move **Morph**. It blends the oscillator's shapes. **Unison** layers up to three oscillators for each played note; **Detune** spreads those layers in pitch.
2. Turn **Filter on**, then lower **Cutoff** to take away high frequencies. Raise **Resonance** to emphasize the area near the cutoff. Compare a held note before and after each change.
3. Lengthen **Attack** for a slower beginning. Lengthen **Release** to let the note fade after you let go. **Decay** controls the move from the initial level toward **Sustain**, the level held while the key remains down.
4. Add **Noise** for a rougher texture or **Glide** for pitch travel between notes. Try short notes, long notes, and a chord before deciding what to keep.

QuarkWave can sound four notes at once. **Unison** changes the number of oscillators within a voice, not the number of playable notes. When you ask for a fifth overlapping note, **Voice steal** chooses which voice is reused. **Velocity curve** changes how strongly playing velocity affects level.

## Add movement and rhythm

![Explore view with modulation, arpeggiator, effects, and LED controls](images/explore-simulated.png)

**LFO 01** is your main source of repeating motion. Raise **Cutoff amount** and set a slow **Rate** to hear the filter move. Then return that amount to zero and try **To morph** or **To amplitude** to hear the difference between tone movement and volume movement. **To detune** moves the unison spread. **Velocity to cutoff** connects harder playing to a brighter filter; **Noise to cutoff** adds less regular movement.

**Vibrato** uses a second LFO. Start with a small **Vibrato amount**, then adjust its rate and choose a sine, triangle, or square wave. The two LFOs have separate jobs, so you can make a slowly changing filter and a quicker pitch wobble at the same time.

For repeating notes, choose an **Arpeggiator** mode and hold a chord. **Arp division** sets the step spacing; **Arp gate** sets how long each step lasts. Start with **Internal BPM**. If you want a sequencer to set the beat, enable **Use external MIDI clock** and send clock from DIN, direct USB MIDI, or a controller connected to the Pico over RTP-MIDI. Clock messages alone do not switch QuarkWave to external tempo. The [external MIDI guide](external-midi-guide.md#follow-midi-clock) explains Start, Continue, Stop, and source priority.

**Sustain** holds released notes until you turn it off. An external pedal can also send MIDI CC64.

## Use effects

The effects are easiest to learn one at a time. Start with a modest listening level, because combining them can change the output level.

| Effect | First experiment | What you are hearing |
| --- | --- | --- |
| **Chorus** | Raise **Mix**, then move **Depth**. | Two short, moving delayed copies widen and animate the sound. |
| **Drive / Fold** | Raise either amount gradually. | Drive rounds and saturates peaks; fold turns peaks back into the waveform. |
| **Bitcrush** | Raise **Mix**, lower **Bits**, then change **Rate**. | Fewer amplitude steps and held samples make a rougher texture. |
| **Tremolo** | Raise **Depth**, then set **Rate**. | Repeating changes in loudness. |
| **Delay** | Raise **Mix**, set **Time** and **FB**. | Echoes; **Sync (1/16)** ties repeat time to the chosen tempo. |

The sound passes through chorus, drive/fold, bitcrush, tremolo, then delay. The [sound-design lessons](sound-design.md#5-follow-the-effects-chain) offer short comparisons if you want to hear each stage clearly.

## Explore a random sound

Select **Randomize** beside the patch buttons, then play a *new* note. QuarkWave picks a complete set of valid sound-control values and updates the panel. It can choose strong effect and volume settings, so turn down your listening level first. Adjust the promising parts and save the result if you want to keep it. Randomize leaves your chosen patch slot, sustain pedal, and browser keyboard Velocity alone.

## Save, load, and recover patches

The eight factory sounds are permanent starting points. Eight separate **user slots**, numbered 0–7, hold the sounds you save. A new Pico starts with empty user slots; an existing instrument may already have sounds in them.

- **Load:** Choose a factory or user sound and select **Load patch**. This replaces unsaved edits and sends the selected settings to the Uno.
- **Save a user sound:** With a user slot selected, name your sound and select **Save patch**. This updates that slot.
- **Save a factory edit or make a copy:** Name your sound, select **Save As**, choose a user slot, and select **Save to slot**. Confirm if the destination already contains a patch. A factory preset cannot be overwritten.
- **Keep a temporary checkpoint:** **More actions → Commit snapshot** records a separate copy. **Load committed sound** brings it back. A commit is useful while experimenting, but give a finished sound a user slot as well.

![Save As dialog with empty and occupied user slots](images/save-as-simulated.png)

*Save As makes the destination explicit. A slot with an existing file counts as occupied even if its name looks like an unused placeholder.*

The Pico remembers the last patch you loaded or saved for startup. Browser keyboard Velocity and the live sustain pedal are performance settings, so they are not stored in the patch. Changes made directly on the Uno from another MIDI controller may not appear in the Pico's controls or saved file.

## Take over a standalone sound

If another controller reaches the Uno before the Pico applies its patch, QuarkWave preserves that sound. The browser shows **Uno: Connected to Pico · standalone sound**.

1. Release notes on the other controller.
2. Open **More actions → Sync Pico patch to Uno**.
3. Read the warning and confirm. Sync resets the Uno's current sound and held notes, then sends the Pico's selected patch.
4. Wait for **Uno: Connected to Pico**. If sync fails, check the Uno connection and try again.

Loading a patch is another deliberate way to replace the current sound. MIDI clock by itself never triggers a sound takeover.

## When something goes wrong

| What you see or hear | Try this |
| --- | --- |
| The page does not open | Check Pico power and Wi-Fi, then try its IP address instead of `quarkwave.local`. |
| **Pico: Not connected** | Reload the page and check the Pico's network connection. |
| **Uno: Not connected to Pico** | Check Uno power. On the breadboard, check both UART paths, the shared ground, and the level shifter. |
| Both boards show connected but you hear nothing | Check the audio input, monitoring, speaker level, and **Volume**. A MIDI connection does not by itself provide an audio connection. |
| **Standalone sound** appears | Keep playing that sound, load another patch, or use explicit **Sync Pico patch to Uno**. |
| **Sound sync failed** appears | Check the Uno return wire and try Sync again. |
| A note hangs | Select **Panic** in the top strip. |
| Save As fails | Check the selected user slot and the message in the patch strip. An unreadable existing file is treated as occupied, not empty. |

For Mac, Windows, and Logic connections, go to [Network MIDI setup](network-midi-setup.md). For the matrix lights, see [Reading the Uno LED display](led-display-guide.md).
