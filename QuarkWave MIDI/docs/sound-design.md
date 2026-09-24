# Learn QuarkWave's sound controls

**For:** musicians who know a little synth programming. **Status:** source-reviewed, hardware untested (firmware snapshot: 2026-09-23). These are listening exercises, not verified descriptions of the finished audio. Use the [optional USB-audio route](network-midi-setup.md#record-the-sound-while-playing-from-the-pico-page) for the current development build; the physical A0 line output is not yet built. Begin at a comfortable level. Reloading a factory patch discards unsaved changes; use [Save As](user-guide.md#save-load-and-recover-patches) first if you want to keep a result.

Each exercise starts from a factory patch, changes one group of controls, and tells you what to compare. Load the named sound from the patch strip before starting. The [patch book](patch-book.md#factory-presets) describes the original settings.

## 1. Hear the oscillator morph

1. Load **Solid Bass** and hold one middle-range note. In Shape, slowly move **Morph** from one end to the other.
2. Return to the original patch by selecting **Load patch** again. In Shape, change **Unison** from **1 oscillator** to **3 oscillators**, then increase **Detune** gradually.
3. Compare single notes and chords. The source blends oscillator shapes for Morph and layers oscillators for Unison; this exercise helps you find useful tone and width settings. Three oscillators per note still use the same four note voices. [Oscillator and voice path](technical-guide.md#how-the-sound-engine-works).

## 2. Shape a note's beginning and end

1. Load **Pluck** and play short notes. In Shape, raise **Attack** toward `200 ms`; play again and compare how quickly the note begins.
2. Lower Attack again. Raise **Release** toward `800 ms`; play a note, then lift the key and compare its tail.
3. Raise **Sustain** and hold a note. Compare the held level with the original Pluck. Attack, Decay, Sustain, and Release are the Uno's amplitude envelope; the browser's envelope drawing previews the settings, not measured output. [Envelope control map](technical-guide.md#control-changes), [Uno voice envelope](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## 3. Connect filter tone with playing strength

1. Load **EP Keys**. In Shape, lower **Cutoff** to about `1 kHz`, then return it near `2.2 kHz`. Hold the same note during each change.
2. In Explore, change **Velocity to cutoff** from its preset setting to zero. Set the keyboard **Velocity** to `40` and then `120`, playing a new note at each setting. Repeat with Velocity to cutoff raised.
3. In Shape, increase **Resonance** gradually while moving Cutoff. The source implements a low-pass filter with velocity modulation; note that the keyboard's Velocity affects only new notes and is not saved in a patch. [Filter and velocity controls](technical-guide.md#control-changes), [keyboard behavior](user-guide.md#get-connected-and-play-a-note).

## 4. Add slow movement

1. Load **Warm Pad** and hold a chord. In Explore's **LFO 01**, set **Cutoff amount** to zero, then raise it gradually while the chord continues.
2. Change **Rate** from slow to faster. Try **To morph** and **To amplitude** separately, returning each to zero before trying the next. This separates filter, wave-shape, and level movement.
3. Turn **Sync LFO to tempo** on and change **Internal BPM** with external clock off. The source derives LFO timing from the selected tempo when sync is enabled. [LFO routing and tempo](technical-guide.md#how-the-sound-engine-works).

## 5. Follow the effects chain

1. Load **Noise Perc**. In Explore, raise **Bitcrush Mix** while repeating short notes; change **Bits** and **Rate divider**, then return the mix to zero.
2. Load **EP Keys**. Compare **Chorus Mix** at zero and near `35%`, then add a little **Tremolo Depth** and adjust its **Rate**.
3. Turn on Delay **Sync to 1/16 note** and raise **Delay Mix** gradually. Change **Internal BPM** to compare the repeat spacing. The code processes chorus, drive/fold, bitcrush, tremolo, then delay. Combined effects can change output level, so adjust listening level as needed. [Effect order](technical-guide.md#how-the-sound-engine-works).

## 6. Build a playable variation

1. Pick the closest factory sound and decide what to change: a faster start, darker tone, more motion, or an effect. Change one section at a time.
2. Play short notes, long notes, and a chord. QuarkWave has four note voices; try **Voice steal** in Shape if your part uses more notes than that. Compare low and high keyboard Velocity settings; use **Velocity curve** to change the level response.
3. Name the variation and use **Save As** to store it in a user slot. Then reload that user slot and compare it with the factory source. [Patch procedure](user-guide.md#save-load-and-recover-patches).

For more prescriptive starting points, try the five [suggested patches](patch-book.md#new-sounds-to-try). Record what you actually hear in the [hardware test log](hardware-test-log.md) before describing any exercise as tested behavior.
