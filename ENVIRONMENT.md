# M0 Environment — Company Laptop (verified)

Checked on 2026-11-11. The check was read-only: nothing was installed, no system settings changed. A throwaway D3D12 probe was compiled in a temp folder, run, and deleted.

**Updated 2026-11-15 (T1):** CMake + Ninja installed; Agility SDK wired into the build and verified; the debug layer turned out to need the Windows *Graphics Tools* optional feature as well, which is now installed (see below).

The project is developed here because this is the only machine with Claude access. Build, run, and profiling all happen on this machine's iGPU. GitHub is the backup / portfolio remote. The home RTX 4060 is optional and only for re-running benchmarks later (after M0).

## Hardware and GPU capabilities (probe results)

| Item | Result | Why it matters |
|---|---|---|
| CPU | Intel Core Ultra 5 225H | — |
| GPU | Intel(R) Graphics (integrated), 128 MB dedicated + ~9 GB shared | The 128 MB is only a reserved carve-out; the GPU uses system RAM |
| RAM | 16 GB | Shared between CPU and GPU |
| OS | Windows 11 (build 26200) | — |
| D3D feature level | **12_2** (DX12 Ultimate) | Everything in `../tracks/ROADMAP.md` (all tracks) is supported |
| Shader model | **6.6** | Bindless via `ResourceDescriptorHeap` works; also covers 16-bit types (6.2) and INT8 dot products (6.4) for Track A; bindless for Track R |
| Resource binding tier | **3** | Bindless works |
| Mesh shader tier | **1** | Meshlets (Track R, R2) work |
| Raytracing tier | **1.1** | Optional later work |
| Architecture | **UMA = 1**, CacheCoherentUMA = 0 | CPU and GPU share one memory pool, like the TPU boards |
| `D3DCompile` (SM 5.1) | **Works** | M0 compiles shaders with zero extra setup |
| **D3D12 debug layer** | **Works** since 2026-11-15 (was missing, `0x887a002d`) | Needs the *Graphics Tools* optional feature, **even with the Agility SDK**; see below |
| Developer mode | Off | `SetStablePowerState` (lock GPU clocks) unavailable without it |

**Verdict: the iGPU runs D3D12 fully. No milestone is gated by hardware.**

## Toolchain status

| Component | Status | Notes |
|---|---|---|
| MSYS2 + GCC 16.1 (ucrt64) | Installed (`D:\ywfan2\MSYS2_mingw`) | **Verified: builds T1.** `ucrt64/bin` must come **first** on `PATH` (see caveats) |
| D3D12 / DXGI / d3dcompiler headers | Included with MinGW | Verified present (`D3D12_SDK_VERSION` 602; fine with the 620 runtime) |
| CMake / Ninja | **Installed 2026-11-15** (`cmake 4.4.0`, `ninja 1.13.2`) | Via MSYS2 `pacman`, no admin |
| D3D12 Agility SDK | Fetched by CMake at configure time | Pinned 1.620.2, SHA256-checked, copied to `<exe dir>/D3D12/`. **Verified: the exe loads this runtime, not the OS one** |
| Windows *Graphics Tools* (optional feature) | **Installed 2026-11-15** | Required by the debug layer. Via DISM (admin, Windows Update, not blocked); took ~25 min, mostly a long pause at 50 % |
| Visual Studio / MSVC / Windows SDK | Not installed | Avoided on this machine for licensing; MinGW is used instead |
| Git | Installed | — |
| Python 3.10.8 + pip 22.2 | Installed | For `summarize.py` / `compare.py`. **NumPy 2.2.6 already installed**; PyTorch not installed (checked 2026-11-11) |

## Network reachability (from this machine)

