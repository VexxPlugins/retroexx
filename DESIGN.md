# DESIGN.md: identity contract for Retroexx (Vexx Audio)

Design contract written before any interface code. Source: VEXX identity kit (v3.0, 2026-08-17).

> **v2 (2026-10-22): the plugin GUI moves to a hardware aesthetic.** The plugin interface leaves
> the light scheme of §1/§5 and adopts the language of a rack unit: almost black blue-tinted
> metal, a recessed screen, a cream band with the large knobs and ice-blue LEDs. The **documents**
> (README, site, slides) stay on the light scheme: white was the kit's rule for paper and web, not
> for an audio plugin.
>
> What does **not** change in the identity: the §3 logomark, the RETROEXX wordmark in Orbitron, gold
> `#FFC94A` as the only brand accent, Rajdhani for the technical voice, and zero third-party graphic
> assets.
>
> **v6 (2026-10-22): VEXX DASH-90 rules the visuals.** The GUI aesthetic is set by the DASH-90
> addendum (a 1990s car dashboard: blue-green VFD, red phosphor, italic seven-segment digits) and it
> wins over this document wherever they clash. That addendum is part of the private design kit and
> is not in this repository; its values are implemented in `src/plugin/ui/Theme.h`. What stays here
> is the brand identity and the structure (§5).
>
> **v7 (2026-10-22): DASH-90 skin applied (phase 2).** §3 tokens in `Theme.h` (old names remain as
> aliases), seven-segment digits with ghost segments, halo and slant, segmented bars (FLUX, DECAY,
> dual-wedge crossfades, MAGNITUDE wedge, FOCUS column, CUT range), knobs with a tick ring and
> knurled body, keys with a small window and square LED (no more ON/OFF), VFD windows with scanlines
> and reflection, hatching that fades when a module is switched off, FLUX pulse and power-on sweep.
> Module colours come from DASH-90 §3.4.
>
> **v8 (2026-10-22): parameter visualisers (phase 3).** WOBBLE: two LFOs (coarse WOW, fine FLUTTER)
> with depth = √Amount × balance × Mix, and cycles and speed monotonic with each RATE (the law in Hz
> is not fixed). DIGITAL: a two-cycle sine held (RATE) and quantised (BITS) by √Amount and balance,
> SMOOTH towards the line between samples, MIX against the ghost sine. DIST: a pictogram per type
> (tubes, transformer, transistor, clipping, bolt, gauge) whose filament brightens with the Amount
> and flickers with the module's real level. NOISE, SPACE and MAGNETIC keep the VU meter until phase
> 4 (they need an audio FIFO).
>
> **v9 (2026-10-22): audio visualisers (phase 4).** `VizTap` (a preallocated SPSC queue) on the
> `NoiseFX`, `Space` and `Magnetic` nodes; the column VU meter is gone. NOISE: min/max oscilloscope
> per quantum on a dB scale (floor at −60 dBFS). SPACE: two 12-dot rings with the L/R energy of the
> node's **output** (not of the internal feedback lines, which have no tap). MAGNETIC: L
> output/input gain (top, `traceWarm`) and R (bottom), from 0 to −24 dB, with a gap on dropouts and
> silence; it does not separate wear from flutter (that would need a tap inside the block).
>
> **v10 (2026-10-23): performance and states (phase 5).** Editor timer at 60 Hz; each zone repaints
> only when what it draws changes (FLUX halo by 8-bit alpha, SPACE in groups of frames, DIGITAL only
> when a parameter moves). Each column's screen is an opaque child (`ModulePanel::Screen`) with the
> LCD background cached; the glass is drawn directly (a cached ARGB layer was more expensive).
> `JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1` in plugin and demo.
> **Measured, target not reached:** offscreen bench (CoreGraphics) 2.7 % of one core at 1x and
> 5.4 % at 2x; real window (`RetroexxDemo --no-audio`, ps %cpu, 8 s average): 24 % with the timer at
> 60 Hz, 17 % at 30 Hz and 12 % with no editor timer (the baseline is the demo itself and
> compositing). §6 states: LED off at 15 %, double click back to the default value also in
> FOCUS/CUT and in the selector, and the selector fixed, which would not go past type 1 with arrows
> or keyboard (it used the normalised value as the index).
> **Owner decision (2026-10-23): visualisers and FLUX halo at 15 Hz** (the timer stays at 60 Hz for
> the hatching fade and the sweep). Real window: 14.4 % for the demo against 12 % with no editor
> timer, that is about 2 points of the editor; offscreen bench 1.2 % at 1x and 2.4 % at 2x. Figures
> from a single measurement series, not an average of several.
>
> The §1-bis values **are not invented**: they fix the tone, the screen contrast and the temperature
> of the cream band. Textures and dials are **procedural** (`src/plugin/ui/Skin.cpp`,
> `ParamKnob.cpp`): no third-party PNG enters the build.

