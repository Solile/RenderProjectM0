# M0 Design Doc: Foundation (render, measure, self-check)

> **Status:** Draft v0.2, prepared by Claude on 2026-11-11 for the owner to revise.
> **v0.2 (2026-11-11):** the project now has three tracks (A: inference engine, B: game engine, R: rendering, the former M1–M3). M0 is their **shared foundation**; its scope is unchanged. Later-milestone references now point to the tracks. See `../tracks/ROADMAP.md` (kept outside this repo).
> Anything marked **[DECIDE]** is a choice for you to make. All §13 questions are answered (golden poses deferred).

---

## 1. Why M0 exists

M0 is the **shared foundation** for the three tracks in `../tracks/ROADMAP.md`: Track A (inference engine: GPU kernels, quantization, accuracy alignment), Track B (game engine: inference inside a real-time frame) and Track R (rendering: making this renderer fast step by step). All are optimization work proven step by step with data, and all need three things before any optimization starts:

1. **Something real to render.** A standard scene (Sponza) gives a meaningful workload, and results can be compared with other people's.
2. **A way to measure.** CPU and GPU time per frame and per pass, collected under a strict, repeatable method.
3. **A way to prove nothing broke.** API validation (debug layer) plus pixel comparison against a reference image (golden image).

M0 builds exactly these and nothing else. **M0 is not about being fast.** Its numbers become the *baseline* that later milestones are compared against. The same loop carries over: GPU timestamps time kernels (A) and frame passes (B); golden images become golden tensors for alignment (A), the quality check for the upscaler (B), and the proof that an optimization didn't change the image (R).

| Work habit (TPU boards) | M0 equivalent |
|---|---|
| Fixed test video, 900 frames | Fixed scene + deterministic camera path |
| Layer-by-layer profile (TIU/GDMA timeline) | GPU timestamps per render pass |
| md5 / bit-exact regression | Golden image comparison |
| Writing down how metrics are measured (fps tail issue) | Measurement methodology (§5) |
| Measuring a baseline before setting targets | M0 output = baseline report |

---

## 2. Goals (acceptance criteria)

M0 is **done** when every item below can be answered "yes":

- [ ] **G1** The program opens a window and renders Sponza with simple lighting. A free-fly camera works (WASD + mouse).
- [ ] **G2** With the D3D12 debug layer on, a full benchmark run produces **0 errors and 0 warnings**. The only exceptions are listed, justified suppressions.
- [ ] **G3** The window shows live CPU frame time, GPU frame time, and per-pass GPU time.
- [ ] **G4** `--benchmark` runs the fixed camera path for N frames and writes a per-frame CSV plus a metadata JSON (§5.4).
- [ ] **G5** A script turns the CSV into a summary: mean, p50, p95, p99, max, stddev, and spread across runs.
- [ ] **G6** `--capture` saves screenshots at fixed poses; a compare script reports differing pixels, max channel difference and PSNR against the golden set.
- [ ] **G7** Running the same build twice on the same machine gives **identical golden images** (0 differing pixels).
- [ ] **G8** A baseline report (`results/m0/baseline_<date>.md`) exists, **written by the owner in their own words**.

---

## 3. Non-goals (explicitly out of scope)

Shadows · PBR / normal maps · post-processing · anti-aliasing · transparency sorting · multithreaded command recording · job system · editor / UI beyond a stats overlay · hot reload · asset cooking · mesh shaders · bindless.

Anything not listed in §2 waits for a later milestone. Scope creep is the biggest risk to M0.

---

## 4. Platform and constraints

| | Primary (company laptop) | Optional later (home) |
|---|---|---|
| GPU | Intel integrated, Feature Level 12_2, **UMA** | NVIDIA RTX 4060, discrete |
| Toolchain | MSYS2 GCC 16 + CMake + Ninja | Visual Studio 2022 Community + CMake |
| Profilers | PIX, Intel GPA | PIX, Nsight Graphics |

- The project is developed on the **company laptop**, because that is the only machine where the owner can work with Claude. The build, run, and profiling all happen there on the iGPU (the probe in `ENVIRONMENT.md` confirms it handles D3D12 fully). GitHub is the backup and portfolio remote.
- The **home RTX 4060 is optional and later**: after M0, pull the repo at home and re-run the same benchmarks for nicer performance numbers. Not required for M0.
- **API:** Direct3D 12 only (no abstraction layer over multiple APIs in M0).
- **Language:** C++20.
- **Build:** CMake + MinGW/GCC (Ninja generator). Q1 in §13 is settled: **MinGW/GCC only** — Visual Studio Community's license restricts use in a large company, so MSVC is avoided on the work machine. The home 4060, if used later, can build the same CMake project with MSVC.
- **Debug layer:** via the D3D12 Agility SDK (a NuGet download placed next to the exe, no admin, no system install; see `ENVIRONMENT.md`).
- **UMA note:** the iGPU shares memory with the CPU (UMA), much like the owner's TPU boards. The upload→default copy in §7.2 may be redundant here; M0 keeps the standard path anyway so it stays correct on a discrete GPU, and measuring the UMA shortcut is a Track A experiment (A1).

