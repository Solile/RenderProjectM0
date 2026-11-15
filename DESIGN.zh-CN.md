# M0 设计文档：打地基（能画、能测、能自检）

> **状态：** 草稿 v0.2，由 Claude 于 2026-11-11 起草，供负责人（你）修改。
> **v0.2（2026-11-11）：** 项目改为三条路线（A：推理引擎，B：游戏引擎，R：渲染，即原来的 M1–M3）。M0 是这几条路线的**共用地基**，范围不变；文中提到后续里程碑的地方改为指向各路线。见 `../tracks/ROADMAP.zh-CN.md`（放在本仓库之外）。
> 标记 **【待定】** 的地方需要你来做决定。§13 的问题已全部回答（golden 机位留待以后）。
> 本文是 `DESIGN.md` 的中文版。两份内容如有出入，以你最后修改的那份为准。

---

## 1. 为什么要有 M0

M0 是 `../tracks/ROADMAP.zh-CN.md` 里三条路线的**共用地基**：路线 A（推理引擎：GPU 算子、量化、精度对齐）、路线 B（游戏引擎：把推理放进实时的一帧里）和路线 R（渲染：把这个渲染器一步步优化快）。它们都是一步步做优化、每一步都用数据证明，在开始任何优化之前，都需要先具备三样东西：

1. **一个真实的渲染对象。** 用标准测试场景（Sponza），负载有意义，结果也能和别人的对比。
2. **一套测量手段。** 每帧、每个 pass 的 CPU 和 GPU 耗时，按严格、可重复的口径采集。
3. **一套"证明没改坏"的手段。** API 校验（debug layer），加上和基准图逐像素比对（golden image）。

M0 只做这三样，别的都不做。**M0 不追求快。** 它测出来的数字就是**基线**，后面的里程碑都拿来跟它比。这套闭环会直接延续下去：GPU 时间戳用来给算子计时（A）和给帧内各 pass 计时（B）；golden 图换成 golden 张量，用于精度对齐（A），也用于检查超分画质（B），还用来证明优化没有改变画面（R）。

| 工作中的做法（TPU 板子） | M0 里的对应 |
|---|---|
| 固定的测试视频，900 帧 | 固定场景 + 确定性的相机路径 |
| 逐层 profile（TIU/GDMA 时间线） | 每个渲染 pass 的 GPU 时间戳 |
| md5 / bit-exact 回归 | golden image 比对 |
| 写清楚指标的测量口径（fps 尾段问题） | 测量方法（§5） |
| 先测基线，再定目标 | M0 的产出 = 基线报告 |

---

## 2. 目标（验收标准）

下面每一项都能回答"是"，M0 才算**完成**：

- [ ] **G1** 程序能打开窗口，用简单光照渲染出 Sponza。自由漫游相机可用（WASD + 鼠标）。
- [ ] **G2** 开启 D3D12 debug layer 跑完一次完整基准测试，结果是 **0 错误、0 警告**。唯一的例外是逐条列出、写明理由的屏蔽项。
- [ ] **G3** 窗口里实时显示 CPU 帧时间、GPU 帧时间，以及每个 pass 的 GPU 时间。
- [ ] **G4** `--benchmark` 沿固定相机路径跑 N 帧，输出逐帧 CSV 和一份元数据 JSON（§5.4）。
- [ ] **G5** 用脚本把 CSV 汇总成统计结果：均值、p50、p95、p99、最大值、标准差，以及多轮之间的波动。
- [ ] **G6** `--capture` 在固定机位截图；比对脚本输出与 golden 图集相比的差异像素数、最大通道差和 PSNR。
- [ ] **G7** 同一个构建在同一台机器上跑两次，**golden image 完全一致**（差异像素为 0）。
- [ ] **G8** 有一份基线报告（`results/m0/baseline_<日期>.md`），**由负责人用自己的话写**。

---

## 3. 不做的事（明确排除在 M0 之外）

阴影 · PBR / 法线贴图 · 后处理 · 抗锯齿 · 透明物体排序 · 多线程录制命令 · job system · 统计信息以外的编辑器或 UI · 热重载 · 资源 cooking · mesh shader · bindless。

§2 没列出的都留到后面的里程碑。M0 最大的风险是范围膨胀。

---

## 4. 平台与约束