## 1. Palette (semantic tokens; do NOT invent other colours)

| Token | Hex | Role |
|---|---|---|
| `--bg` | `#FFFFFF` | page background. Light is the default |
| `--surface` | `#F3F4F6` | panels, callouts, code blocks |
| `--ink` | `#2E3338` | body text and headings |
| `--ink-muted` | `#6B7280` | metadata, units, footer |
| `--accent` | `#7B1E3A` | burgundy red: header rule, h2 border, highlighted values, logo nodes |
| `--detail` | `#FFC94A` | gold: subheadings, logo core, fine detail |
| `--border` | `#E5E7EB` | tables and separators |
| `--row-alt` | `#F9FAFB` | alternate table rows |

Hard rules: red and gold are **never** page backgrounds; no dark or "navy" backgrounds in
documents, except for the site dark theme in §1-quater, which swaps these values instead of
inventing new ones; gold is not used for body text (1.6:1 contrast on white).

## 1-bis. Plugin GUI palette (v2)

> **Superseded in v7** by DASH-90 §3 (see the v7 note above).

| Token | Hex | Role | Origin |
|---|---|---|---|
| `window` | `#0B0C16` | window background | mean of `Back_Main_2x.png` (±4 noise, R<G<B) |
| `windowDeep` | `#05060C` | vignette, recessed housings | p5 of the same background |
| `screen` | `#0A0B15` | module screens | mean of `Back Main Display_2x.png` |
| `hairline` | `#1C1E2B` | 1 px separators | plate edge |
| `cream` | `#D9D3C1` | band behind the large knobs | p95 of the cream band (224,217,199) |
| `creamDeep` | `#B8B2A0` | band shadow | p5 of the same band |
| `creamInk` | `#211E17` | engraving on cream | dominant of the `Big_Knob_Only_Knob` engraving |
| `ink` | `#E7E3D8` | main text on dark | label ink |
| `inkMuted` | `#8A8A93` | knob legends | secondary labels |
| `led` | `#D9E7FF` | dial value arc | LED measured in `Big_Knob_Only_Led` (ice blue) |
| `accent` | `#FFC94A` | brand gold: module LED, values, header rule | VEXX kit (unchanged) |
| `amber` | `#F8AB54` | VU meter | visualiser colour (`0xF8AB54`) |
| `hot` | `#FF6345` | VU meter hot zone and errors | `0xFF6345` |
| `bodyDark` | `#313131` | small knob body | dominant of `DEGR Global Knob 050` |
| `bodyCream` | `#BFBBB2` | large knob body | dominant of `Big_Knob_Only_Knob` |

Measured contrast (WCAG 2.1, computed on the real HEX values):

| Pair | Ratio | Verdict |
|---|---|---|
| `ink` / `window` | 15.19:1 | AAA |
| `inkMuted` / `screen` | 5.73:1 | AA |
| `accent` / `window` | 12.71:1 | AAA |
| `led` / `bodyDark` | 10.42:1 | AAA |
| `creamInk` / `cream` | 11.12:1 | AAA |
| `creamInkMuted` / `cream` | 5.26:1 | AA small text |
| `inkDim` / `window` | 4.73:1 | AA (switched-off module stays readable) |

Hard rules for v2: no loud gradients and no glassmorphism; light comes from the top left on every
edge; gold only marks state (module LED, numeric value, header rule); the VU meter is the only place
that uses amber/red, and it is real data measured in the engine, not decoration.

## 1-ter. Per-module colour (v4) and structural red

> **Superseded in v7:** module colours and the rest of the GUI palette are those of DASH-90 §3.
> This section stays as the history of v4.

**Every column is tinted with a hue** and its controls carry a ring of that colour:
NOISE `#4B2220`, WOBBLE `#463222`, MAGNETIC `#343744`.

| Module | Accent | Where it is used |
|---|---|---|
| NOISE | `#E8503F` **red** | column wash (26 % at the top to 6 % at the bottom), LED, dial ring, LCD digits |
| WOBBLE | `#F0A24A` amber | same |
| DIST | `#D8C86A` olive | same |
| DIGITAL | `#9FC4E8` ice blue | same |
| SPACE | `#7FD6A8` mint green | same |
| EQ | `#E0D9C7` cream | same |
| MAGNETIC | `#A8B6D8` slate blue | same |
| WIDTH | `#C8CBD2` light grey | same |
| MASTER | `#E0D9C7` (no module colour) | warm band; the dials use the blue-green of the LCD |

v4 rules:

- **Red is the dominant colour** (NOISE is the first column and the one the eye goes to) and it is
  also the **structural red**: a 2 px rule under the header (`redLine #D2402F`; v6 removed the
  footer). Errors and the VU meter hot zone stay on `hot #FF6345`.
