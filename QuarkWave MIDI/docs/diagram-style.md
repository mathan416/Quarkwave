# Diagram style for QuarkWave MIDI

The [current Pico–Uno connection schematic](images/pico-uno-current-uart.svg) is the visual baseline for documentation diagrams. The SVGs are self-contained so they render in Markdown previews and exported manuals without fonts or external assets.

| Role | Color | Use |
| --- | --- | --- |
| Canvas | `#0b1728` | Rounded dark background. |
| Main card | `#142a3b`, border `#589bb5` | Boards, sources, controls, and components. |
| Processing module | `#242336`, border `#ac90c7` | Level shifter, audio buffer, effects, or amplifier. |
| Output card | `#302a25`, border `#d4a46d` | DAC, connector, or destination. |
| Forward signal | `#32bdd6` | Normal left-to-right signal flow. |
| Return signal | `#f5b85b` | Reverse MIDI or feedback path. |
| Supply / reference | `#b6a4d2` | Power paths or supply notes; dashed when an endpoint still needs verification. |
| Ground | `#7bd6a2` | Common ground or return. |
| Lit matrix pixel | `#ff6767` | A physically illuminated Uno LED in illustrative views. |
| Unlit matrix pixel | `#39485b` | An off LED in illustrative views. |

Use system fonts and a clear title, subtitle, and note area. Label every path and component directly; colors add grouping but never carry the meaning alone. Keep the main flow left to right, use short orthogonal connections, and avoid lines crossing labels or other paths. The diagram colors describe **function**, not the actual jumper colors.

Captions in the guides must say whether a drawing shows **current wiring**, a **proposed unbuilt circuit**, or an **illustrative firmware view**. Preserve component values, pad and pin names, signal direction, and grounded nodes when restyling a schematic. Each SVG should provide a `<title>` and `<desc>` for nonvisual readers and be checked at both full size and a narrower manual/page width.