| | 主力（公司笔记本） | 可选、后期（家里） |
|---|---|---|
| GPU | Intel 核显，Feature Level 12_2，**UMA** | NVIDIA RTX 4060，独显 |
| 工具链 | MSYS2 GCC 16 + CMake + Ninja | Visual Studio 2022 Community + CMake |
| 性能分析工具 | PIX、Intel GPA | PIX、Nsight Graphics |

- 项目在**公司笔记本**上开发，因为那是唯一能和 Claude 一起工作的机器。编译、运行、profile 都在那台机器的核显上进行（`ENVIRONMENT.md` 里的探测证明核显完整支持 D3D12）。GitHub 用作备份和作品集远端。
- **家里的 RTX 4060 是可选的、后期才用**：M0 之后，在家里拉取仓库、重跑同一套基准，拿到更漂亮的性能数字。M0 用不到。
- **图形 API：** 只用 Direct3D 12（M0 不做多 API 抽象层）。
- **语言：** C++20。
- **构建：** CMake + MinGW/GCC（Ninja 生成器）。§13 的问题 1 已定：**只要 MinGW/GCC** —— Visual Studio Community 的许可证限制大公司使用，所以工作机上避开 MSVC。家里的 4060 以后要用的话，同一套 CMake 工程可以用 MSVC 编译。
- **debug layer：** 通过 D3D12 Agility SDK 提供（一个 NuGet 下载包，放在 exe 旁边，不需要管理员、不装进系统；见 `ENVIRONMENT.md`）。
- **关于 UMA：** 核显和 CPU 共用内存（UMA），和你的 TPU 板子很像。§7.2 里 upload→default 的拷贝在这里可能是多余的；M0 仍然走标准路径，这样在独显（家里的 4060）上也能正确运行，而测量 UMA 这条捷径是路线 A 的一个实验（A1）。

---

## 5. 测量方法

这一节最重要。口径错了，后面所有数字都没有意义。

### 5.1 帧预算（参照线，不是目标）

- **参照：** 1920×1080 @ 60fps → **每帧 16.6ms**。
- M0 只记录自己离这条线有多远，达不到没关系。

### 5.2 基准测试的运行设置

| 设置 | 取值 | 原因 |
|---|---|---|
| 渲染分辨率 | 固定 1920×1080，与窗口大小无关 | 拖动窗口大小不能改变负载 |
| 垂直同步 | 关闭（支持时用 `Present(0, ALLOW_TEARING)`） | 垂直同步会把帧时间锁在 16.6ms，掩盖真实开销 |
| 同时在途的帧数 | 2 | 标准做法，固定下来，避免成为隐藏变量 |
| 预热 | 前 **120 帧** 丢弃 | 排除首次使用时的 shader 编译、资源上传和频率爬升 |
| 测量帧数 | 每轮 **1000** 帧 | 已定（§13） |
| 轮数 | **3～5** 轮，每轮之间冷却 **20～30 秒**（已定，§13） | 核显发热降频是真实存在的，多轮之间的波动能把它暴露出来 |
| 电源 | 插电，Windows "最佳性能"模式 | 电池模式会改变频率。程序会自动记录是否插电 |
| 构建 | Release（`-O2`），计时运行时 **关闭** debug layer | debug layer 有 CPU 开销。校验运行单独进行（G2） |
| 相机 | 确定性路径，按**帧序号**驱动，不按真实时间 | 第 i 帧永远是同一个画面，与程序跑多快无关 |

### 5.3 测量哪些量

| 指标 | 怎么测 | 说明 |
|---|---|---|
| CPU 帧时间 | 相邻两帧开始时刻之间的 `QueryPerformanceCounter` 差值 | 包含等待 GPU 的时间，反映真实帧率 |
| CPU 工作时间 | 帧开始 → `Present` 调用返回，扣掉 fence 等待 | 判断 CPU 是不是瓶颈 |
| GPU 帧时间 | 在本帧命令列表的开头和结尾各打一个时间戳 | — |
| GPU 每 pass 时间 | 每个 pass 前后各打一个时间戳（M0 只有 `Clear` 和 `Opaque`） | 看每个 pass 在 GPU 帧里占多少 |
| fence 等待时间 | CPU 为了等一个可用的帧槽位而阻塞的时间 | 高 = GPU 是瓶颈 |