- Brand gold stays **only** in the wordmark and the "VEXX AUDIO" subtitle.
- A switched-off module **keeps its hue** (wash at 5 %, title at 62 % brightness) and does not turn
  grey: that is the standby state.
- The MASTER band becomes **warm** graphite (`#17130F`) and its knobs warm steel (`#443C31`), so the
  whole does not read as a server panel.

## 1-quater. Dark theme for the web (2026-10)

The site ships a dark theme. It is not a second identity: it is the same structure with the light
values swapped for measured ones. It applies when the system asks for it, and the toggle stores the
choice; with JavaScript disabled the media query still handles it.

| Token | Light | Dark | Role |
|---|---|---|---|
| `--bg` | `#FFFFFF` | `#0B0C0F` | page background |
| `--surface` | `#F3F4F6` | `#14161B` | panels, code blocks |
| `--ink` | `#2E3338` | `#E8E4DA` | body text and headings |
| `--muted` | `#6B7280` | `#A6ACB4` | metadata, units, footer |
| `--accent` | `#7B1E3A` | `#D9627F` | links, call to action, rules |
| `--accent-on` | `#FFFFFF` | `#0B0C0F` | text on the accent |
| `--border` | `#E5E7EB` | `#262A31` | tables and separators |
| `--row-alt` | `#F9FAFB` | `#101216` | alternate table rows |

Measured contrast (WCAG 2.1, on the real HEX values):

| Pair | Light | Dark |
|---|---|---|
| ink / bg | 12.75:1 | 15.41:1 |
| muted / bg | 4.83:1 | 8.55:1 |
| accent / bg (links) | 10.05:1 | 5.59:1 |
| accent-on / accent (button) | 10.05:1 | 5.59:1 |

Rules for the dark theme: burgundy `#7B1E3A` cannot carry text on a dark background (1.84:1), so the
accent lifts to rose `#D9627F` while staying in the same family. Gold keeps its role: a detail,
never body text. The plugin screenshots are dark, so the plate uses `--surface` and `--border`:
they must read as part of the page. Nothing else changes: same spacing, same type scale, same
components.

## 2. Typography

- **Orbitron** (variable 400-900, OFL): headings, section labels, module names, wordmark.
- **Rajdhani** Medium 500 (OFL): body, numeric values, tooltips.
- 1.25 scale over 12 px: 12 / 15 / 18.75 / 23.4 / 29.3. Body 12 px, line-height 1.55.
- On the web the same 1.25 scale is anchored at 16 px (16 / 20 / 25 / 31 / 39): 12 px is the print
  and plugin-UI size, too small for a page body. The web pages in `docs/` and in the `vexxplugins.github.io`
  repository use it.
- Numbers are always tabular (`font-variant-numeric: tabular-nums`) so knobs do not jitter.

## 3. Logo (hexagon + circuit, "Agent-Circuit")

Exact geometry, so it can be redrawn as vector (the kit on this machine has no SVG; it is generated
from this contract):

- Hexagon (shield), `stroke #7B1E3A`, 3 px, interior `#F3F4F6`.
- Six spokes `#2E3338` from the centre to the vertices, with a **red node** `#7B1E3A` at each tip.
- **Gold** core `#FFC94A`, radius 9, with a `#2E3338` centre dot ("processor").
- Wordmark **RETROEXX** in Orbitron 800, `#2E3338`, letter-spacing 3.
- Sub: **VEXX AUDIO** in Orbitron 600, 10 px, `#FFC94A`, letter-spacing 3.
- No taglines. Solid fills and strokes only (safe for PDF and for scaling).

## 4. Spacing, radii, density

- 8-point grid: 4 / 8 / 16 / 24 / 32 / 48 / 64.
- Radii: 0 for panels and tables, 2 px for controls, 50 % only on the logo core. Square corners by
  identity, not by fashion.
- 62 / 38 proportion: the control area on the right carries no wide blocks.

## 5. Plugin interface structure (v6)

The window measures **1240 x 957**: the reference canvas (797 x 615) times `kScale = 1240/797`
(`ui::geometry` in `Theme.h`). Every rect comes from the scaled canvas, not from distributing the
leftover space. Top to bottom:

