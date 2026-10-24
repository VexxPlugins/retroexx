# ROADMAP

Engine and interface status, plus what is still open. Anything marked *verified* has a test that
checks it; anything that does not is declared as provisional in the code itself and here.

## Done

| Piece | Status |
|---|---|
| Own `Block` framework (`PortData`, `iBlock`, `Condition`, `Container`, `PlugChain`) | done |
| Engine graph: 22 nodes, 13 classes, 125 model parameters | done (`src/gen/`) |
| Host surface | **58/58 parameters** with id, range, default and type |
| Host-to-model laws | **50/58** (39 verified, 11 identity) |
| Blocks with their own DSP | **12/17**: Noise, SComp, Wobble, Magnetic, Dist, Digital, Space, EQ_Post, EQ_Post_Tone, Width, and the two DSL chains |
| Engine | full graph, 64-sample quantum, **64-sample latency**, any block size |
| Wrapper | JUCE 8.0.13: **AU + VST3**, `auval` PASS, own editor, state with magic `RTXX` |
| Desktop demo | `RetroexxDemo.app`: the real editor in a window, no audio of its own |
| Presets | **10 factory presets** (host programs) and user presets on disk |
| Tests | `ctest`: **17/17** without JUCE, **19/19** with JUCE |
| Performance | with every module active: **9.5x to 31x** real time (worst case 96 kHz, 64-sample block) |

## Open (declared, not filled in by guesswork)

1. **Flux signals** (`*_Flux`, `WearGain`, `DepthSteerSig`, `DropOutSteerSig`). These are what link
   macros to modulation. Today they hold whatever the model parameters say in their factory state,
   which is the no-flux case; the generator is not implemented.
2. **DIGITAL filters** (`Filter Lo/Hi/Mode`). With the filters open the block does not colour the
   signal, but at intermediate positions the `log 20..20000` mapping is not verified: the actual cut
   moves with the macro state. `FilterLo` already carries the macro factor; the rest stays
   provisional and is declared as such in `src/blocks/BitColor.h`.
3. **DIST filters** (`Filter Lo/Hi`). The `log 20..20000` law lowers the error but does not reach
   1 dB, so it is not applied: it would change the factory sound.
4. **`WaveShapeMorph`** and the exact algebra of the LFO warp (shape 1). Only the `warp = 0.5` case
   is guaranteed, which is the factory one.

## Next

- Close item 1 so macros move with modulation, not only in a static setting.
- Publish signed, notarised macOS binaries and a small product page.
- Add a `CHANGELOG.md` once there is a first release beyond 0.1.