- **判断瓶颈的经验法则：** fence 等待高 → GPU 瓶颈；GPU 时间明显小于 CPU 帧时间 → CPU 瓶颈。
- GPU 时间戳通过 readback buffer 在 **2 帧之后**读回（因为有 2 帧在途）。
- **已知局限：** 没开开发者模式就用不了 `SetStablePowerState`，GPU 频率会浮动。报告里要如实写出多轮之间的波动，不要掩盖。**已定：开发阶段不锁频。**等到出成果、需要录屏时再开开发者模式锁频，目标是 TDP 下能持续的频率（注意：`SetStablePowerState` 锁到的频率由驱动决定，未必能精确指定）。

### 5.4 输出格式

`results/m0/<时间戳>/run<k>.csv`，每个测量帧一行：

```
frame,cpu_frame_ms,cpu_work_ms,fence_wait_ms,gpu_frame_ms,gpu_clear_ms,gpu_opaque_ms
```

`results/m0/<时间戳>/meta.json`：GPU 名称、驱动版本、分辨率、构建类型、git commit、编译器、是否插电、日期、预热帧数和测量帧数。

**汇总规则（由脚本计算，不在引擎里算）：** 每一列都算均值、p50、p95、p99、最大值、标准差；再算多轮之间的波动（最大均值 − 最小均值）。**核心指标用 p95 GPU 帧时间，不用均值**（已定），因为均值会掩盖卡顿。

---

## 6. golden image 校验

- **机位：** 基准路径上 5 个固定点（帧序号 0、200、400、600、800），暂定。【以后再定：等相机路径做出来再选】
- **格式：** 通过 readback buffer 读回后台缓冲，存成 1920×1080 的 PNG。
- **M0 对确定性的要求：** 不能有基于时间的动画、不能有随机数、不能有时域效果。输出必须完全由（构建版本、场景、机位）决定。
- **比对脚本输出：** 差异像素数、最大通道绝对差、PSNR。
- **通过阈值：**
  - 同一台机器、同一个驱动：**差异像素必须为 0**（G7）。
  - 换驱动或换显卡：容差待定。先用笔记本和 4060 的结果对比，测出实际差异，再定阈值。【以后再定】
- **什么时候允许更新 golden image：** 只有当某次改动*本来就打算*改变画面时才能更新，并且提交说明里要写清楚原因。

---

## 7. 架构

### 7.1 模块划分

```
app/        主循环、命令行解析、模式选择（交互 / 基准测试 / 截图）
platform/   Win32 窗口、输入、高精度计时器
rhi/        很薄的一层 D3D12 封装：设备、队列 + fence、交换链、描述符堆、
            上传辅助、barrier 辅助、时间戳查询堆
asset/      glTF 加载（cgltf）+ 图片解码（stb_image）→ CPU 端的网格和材质数据
render/     各个渲染 pass（Clear、Opaque）、每帧常量
camera/     自由漫游相机 + 确定性路径（关键帧、四元数 slerp）
profile/    CPU 作用域计时、GPU 时间戳收集、CSV/JSON 输出、屏幕上的统计信息
capture/    后台缓冲读回 → PNG
core/       数学库、线性分配器（由负责人手写，见 §9）
tools/      Python 脚本：summarize.py（CSV → 统计）、compare.py（图片比对）
```

"rhi"（Render Hardware Interface，渲染硬件接口）是引擎对图形 API 的一层封装。M0 里故意做得很薄，只是为了不重复写 D3D12 样板代码，不打算做成通用抽象。

### 7.2 数据流（加载时）

```
Sponza.gltf + .bin + 贴图
  → cgltf 解析 → CPU 网格（位置、法线、UV、索引）+ 材质（基础色贴图）
  → 拷进 UPLOAD 堆上的缓冲
  → GPU 拷贝 → DEFAULT 堆上的顶点/索引缓冲和贴图
  → barrier：COPY_DEST → 顶点/索引缓冲状态、像素着色器资源状态
```

> **关于 UMA：** 在这块核显上，CPU 和 GPU 共用内存，upload→default 这一步拷贝可能是多余的。M0 仍然走标准路径，这样在独显（家里的 4060）上也能正确运行。专门测量 UMA 路径，是**路线 A 的一个实验（A1）**。

### 7.3 每帧流程（运行时）

```
1. 等待本帧槽位的 fence（记录 fence_wait_ms）
2. 重置本槽位的命令分配器；重置本槽位的线性分配器
3. 读回 2 帧之前的 GPU 时间戳
4. 录制命令列表：
     timestamp[frame_begin]
     barrier：后台缓冲 PRESENT → RENDER_TARGET            ← 手写（§9）
     timestamp[clear_begin]  清除颜色和深度  timestamp[clear_end]
     timestamp[opaque_begin] 逐个网格绘制，每个网格一次 draw call   timestamp[opaque_end]
     （叠加统计文字）
     barrier：后台缓冲 RENDER_TARGET → PRESENT            ← 手写（§9）
     timestamp[frame_end]；把查询结果解析到 readback buffer
5. ExecuteCommandLists → Present → Signal fence
```

