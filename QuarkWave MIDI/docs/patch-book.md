# QuarkWave patch book

**Status:** source-reviewed, hardware untested (firmware snapshot: 2026-09-23). The factory settings below come from the [Pico preset definitions](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino#L345). Descriptions of how they might sound, and the new recipes, are suggestions to audition on an assembled instrument.

## Before you start

The Pico contains eight read-only factory presets, numbered `100–107` in its patch list. A new Pico selects **Warm Pad**. The eight user slots, `0–7`, are separate; on a new Pico they are empty until you save. Factory edits live in the current sound until you load another patch or save them to a user slot. [Preset definitions](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino#L345), [patch storage](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [save procedure](user-guide.md#save-load-and-recover-patches).

To try a sound, choose it in the patch strip and select **Load patch**. Set a comfortable **Master volume** before playing. To keep an edit, enter a name, select **Save As**, choose one of the eight user slots, and select **Save to slot**. The panel asks before replacing an occupied slot. Saving never changes the factory preset. The keyboard's **Velocity** setting changes new browser notes but is not part of any patch. These operating steps are **source-reviewed, hardware untested**. [Browser patch actions](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [Pico save handling](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino).

## Factory presets

The descriptions follow the initialized settings; they are starting points for listening, not claims from an audio test. Values shown here are rounded source settings. The Pico converts them to effective MIDI steps when loading, so a panel readout may differ slightly. All eight select internal tempo at 120 BPM, leave the arpeggiator off, and start with bitcrush, tremolo, drive, and fold mixes or amounts at zero. [Factory table](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino#L345), [MIDI value conversion](technical-guide.md#panel-ranges-and-stored-patch-values).

| Factory sound | Source settings that shape it | First thing to try |
| --- | --- | --- |
| **Warm Pad** `100` | Three oscillators, 12-cent detune, 0.08 s attack, 0.8 s release, 1.4 kHz cutoff, gentle LFO and delay. | Hold a chord, then raise **Cutoff** in Perform. |
| **Pluck** `101` | One oscillator, 0.002 s attack, 0.12 s decay, low sustain, 3.8 kHz cutoff, delay and velocity-to-cutoff response. | Play short notes at different keyboard **Velocity** values. |
| **Solid Bass** `102` | One oscillator, morph 0.85, 900 Hz cutoff, 0.03 s glide, and no delay mix. | Play single notes, then increase **Glide** in Shape. |
| **PWM Lead** `103` | Two oscillators, 10-cent detune, morph 0.75, LFO routing to morph and detune, and a small delay mix. | Hold a note and change **Rate** under LFO 01 in Explore. |
| **EP Keys** `104` | One oscillator, 0.35 s decay and release, velocity-to-cutoff 0.6, light chorus and delay. | Try soft and hard notes, then raise **Chorus Mix**. |
| **Sweep Pad** `105` | Three oscillators, 0.15 s attack, 1.2 s release, 1.1 kHz cutoff, slow deep filter LFO, chorus and delay. | Hold a chord and change LFO 01 **Cutoff amount**. |
| **Noise Perc** `106` | One oscillator, 0.5 noise amount, very short envelope, 3.2 kHz cutoff, strong velocity-to-cutoff response. | Play short notes across the keyboard, then adjust **Noise**. |
| **Chrs Strngs** `107` | Three oscillators, 16-cent detune, 0.9 s release, 1.5 kHz cutoff, chorus mix 0.35 and light delay. | Hold a chord and compare **Chorus Mix** at its preset value and zero. |

**A useful comparison:** Warm Pad, Sweep Pad, and Chrs Strngs all use three oscillators, but differ in envelope timing, filter movement, and chorus. Load each in turn and hold the same chord. Pluck, EP Keys, and Noise Perc show how envelope and velocity response change short notes. [Factory table](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino#L345).

## New sounds to try

These are **suggested user patches**, not built-in presets. Start each recipe by loading its named factory preset; only change the listed controls. The values are within the Uno's documented MIDI ranges, but the exact panel value may land on the nearest MIDI step. Play and adjust by ear, then use **Save As** if you like the result. Sound descriptions are ideas for testing and remain **hardware untested**. [Panel controls](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [Uno control ranges](technical-guide.md#control-changes).

### Glass Bells — from Pluck

For bright, short notes with a longer tail:

1. Load **Pluck**. In Shape, set **Decay** near `0.40 s`, **Sustain** near `10%`, **Release** near `0.70 s`, and **Cutoff** near `6000 Hz`.
2. In Explore, set **Chorus Mix** near `10%` and **Delay Mix** near `25%`. Leave the preset's **Sync to 1/16 note** on.
3. Play single notes at several Velocity settings. Save As **Glass Bells** if the balance works.

### Sub Current — from Solid Bass

For a restrained bass with more pitch glide:

1. Load **Solid Bass**. In Shape, set **Cutoff** near `550 Hz`, **Glide** near `0.08 s`, and keep **Unison** at **1 oscillator**.
2. In Explore, add **Drive** near `15%`. Leave **Delay Mix** at zero.
3. Play a slow one-note line, then try short overlaps between notes. Save As **Sub Current** if useful.

### Orbit Arp — from PWM Lead

For a repeating sequence that follows QuarkWave's internal tempo:

1. Load **PWM Lead**. In Explore, leave **Use external MIDI clock** off and set **Internal BPM** to `120`.
2. Set **Arpeggiator** to **Up**, **Arp division** to `2`, and **Arp gate** near `55`.
3. Turn **Sync to 1/16 note** on in Delay and set **Delay Mix** near `25%`.
4. Hold a three-note chord. Save As **Orbit Arp** if you want this rhythm available later.

### Tape Haze — from EP Keys

For softer keys with a worn texture:

1. Load **EP Keys**. In Shape, set **Cutoff** near `1700 Hz`.
2. In Explore, set **Chorus Mix** near `35%`, **Bitcrush Mix** near `15%`, **Bits** near `8`, and **Tremolo Depth** near `10%` with **Rate** near `2 Hz`.
3. Play a short progression; reduce Bitcrush Mix if the texture obscures the notes. Save As **Tape Haze** if you like it.

### Storm Drums — from Noise Perc

For a noisy, struck texture:

1. Load **Noise Perc**. In Shape, set **Noise** near `70%`, **Cutoff** near `2500 Hz`, **Decay** near `0.12 s`, and **Release** near `0.08 s`.
2. In Explore, set **Bitcrush Mix** near `25%`, **Bits** near `6`, and **Drive** near `18%`.
3. Play short notes at different pitches and velocities. Save As **Storm Drums** if it earns a slot.

## Make your own variation

1. Load the closest factory preset, then change one section at a time: Shape for tone and envelope, Explore for movement and effects.
2. Compare the edited sound with the factory starting point by loading the factory preset again. Reloading discards unsaved edits, so use **Save As** first if you want to keep them.
3. Give the user patch a name that hints at its use. Each user slot holds one patch; confirm before overwriting one you want to keep.

The Pico stores browser-edited patch settings. Changes sent directly to the Uno from a separate MIDI controller may not be reflected in the Pico's saved patch data. [Patch behavior](user-guide.md#save-load-and-recover-patches), [Pico patch storage](technical-guide.md#patch-lifecycle-and-storage).
