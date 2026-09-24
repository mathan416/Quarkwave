# Sounds to start from

QuarkWave comes with **eight factory sounds** on the Pico. They are read-only, so you can explore freely: loading a factory sound again restores its starting settings. Eight separate **user slots** hold the variations you decide to keep.

Choose a sound in the patch strip and select **Load patch**. Set a comfortable **Volume** before playing. The factory sounds are at indexes 100–107 inside the firmware; the user slots are 0–7. A new Pico starts with Warm Pad selected and empty user slots.

## Factory presets

Each sound has a different job. The values below are starting settings from the preset definitions; the panel may show a nearby value after the seven-bit MIDI conversion. All eight use internal tempo at 120 BPM, start with the arpeggiator off, and leave bitcrush, tremolo, drive, and fold off.

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

For a quick comparison, play the same chord with **Warm Pad**, **Sweep Pad**, and **Chrs Strngs**. All three use three oscillators, but their envelope, filter motion, and chorus settings make them behave differently. For short notes, compare **Pluck**, **EP Keys**, and **Noise Perc**.

## New sounds to try

These five recipes are **ideas for user patches**, not extra factory presets. Load the named starting sound, change only the listed controls, then play and adjust by ear. The sliders land on MIDI steps, so a displayed value may be close to the suggested number rather than identical. Save As only when you like the result.

### Glass Bells — from Pluck

A bright pluck with a longer tail:

1. Load **Pluck**. In Shape, set **Decay** near `0.40 s`, **Sustain** near `10%`, **Release** near `0.70 s`, and **Cutoff** near `6000 Hz`.
2. In Explore, set **Chorus Mix** near `10%` and **Delay Mix** near `25%`. Leave the preset's **Sync to 1/16 note** on.
3. Play single notes at several Velocity settings. Save As **Glass Bells** if the balance works.

### Sub Current — from Solid Bass

A restrained bass with a little more pitch movement:

1. Load **Solid Bass**. In Shape, set **Cutoff** near `550 Hz`, **Glide** near `0.08 s`, and keep **Unison** at **1 oscillator**.
2. In Explore, add **Drive** near `15%`. Leave **Delay Mix** at zero.
3. Play a slow one-note line, then try short overlaps between notes. Save As **Sub Current** if useful.

### Orbit Arp — from PWM Lead

A repeating line that follows QuarkWave's internal tempo:

1. Load **PWM Lead**. In Explore, leave **Use external MIDI clock** off and set **Internal BPM** to `120`.
2. Set **Arpeggiator** to **Up**, **Arp division** to `2`, and **Arp gate** near `55`.
3. Turn **Sync to 1/16 note** on in Delay and set **Delay Mix** near `25%`.
4. Hold a three-note chord. Save As **Orbit Arp** if you want this rhythm available later.

### Tape Haze — from EP Keys

Soft keys with a worn edge:

1. Load **EP Keys**. In Shape, set **Cutoff** near `1700 Hz`.
2. In Explore, set **Chorus Mix** near `35%`, **Bitcrush Mix** near `15%`, **Bits** near `8`, and **Tremolo Depth** near `10%` with **Rate** near `2 Hz`.
3. Play a short progression; reduce Bitcrush Mix if the texture obscures the notes. Save As **Tape Haze** if you like it.

### Storm Drums — from Noise Perc

A noisy, struck texture:

1. Load **Noise Perc**. In Shape, set **Noise** near `70%`, **Cutoff** near `2500 Hz`, **Decay** near `0.12 s`, and **Release** near `0.08 s`.
2. In Explore, set **Bitcrush Mix** near `25%`, **Bits** near `6`, and **Drive** near `18%`.
3. Play short notes at different pitches and velocities. Save As **Storm Drums** if it earns a slot.
## Make your own variation

1. Start from the factory sound closest to what you want. Change one section at a time: **Shape** for tone and note shape, **Explore** for movement and effects.
2. Play the sound in a musical phrase, not only as a single held key. Try a chord, a soft note, and a strong note.
3. Give the variation a descriptive name and use **Save As** to place it in a user slot. The panel confirms before replacing an occupied slot.

Reloading the factory sound discards unsaved changes. The Pico saves the controls it knows about; changes sent straight to the Uno by another MIDI device may not be reflected in that file. For the exact preset values and MIDI ranges, see the [technical guide](technical-guide.md#panel-ranges-and-stored-patch-values).