**"每个网格一次 draw call"是故意的。** 这就是朴素基线，由路线 R 来优化（instancing、bindless、间接绘制、GPU 裁剪；见 `../tracks/R-rendering/README.zh-CN.md`）。

### 7.4 着色（M0）

- 顶点：位置、法线、UV。常量：视图投影矩阵，以及每次绘制的模型矩阵。
- 像素：基础色贴图 ×（Lambert 方向光 + 常量环境光）。
- 带 alpha 测试的材质（Sponza 里的植物）：alpha < 0.5 时 `clip()`。**已定：M0 包含植物，用最朴素的方式画**（只做 alpha 测试，不排序、不做 alpha-to-coverage）。
- 程序启动时用 `D3DCompile` 编译 shader（SM 5.1）。第一条需要 SM 6.x 的路线开始时换成 DXC（A1：16 位类型和 INT8 点积；R2：bindless 和 mesh shader）。

### 7.5 第三方代码

| 库 | 用途 | 许可证 |
|---|---|---|
| cgltf | 解析 glTF | MIT |
| stb_image / stb_image_write | 解码贴图、写 PNG | 公有领域 / MIT |
| D3D12 Agility SDK | debug layer、新版 D3D12 运行时 | 微软（可随程序分发） |
| Dear ImGui | 统计信息叠加显示 | MIT（已定：用 ImGui 叠加显示） |

以上都放在 `third_party/` 下，并固定版本。

---

## 8. 命令行接口

```
m0.exe                                    交互模式（自由漫游相机、实时统计）
m0.exe --benchmark --runs 3 --frames 1000 --warmup 120 --out results/m0/
m0.exe --capture --out captures/<名字>/    渲染各个 golden 机位，保存 PNG 后退出
m0.exe --validate                         沿基准路径运行，开启 debug layer 和 GPU-based validation；出现任何消息即失败
python tools/summarize.py results/m0/<时间戳>/
python tools/compare.py golden/ captures/<名字>/
```

---

## 9. 负责人手写的部分

规则：**大部分代码由 AI 写，下面这几部分由负责人不借助 AI 独立完成，写完再请 AI 审查。** 量不大，但真正的理解（以及面试题）都在这里。

| 部分 | M0 里用在哪 | 最小范围 | 怎么检查 |
|---|---|---|---|
| **数学库** | 相机、视图/投影矩阵、路径插值 | `vec3/vec4/mat4`，乘法、转置、求逆、`lookAt`、`perspective`（D3D：左手系，深度 0..1），四元数（轴角构造、相乘、旋转向量、slerp、转矩阵） | 单元测试，和 GLM 或 DirectXMath 的结果对拍 |
| **线性分配器** | 每帧的常量数据，放在 UPLOAD 堆上的缓冲里（每个帧槽位一个） | `alloc(size, align)`、`reset()`、溢出断言。**常量缓冲要求 256 字节对齐** | 溢出和对齐测试；每帧用量统计 |
| **barrier** | 后台缓冲 PRESENT ↔ RENDER_TARGET；加载时 COPY_DEST → 着色器资源 | 每一次状态切换都手写，不用自动推断状态的辅助函数 | debug layer 无报错（G2）。练习：故意删掉一个 barrier，记录 debug layer 报了什么 |

**任务队列**要等到做共用的 job system（`core/jobs/`，R3 或 B2 先用到）时才用得上，暂时作为在家的练习。各路线亲手写的部分，列在各自的 README 里。

**已定：允许 AI 写的临时占位实现**，这样 M0 不会被卡住。规则：

- 占位实现放在单独的、明确标注的文件里（文件头写 `PLACEHOLDER — 将由负责人手写替换`）。
- 单元测试由 AI 先写好；占位实现和负责人的版本必须通过**同一套**测试。测试就是"完成"的定义。
- 负责人写的时候不让 AI 写代码，卡住时按提示阶梯求助：1 概念提示 → 2 指出问题位置 → 3 直接给出那一行（尽量少用，用了就记进 `notes/`）。
- 建议替换顺序：barrier（T1）→ 线性分配器（T3 前后）→ 数学库（T4 之前）。