---

## 5. Measurement methodology

This section matters most. If the method is wrong, every number after it is meaningless.

### 5.1 Frame budget (reference line, not a target)

- **Reference:** 1920×1080 @ 60 fps → **16.6 ms/frame**.
- M0 records where it stands relative to this line. Missing it is fine.

### 5.2 Benchmark run settings

| Setting | Value | Reason |
|---|---|---|
| Render resolution | Fixed 1920×1080, independent of window size | Window resizing must not change the workload |
| V-sync | Off (`Present(0, ALLOW_TEARING)` when supported) | V-sync caps frame time at 16.6 ms and hides the real cost |
| Frames in flight | 2 | Standard. Fixed so it's not a hidden variable |
| Warm-up | First **120 frames** discarded | Excludes first-use shader compilation, uploads, clock ramp-up |
| Measured frames | **1000** per run | Settled (§13) |
| Runs | **3–5**, with a **20–30 s** cool-down between runs (settled, §13) | iGPU thermal throttling is real; spread across runs shows it |
| Power | AC power, Windows "Best performance" mode | Battery mode changes clocks. The app records AC status automatically |
| Build | Release (`-O2`), debug layer **off** for timing runs | The debug layer adds CPU overhead. Validation runs are separate (G2) |
| Camera | Deterministic path driven by **frame index**, not wall-clock time | Frame *i* always shows the same view, however fast the program runs |

### 5.3 What is measured

| Metric | How | Notes |
|---|---|---|
| CPU frame time | `QueryPerformanceCounter` between consecutive frame starts | Includes waiting on the GPU, so it reflects the real frame rate |
| CPU work time | Frame start → `Present` call returns, excluding fence waits | Shows whether the CPU is the bottleneck |
| GPU frame time | Timestamp query at the start and end of the frame's command list | — |
| GPU per-pass time | Timestamp pair around each pass (M0: `Clear`, `Opaque`) | Each pass's share of the GPU frame |
| Fence wait time | Time the CPU blocks waiting for a frame slot | High = GPU-bound |

- **Bottleneck rule of thumb:** high fence wait → GPU-bound. GPU time well below CPU frame time → CPU-bound.
- GPU timestamps are read back **2 frames later** (frames in flight), through a readback buffer.
- **Known caveat:** without developer mode, `SetStablePowerState` is unavailable, so GPU clocks float. Report the spread across runs; don't hide it. **Settled: no clock locking during development.** Enable developer mode and lock clocks later, when recording results, aiming for the sustained clock at TDP (note: `SetStablePowerState` uses a driver-chosen clock, which may not be exactly selectable).

### 5.4 Output format

`results/m0/<timestamp>/run<k>.csv`, one row per measured frame:

```
frame,cpu_frame_ms,cpu_work_ms,fence_wait_ms,gpu_frame_ms,gpu_clear_ms,gpu_opaque_ms
```

`results/m0/<timestamp>/meta.json`: GPU name, driver version, resolution, build type, git commit, compiler, AC power status, date, warm-up and measured frame counts.

**Summary rules (computed by script, not by the engine):** mean, p50, p95, p99, max, stddev for each column; spread across runs (max mean − min mean). **The headline number is p95 GPU frame time, not the mean** (settled): the mean hides stutter.

---

## 6. Golden image validation

- **Poses:** 5 fixed points on the benchmark path (frame indices 0, 200, 400, 600, 800), provisional. [DECIDE later: pick once the camera path exists]
- **Format:** back buffer read back through a readback buffer, saved as PNG at 1920×1080.
- **Determinism requirements in M0:** no time-based animation, no random numbers, no temporal effects. Output must be a pure function of (build, scene, pose).
- **Compare script reports:** differing pixel count, max absolute channel difference, PSNR.
- **Pass thresholds:**
  - Same machine, same driver: **exactly 0 differing pixels** (G7).
  - Different driver or GPU: tolerance TBD. Measure it first by comparing the laptop against the 4060, then set the threshold. [DECIDE later]
- **When a golden image may be updated:** only when a change is *intended* to alter the image. The commit message must say why.

---

## 7. Architecture

### 7.1 Modules

