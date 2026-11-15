# M0 — Shared foundation for three engine tracks

A personal learning project on Direct3D 12 with three tracks built on one foundation:

- **Track A — inference engine:** GPU kernels, a tiny inference runtime, quantization, and a layer-wise accuracy-alignment tool.
- **Track B — game engine:** ML inference inside a real-time frame: budgets, async compute, a budget-aware scheduler.
- **Track R — rendering:** the naive renderer made fast step by step: instancing, bindless, GPU culling, meshlets, multithreaded recording.

Both start from **M0**, which does one thing: **render something real, measure it rigorously, and prove nothing broke.** It isn't meant to be fast. It builds the measurement and validation loop that every track uses for every later step.

- M0 design: `DESIGN.md` (`DESIGN.zh-CN.md`)
- Overall plan, separation rules and track plans: kept outside this repo, in `../tracks/` (`ROADMAP.md`, `A-inference/`, `B-game-engine/`, `R-rendering/`)

## What M0 does

- Opens a window and renders the Sponza test scene with simple lighting.
- Free-fly camera, plus a deterministic camera path for repeatable benchmarks.
- Per-frame and per-pass CPU/GPU timing, with `--benchmark` writing a CSV + metadata.
- Golden-image validation: capture fixed views, compare pixel-for-pixel against a reference set.
- Runs clean under the D3D12 debug layer (0 errors, 0 warnings).

## Milestones (this repo evolves in place)

| | Track | Focus |
|---|---|---|
| **M0** | Shared | Render + measure + self-check (current) |
| A1 | A | GEMM on D3D12 compute: naive → tiled → FP16 → INT8, against a measured roofline |
| A2 | A | Tiny inference runtime; a public detector end to end |
| A3 | A | INT8/FP16 quantization + layer-wise alignment tool (HTML report) |
| A4 | A | Benchmark against an established runtime; write-up |
| A5 | A | Market models: a small transformer (NLU) |
| B1 | B | Neural upscaler inside the frame, within a budget |
| B2 | B | Async compute: inference overlapping graphics |
| B3 | B | Budget-aware scheduler; demo + write-up |
| R1 | R | Naive baseline, pushed until it struggles |
| R2 | R | Instancing → bindless → indirect draw + GPU culling → meshlets |
| R3 | R | Multithreaded command recording |

Suggested order: M0 → A1 → A2 → (A3 ∥ R1) → then a second active track (B or R), alternating with A; at most two tracks active at a time. Code for all tracks lives in this repository, marked with git tags (`m0`, `a1`, `b1`, `r1`, …). Code, notes and results are kept separate per track; see `../tracks/ROADMAP.md` §4.

## Build

Requires a C++20 compiler, CMake, and Ninja. Primary setup is MinGW-w64 GCC (see `ENVIRONMENT.md`); the same CMake project also builds with Visual Studio / MSVC.

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

Fetch the test scene first:

```sh
python tools/fetch_assets.py
```

## Layout

```
DESIGN.md / DESIGN.zh-CN.md     M0 design doc (EN / ZH)
ENVIRONMENT.md                  toolchain + setup checklist
src/                            shared: platform, rhi, profile, capture, core
                                M0 renderer: app, asset, render, camera
                                later: infer/ (A), game/ (B); R evolves the renderer
shaders/                        HLSL
third_party/                    pinned dependencies
tools/                          summarize.py, compare.py, fetch_assets.py
golden/                         committed reference images
notes/                          hand-written notes: m0/, A/, B/, R/
results/                        benchmark output: m0/, A/, B/, R/ (only .md reports committed)
```

## Usage

```sh
m0                                      # interactive: free-fly camera + live stats
m0 --benchmark --runs 3 --frames 1000   # run the fixed path, write results/m0/
m0 --capture --out captures/<name>/     # render golden poses to PNG
m0 --validate                           # debug-layer + GPU validation, fail on any message
python tools/summarize.py results/m0/<timestamp>/
python tools/compare.py golden/ captures/<name>/
```

## Scope

See `DESIGN.md` §3 for what M0 deliberately leaves out (shadows, PBR, post-processing, multithreading, editor, and more) — scope creep is the main risk, so new ideas go to a `later.md` list rather than into M0.

---

*Personal learning project. Not affiliated with or derived from any employer's code or data.*