---

## 10. 任务顺序

测量手段排在场景内容**之前**，这样后面每个任务从第一天起就有数据。

| # | 任务 | 完成标志 |
|---|---|---|
| T1 | CMake 工程、窗口、设备、交换链、清屏，接入 Agility SDK 的 debug layer | 窗口显示清屏颜色；debug layer 生效且无报错 |
| T2 | CPU 计时 + 清屏 pass 上的 GPU 时间戳查询；统计信息显示在窗口里 | 能看到清屏 pass 的 GPU 时间 |
| T3 | glTF 加载 + 上传 + Opaque pass（每个网格一次 draw） | 能看到 Sponza |
| T4 | 自由漫游相机 + 确定性路径 | 路径每次回放完全一样 |
| T5 | `--benchmark` + CSV/JSON + `summarize.py` | G4、G5 |
| T6 | `--capture` + `compare.py` + golden 图集 | G6、G7 |
| T7 | `--validate` 模式；清掉所有 debug layer 消息 | G2 |
| T8 | 跑基线 + 负责人写报告 | G8 |

每个任务的流程：负责人说明目标和验证方法 → AI 实现 → 负责人审查并跑检查 → 负责人在 `notes/` 里用自己的话写 3～5 行笔记。

---

## 11. 目录结构

```
M0/
  README.md  DESIGN.md  ENVIRONMENT.md   （以及 .zh-CN.md 中文版）
  CMakeLists.txt
  src/ app/ platform/ rhi/ asset/ render/ camera/ profile/ capture/ core/
  shaders/
  third_party/       固定版本的依赖
  assets/            Sponza（不提交，改用下载脚本获取）
  tools/             summarize.py、compare.py、fetch_assets.py
  golden/            基准 PNG（提交）
  results/           基准测试输出（不提交，.md 报告除外）；M0 输出到 results/m0/
  notes/             负责人的每个任务笔记；M0 的笔记放在 notes/m0/
```

各路线的计划文档放在本仓库之外的 `../tracks/`。各路线的代码（`src/infer/`、`src/game/` 等）由路线自己添加到本仓库，遵守 `../tracks/ROADMAP.zh-CN.md` §4 的依赖规则。

---

## 12. 风险

| 风险 | 影响 | 应对 |
|---|---|---|
| 范围膨胀（"顺手加个阴影吧……"） | M0 永远做不完 | §3 的排除清单；新想法写进 `later.md` |
| 核显发热降频、频率波动 | 数据噪声大 | 预热、3 轮、冷却、报告波动；可选锁定频率 |
| MinGW + D3D12 磕磕绊绊 | 时间耗在构建问题上 | T1 先把这个解决；探测时已用这套 GCC 编译通过一个 D3D12 程序 |
| Intel 驱动的怪问题 | 遇到不是你造成的 bug | debug layer；看着不对就回家用 4060 再核对一遍 |
| Sponza 有不同版本（Khronos 版和 Intel 版） | 结果不可比 | 在 `fetch_assets.py` 里固定具体的资源版本 |
| 公司政策问题 | 项目没法带回家继续 | 仓库里不放任何公司内容；挪走前先问领导 |

---

## 13. 需要负责人决定的问题

1. ~~M0 必须同时支持 GCC 和 MSVC 编译吗？~~ **已定：只要 MinGW/GCC**（公司笔记本；出于许可证考虑避开 MSVC）。（§4）
2. ~~测量帧数、轮数、冷却时间定多少？~~ **已定：每轮 1000 帧，3～5 轮，冷却 20～30 秒。**（§5.2）
3. ~~核心指标是否用 p95 GPU 帧时间？~~ **已定：用 p95**，能暴露均值掩盖的问题。（§5.4）
4. ~~要不要开开发者模式锁定 GPU 频率？~~ **已定：开发阶段不锁；出成果、需要录屏时再锁到 TDP 下的持续频率。**（§5.3）
5. golden 机位用哪几帧？**以后再定**，等相机路径做出来再选。（§6）
6. ~~M0 要不要处理带 alpha 测试的植物？~~ **已定：包含，用最朴素的方式画。**（§7.4）
7. ~~ImGui 叠加还是标题栏？~~ **已定：ImGui 叠加显示。**（§7.5）
8. ~~是否允许 AI 临时占位实现？~~ **已定：允许，规则见 §9。**（§9）