| Source | Reachable | Used for |
|---|---|---|
| github.com (pages, `/raw/` paths, release downloads) | **Yes (200)** | Sponza, DXC, libraries, and **pushing the repo** |
| raw.githubusercontent.com | **No (times out)** | Use `git clone` or `github.com/<repo>/raw/<branch>/...` |
| NuGet (api.nuget.org) | Yes | Agility SDK (latest 1.620.2) |
| MSYS2 mirrors | Yes | CMake, Ninja |
| winget | Yes | PIX is `Microsoft.PIX` |
| pypi.org | Yes, but slow (index page took >15 s) | Prefer a mirror |
| Tsinghua / Aliyun PyPI mirrors | **Yes, fast** (~1 MB/s wheel download) | Track A Python packages (`pip -i <mirror>`) |
| download.pytorch.org | **Yes** (~1 MB/s) | PyTorch CPU wheels (`torch 2.14.1+cpu` for cp310 / win_amd64 available) |
| images.cocodataset.org | **http only** (https fails); **very slow** (~23–60 KB/s, checked 2026-11-11) | Official COCO. val2017.zip 815,585,330 B (~10 h at this rate); annotations_trainval2017.zip 252,907,541 B (~2–3 h). Single images are OK |
| hf-mirror.com (third-party Hugging Face mirror) | **Yes, fast** (~750 KB/s) | COCO val as parquet (`detection-datasets/coco`, 2 shards, ~807 MB, ~20 min). huggingface.co itself times out |

## Policy observations

- Account is in the local Administrators group; elevated installs are possible via UAC.
- AppLocker is enforced but the only deny rule targets OBS. Programs built anywhere are allowed to run.
- No WSUS / Feature-on-Demand restrictions found.
- **Technically possible ≠ allowed.** Running a personal project on the work machine is a judgment for the owner. A one-line heads-up to their lead clears it.

## Setup checklist (what to do before T1)

- [x] **CMake + Ninja**, no admin, into the existing MSYS2:
      `pacman -S --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`
- [x] **D3D12 Agility SDK** (NuGet `Microsoft.Direct3D.D3D12`): nothing to do by hand. `cmake/AgilitySDK.cmake` downloads the pinned package and copies `D3D12Core.dll` + `d3d12SDKLayers.dll` to `<exe dir>/D3D12/`.
- [x] **Graphics Tools** optional feature, needed for the debug layer (admin, downloads from Windows Update): Settings → System → Optional features → "Graphics Tools", or
      `dism /Online /Add-Capability /CapabilityName:Tools.Graphics.DirectX~~~~0.0.1.0`.
      Microsoft offers no standalone download. On a machine without it, run `m0 --no-debug-layer` and validate elsewhere.
- [ ] **Shader compiler:** nothing — `D3DCompile` already works. DXC comes later, with the first track that needs SM 6.x (A1 or R2).
- [ ] **PIX** for frame capture (winget; the installer likely needs admin). Optional for M0.
- [x] **Git:** repo on a personal account (`github.com/Solile/RenderProjectM0`), personal `user.name` / `user.email` set per repo.
- [ ] **Sponza:** fetched by `tools/fetch_assets.py` in T3 (pins the exact version).

## Known MinGW + D3D12 caveats

Most D3D12 samples assume MSVC. Expect small fixes: `IID_PPV_ARGS` vs `__uuidof`, and PIX symbol resolution is weaker with MinGW's DWARF than with MSVC PDBs. The probe already cleared the basic "can it compile and link and create a device" bar, so these are polish, not blockers.

Found while building T1 (2026-11-15):

- **`PATH` order.** Git for Windows puts its own `mingw64/bin` on `PATH`. If it comes before MSYS2's `ucrt64/bin`, `cc1plus.exe` (which lives in `lib/gcc/...`, not `bin/`) loads Git's `libgmp` / `zlib1` / `libzstd` DLLs and exits **without any error message**; `gcc --version` still looks fine. Build from the MSYS2 UCRT64 shell, or put `ucrt64/bin` first.
- **Agility SDK exports work with GCC.** `__declspec(dllexport)` inside `extern "C"` exports `D3D12SDKVersion` / `D3D12SDKPath` from the exe (check with `objdump -p m0.exe`). Declare `D3D12SDKPath` first and define it outside the block; GCC warns on an initialized `extern` non-const pointer.
- **The debug layer needs Graphics Tools.** The Agility SDK's own `d3d12SDKLayers.dll` refuses to load without the OS feature. The runtime says so only through `OutputDebugString` ("requires the D3D12 SDK Layers for Windows 10, but they are not present on the system"); the API just returns `0x887a002d`.