```
app/        main loop, command-line parsing, mode selection (interactive / benchmark / capture)
platform/   Win32 window, input, high-resolution timer
rhi/        thin D3D12 layer: device, queue + fence, swap chain, descriptor heaps,
            upload helper, barrier helpers, timestamp query heap
asset/      glTF loading (cgltf) + image decoding (stb_image) → CPU-side mesh/material data
render/     the passes (Clear, Opaque), per-frame constants
camera/     free-fly camera + deterministic path (keyframes, quaternion slerp)
profile/    CPU scoped timers, GPU timestamp collection, CSV/JSON writer, on-screen stats
capture/    back-buffer readback → PNG
core/       math library, linear allocator (hand-written by the owner; see §9)
tools/      Python: summarize.py (CSV → stats), compare.py (image diff)
```

"rhi" (Render Hardware Interface) is the engine-side wrapper over the graphics API. In M0 it's deliberately thin: just enough to avoid repeating D3D12 boilerplate. It's not meant to be a general abstraction.

### 7.2 Data flow (load time)

```
Sponza.gltf + .bin + textures
  → cgltf parse → CPU mesh (positions, normals, UVs, indices) + materials (base-color texture)
  → copy into an UPLOAD-heap buffer
  → GPU copy → DEFAULT-heap vertex/index buffers and textures
  → barriers: COPY_DEST → vertex/index buffer / pixel-shader resource states
```

> **UMA note:** on this iGPU the upload→default copy may be unnecessary, since CPU and GPU share memory. M0 still uses the standard path so it also runs correctly on a discrete GPU (the home 4060). Measuring a UMA-specific path is a **Track A experiment (A1)**.

### 7.3 Frame lifecycle (run time)

```
1. Wait on the fence for this frame slot (record fence_wait_ms)
2. Reset this slot's command allocator; reset its linear allocator
3. Read back GPU timestamps from 2 frames ago
4. Record the command list:
     timestamp[frame_begin]
     barrier: back buffer PRESENT → RENDER_TARGET          ← hand-written (§9)
     timestamp[clear_begin]  clear color + depth  timestamp[clear_end]
     timestamp[opaque_begin] draw every mesh, one draw call each   timestamp[opaque_end]
     (overlay text)
     barrier: back buffer RENDER_TARGET → PRESENT          ← hand-written (§9)
     timestamp[frame_end]; resolve queries into the readback buffer
5. ExecuteCommandLists → Present → Signal fence
```

**"One draw call per mesh" is deliberate.** It's the naive baseline that Track R optimizes (instancing, bindless, indirect draws, GPU culling; see `../tracks/R-rendering/README.md`).

### 7.4 Shading (M0)