| Zone | y (px) | Content |
|---|---|---|
| Header | 0-107 | logomark + RETROEXX + VEXX AUDIO; **preset VFD window** (status line on top: PRESET, last message or engine error in amber; selector below), previous/next arrows, **LOAD**, **SAVE**; **MAGNITUDE fader**. 2 px red rule at 107-109 |
| Screen | 121-599 | **6 columns in ONE row** (128 px pitch: 74 margin, 124 wide, 310 tall): the module screen on top (name + VU meter; on hover/drag, the **trip computer**), the pod with its controls in absolute position below |
| Band | 599-792 | **6 large knobs** with a VFD ring, one per column and aligned with it: the module Amount (macro), its **Enable** at the top left, a fixed seven-segment readout at the top right and the engraved name below |
| Brand | 792-836 | VEXX AUDIO centred, version on the right |
| Strip | 836-957 | **IN** (GAIN) · **EQ** (Enable, low cut mode, 2-handle CUT, high cut mode, TONE and its mode) · **OUT** (WIDTH, GAIN). Vertical labels and separators; legend + value in seven-segment digits under each knob |

There is no footer: messages (preset saved, load error, engine error) go on the status line of the
preset window.

**Trip computer** (DASH-90 §7): each column's screen shows the name of the control under the mouse
(or being dragged) and its value in italic seven-segment digits with the unit. With no hover it
returns to the module name and its VU meter. That is why the column controls have **no** LCD of
their own: the pod paints the legend.

### Controls per column, one by one

There are no filler knobs: every slot carries the control it should (`columnLayout` in
`src/plugin/HostModules.cpp`).

| Column | Top | Row 1 | Row 2 | Row 3 | Bottom |
|---|---|---|---|---|---|
| NOISE | TYPE **selector** (`VINYL 1`…`APOLLO`) | TONE (knob) + POST **button** | FOLLOW + DUCK (knobs) | - | FLUX **slider** |
| WOBBLE | WOW ⇄ FLUTTER **crossfade** | RATE + RATE (knobs) | STEREO **button** + MIX (knob) | - | FLUX **slider** |
| DIST | TYPE **selector** (`TUBEPAIR`…`AIR`) | **FOCUS** (2 handles) + MIX (knob) | - | - | FLUX **slider** |
| DIGITAL | RATE ⇄ BITS **crossfade** | **FOCUS** (2 handles) + MIX | SMOOTH (knob) + CUT **button** | - | FLUX **slider** |
| SPACE | DECAY **slider** | **FOCUS** (2 handles) + PREDELAY | STEREO **button** | - | FLUX **slider** |
| MAGNETIC | WEAR ⇄ FLUTTER **crossfade** | STEREO **button** + FLUTTER RATE | DROPOUTS (knob) | - | FLUX **slider** |

EQ strip: **Enable** + **CUT** (2 handles, horizontal) + three **pairs of stacked keys** (top = 0,
bottom = 1, two states per key: low cut Soft/Hard, high cut Soft/Hard, Tone Tilt/Mid) + TONE
**knob**. Header: MAGNITUDE **fader**.

Type names come from the parameter itself (`hostParamChoiceName`), so the GUI selector and the host
cannot show different things.

Distribution of the 58 parameters (checked by `tests/test_ui_snapshot.cpp`, so it is not a list that
can lie): every id edited by **some** editor control, **exactly once**. The module Enable lives only
in the band: the column has no switch.

v6 rules:

- A module's **Amount is not repeated** in its column: it lives in the large knob.
- The column **listens** to Enable (it dims when off) but does not edit it.
- Values are read in seven-segment digits: in the trip computer (columns), in each plate's window
  (band) and under each knob (strip). Text that is not a number (type names) goes in Rajdhani inside
  the same glass.
- Required states: hover, focus (gold ring), active, disabled. Double click returns to the default
  value.

## 6. FORBIDDEN in this project

- Glassmorphism, mesh/aurora gradients, purple/indigo. (Dark backgrounds are allowed **only** in the
  plugin GUI since v2; in documents white still rules.)
- Inter, Space Grotesk or Geist as the typeface.
- Emojis as icons; single-weight vector iconography (Lucide or equivalent).
- Em dashes in headlines and short interface copy.
- `outline: none` with no focus replacement; controls with no hover/focus/active/disabled state.
- Invented data in the UI (fake meters, marketing figures, testimonials).
- Any third-party graphic asset (logo, knobs, textures, screenshots). The UI is our own.

## 7. Assets

- Palette, typography and structure: as fixed by this document. The geometry of the six columns,
  the large-knob band and the IN/OUT bar are defined here, not imported.
- `Orbitron` (weights 500 to 900) and `Rajdhani-Medium.ttf` (Google Fonts, OFL) in
  `src/plugin/ui/assets/fonts/`, with `OFL-Orbitron.txt` and `OFL-Rajdhani.txt`.
- Logo: SVG generated from the §3 geometry in `src/plugin/ui/assets/logomark.svg`; it is the single
  source of the mark (the editor draws it from that file).

Checks before calling the GUI finished: the `frontend-master` slop audit (§5) over the UI files,
AA contrast measured on the real HEX values, and a rendered screenshot reviewed.