- Vertex: position, normal, UV. Constants: view-projection matrix and model matrix, per draw.
- Pixel: base-color texture × (Lambert directional light + constant ambient).
- Alpha-tested materials (Sponza's foliage): `clip()` on alpha < 0.5. **Settled: foliage is in M0, drawn the naive way** (alpha test only; no sorting, no alpha-to-coverage).
- Shaders compiled at startup with `D3DCompile` (SM 5.1). Switch to DXC when the first track needs SM 6.x (A1: 16-bit types and INT8 dot products; R2: bindless and mesh shaders).

### 7.5 Third-party code

| Library | Purpose | License |
|---|---|---|
| cgltf | glTF parsing | MIT |
| stb_image / stb_image_write | Texture decode, PNG write | Public domain / MIT |
| D3D12 Agility SDK | Debug layer, current D3D12 runtime | Microsoft (redistributable) |
| Dear ImGui | Stats overlay | MIT (settled: ImGui overlay) |

All of these go into `third_party/` with pinned versions.

---

## 8. Command-line interface

```
m0.exe                                    interactive (free-fly camera, live stats)
m0.exe --benchmark --runs 3 --frames 1000 --warmup 120 --out results/m0/
m0.exe --capture --out captures/<name>/   render the golden poses, save PNGs, exit
m0.exe --validate                         benchmark path with debug layer + GPU-based validation; fail on any message
python tools/summarize.py results/m0/<timestamp>/
python tools/compare.py golden/ captures/<name>/
```

---

## 9. Owner hand-written parts

The rule: **AI writes most of the code. These parts the owner writes alone, without AI, then asks for review afterwards.** They're small, and they're where real understanding (and interview questions) live.

| Part | Where it's used in M0 | Minimum scope | Check |
|---|---|---|---|
| **Math library** | Camera, view/projection matrices, path interpolation | `vec3/vec4/mat4`, multiply, transpose, inverse, `lookAt`, `perspective` (D3D: left-handed, depth 0..1), quaternion (axis-angle, multiply, rotate vector, slerp, to-matrix) | Unit tests against GLM or DirectXMath results |
| **Linear allocator** | Per-frame constant data in an UPLOAD-heap buffer (one per frame slot) | `alloc(size, align)`, `reset()`, overflow assert. **Constant buffers need 256-byte alignment** | Overflow and alignment tests; per-frame usage statistic |
| **Barriers** | Back buffer PRESENT ↔ RENDER_TARGET; COPY_DEST → shader-resource at load | Write each transition by hand, no helper that infers states | Debug layer clean (G2). Exercise: remove one barrier on purpose and record what the debug layer says |

The **task queue** isn't needed until the shared job system (`core/jobs/`, first used by R3 or B2), so it stays a home exercise for now. Each track lists its own hand-written parts in its README.

**Settled: AI-written placeholders are allowed** so M0 isn't blocked. Rules:

- Placeholders live in separate, clearly marked files (header: `PLACEHOLDER — to be replaced by owner's hand-written version`).
- AI writes the unit tests first; the placeholder and the owner's version must pass the **same** tests. The tests define "done".
- While the owner writes, AI writes no code. When stuck, use the hint ladder: 1 concept hint → 2 point to where the bug is → 3 show the line (use sparingly; log it in `notes/`).
- Suggested replacement order: barriers (T1) → linear allocator (around T3) → math lib (before T4).

---

## 10. Task order

Measurement comes **before** content, so every later task is measured from day one.

| # | Task | Done when |
|---|---|---|
| T1 | CMake project, window, device, swap chain, clear screen, Agility SDK debug layer | Window shows a clear color; debug layer active and clean |
| T2 | CPU timers + GPU timestamp queries on the clear pass; stats in the window | Clear-pass GPU time visible |
| T3 | glTF load + upload + Opaque pass (one draw per mesh) | Sponza visible |
| T4 | Free-fly camera + deterministic path | Path replays identically |
| T5 | `--benchmark` + CSV/JSON + `summarize.py` | G4, G5 |
| T6 | `--capture` + `compare.py` + golden set | G6, G7 |
| T7 | `--validate` mode; clean all debug-layer messages | G2 |
| T8 | Baseline runs + owner-written report | G8 |

For each task: the owner states the goal and how to verify it → AI implements → the owner reviews and runs the check → the owner writes 3–5 lines of notes in their own words in `notes/`.

---

## 11. Directory layout

```
M0/
  README.md  DESIGN.md  ENVIRONMENT.md   (+ .zh-CN.md versions)
  CMakeLists.txt
  src/ app/ platform/ rhi/ asset/ render/ camera/ profile/ capture/ core/
  shaders/
  third_party/       pinned dependencies
  assets/            Sponza (not committed; fetch script instead)
  tools/             summarize.py, compare.py, fetch_assets.py
  golden/            reference PNGs (committed)
  results/           benchmark outputs (not committed, except .md reports); M0 writes to results/m0/
  notes/             owner's per-task notes; M0 notes go in notes/m0/
```

Track plans live outside this repo, in `../tracks/`. Track code (`src/infer/`, `src/game/`, …) is added here by the tracks, following the dependency rules in `../tracks/ROADMAP.md` §4.

---

## 12. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Scope creep ("just add shadows…") | M0 never finishes | §3 non-goals; new ideas go to a `later.md` list |
| iGPU thermal throttling / clock variance | Noisy numbers | Warm-up, 3 runs, cool-down, report spread; optional stable power state |
| MinGW + D3D12 friction | Time lost on build issues | T1 settles this first; the probe already compiled a D3D12 program with this GCC |
| Intel driver quirks | Bugs that aren't yours | Debug layer; re-check against the 4060 at home if something looks wrong |
| Sponza variants differ (Khronos vs Intel) | Results not comparable | Pin the exact asset version in `fetch_assets.py` |
| Company-policy questions | Can't continue at home | Keep the repo free of company content; ask your lead before moving it |

---

## 13. Open questions for the owner

1. ~~Must M0 build on both GCC and MSVC?~~ **Settled: MinGW/GCC only** (company laptop; MSVC avoided for licensing). (§4)
2. ~~Measured frames, runs, cool-down length?~~ **Settled: 1000 frames per run, 3–5 runs, 20–30 s cool-down.** (§5.2)
3. ~~Headline metric: p95 GPU frame time?~~ **Settled: yes, p95**; it exposes what the mean hides. (§5.4)
4. ~~Developer mode for stable GPU clocks?~~ **Settled: not during development; lock to the sustained TDP clock later, when recording results.** (§5.3)
5. Golden poses: which frames? **Decide later**, once the camera path exists. (§6)
6. ~~Alpha-tested foliage in M0?~~ **Settled: yes, drawn the naive way.** (§7.4)
7. ~~ImGui overlay or window title?~~ **Settled: ImGui overlay.** (§7.5)
8. ~~AI placeholders until your versions are ready?~~ **Settled: allowed; rules in §9.** (§9)
