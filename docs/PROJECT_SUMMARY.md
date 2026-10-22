# SDL2 游戏架构与 Vulkan Pong 项目经验总结

本文基于 2026-10-19 的本地源码整理，主要覆盖 `PongFromScratch` 和 `E:/projects/github/SDL-proj` 全部 13 个子目录，另保留同级 `sdl-tutorial-01` 作为最小窗口补充材料。SDL 学习已涉及游戏对象、输入、状态机、数据驱动和地图，Vulkan 是当前底层渲染实践重点。本次仅静态核对源码、构建配置与来源说明，未重新构建或运行程序。

逐项目经验、源码依据与完成度见 [SDL-proj 全量项目经验](SDL_PROJECT_EXPERIENCE.md)。本总结提炼 SDL 与 Vulkan 的衔接，并给出后续学习验收标准。

文中的“代码覆盖”表示相应实现存在于学习目录中，不直接证明原创、已运行或已掌握。教程跟练和示例阅读同样具有学习价值；个人能力需要通过解释、独立复现、修改和验证来确认。

## 0. 代码范围与来源归属

| 代码范围 | 已核对的依据 | 总结中的归属与表述 |
|---|---|---|
| `PongFromScratch/src` 与 Shader | 当前渲染器、平台层、游戏原型及学习笔记 | 当前 Vulkan 学习实现；未完成原创性或教程来源核对，使用“学习搭建、实践”，不宣称全部独立原创 |
| `SDL-proj` 的 12 个源码目录（含副本） | 递归清单、应用源码、资源配置与阶段提交 | SDL2 教程实践和架构演进；逐项区分实际接入、未完成模块与重复快照，原教程及个人改动记录待补 |
| `SDL-proj/SDL-demos` | solution、vcxproj、vcpkg 清单 | 多项目构建与依赖组织，无独立应用源码 |
| `SDL-proj` 内 TinyXML/tinystr 与 Base64 | 文件头作者与许可声明 | 第三方辅助代码；总结集成与调用经验，不认领解析器或编码算法实现 |
| `SDL-proj` 图片、精灵图和音频 | 资源文件及配置引用 | 学习素材，来源待补，不计为个人美术或音乐创作 |
| `sdl-tutorial-01/hello.c` | 应用层最小窗口程序，无作者或教程 URL 声明 | 来源待补充的 SDL2 教程练习；不直接标为个人原创，也不能仅凭内容简单就认定为官方原样示例 |
| `sdl-tutorial-01/CMakeLists.txt` 与 `.vscode` | SDL2 查找与链接、MinGW/GDB 配置 | 构建与调试配置的学习材料；源码和配置来源仍可后续补记 |
| `sdl-tutorial-01/vendored/SDL` | 上游 README、LICENSE、测试说明及代表性文件头 | 随目录保存的 SDL 官方库源码及演示/测试代码；不计为个人实现，本次未逐文件核验本地改动 |
| `vulkan-samples/Vulkan` | README 指向 `SaschaWillems/Vulkan`，LICENSE 标注 Sascha Willems | 第三方 Vulkan 示例参考库；其高级功能不计入当前项目成果，不能由目录存在推断已经学习 |

来源依据：[SDL 上游说明](../../sdl-tutorial-01/vendored/SDL/README.md)、[SDL 许可证](../../sdl-tutorial-01/vendored/SDL/LICENSE.txt)、[SDL 测试说明](../../sdl-tutorial-01/vendored/SDL/test/README)、[测试代码声明](../../sdl-tutorial-01/vendored/SDL/test/COPYING)、[Vulkan 示例库说明](../../vulkan-samples/Vulkan/README.md)、[示例库许可证](../../vulkan-samples/Vulkan/LICENSE.md)。这些跨项目链接依赖当前同级目录结构；单独分享本文时应同时注明目录或原始项目来源。

SDL 的 `testvulkan.c`、`testspriteminimal.c` 等带有上游版权声明，属于参考示例；它们的存在不说明个人已完成 SDL/Vulkan 集成、精灵系统、音频或多线程功能。Sascha Willems 示例库的 README 另行链接了 Khronos 官方 Vulkan-Samples，二者应区分。

后续对每个跟练或改造记录“原始来源、阅读/运行/复现状态、自己的改动、验证结果”。来源不明时保留待确认状态，避免把整套依赖源码或示例库纳入个人成果。

## 1. 项目定位与阶段成果

学习内容目前形成两条相互衔接的路径：

- **SDL2 游戏架构练习**：从窗口与纹理绘制，逐步实践 C++ 多态、向量运动、输入管理、状态机、XML 工厂和 TMX 地图；AlienAttack 继续探索射击系统组合，仍处开发阶段。
- **Vulkan Pong**：以经典 Pong 游戏为载体，使用 C++、Win32 API 和 Vulkan，学习底层窗口、输入、资源加载与二维渲染流程，理解画面从 CPU 数据到 GPU 执行、再到窗口显示的过程。

两条路径目前是独立程序。PongFromScratch 直接使用 Win32 创建窗口，没有接入 SDL；已核对的 SDL-proj 应用代码使用 SDL Renderer，没有调用 Vulkan。

当前经验可概括为：**通过 SDL2 阶段练习建立二维游戏架构认识，并在 Vulkan Pong 中搭建显式 GPU 渲染基础，实现纹理、材质、实体变换和键盘输入之间的数据衔接。**

| 方向 | 当前实现 |
|---|---|
| SDL2 绘制与对象 | Renderer、SDL_image 纹理缓存、精灵帧动画、继承多态、LoaderParams、Vector2D |
| SDL2 输入与流程 | 键鼠/摇杆输入练习、状态栈、菜单/暂停/结束、按钮回调、基础 AABB 碰撞 |
| SDL2 数据与场景 | TinyXML 状态配置、类型注册与对象工厂、TMX/Base64/zlib、图块和对象层、视口滚动 |
| 射击原型探索 | AlienAttack 中子弹、音频、碰撞、死亡动画与滚动背景代码；存在未完成接口和编译问题 |
| 渲染基础 | Vulkan 实例、设备、窗口表面、交换链、Render Pass、Framebuffer、图形管线 |
| 资源与数据 | 手动分配 Buffer/Image 内存，纹理上传，UBO/SSBO，描述符与资源缓存 |
| 着色器 | GLSL 编译为 SPIR-V，矩形顶点生成，像素坐标转换，纹理采样与材质颜色 |
| 绘制组织 | 实体转换为 RenderCommand，Push Constants 指定 Transform 索引，索引绘制 |
| 同步 | Fence 等待 GPU 完成，Semaphore 串联获取图像、提交与呈现 |
| 游戏原型 | 创建左右球拍与球，通过 W/S 控制左球拍上下移动 |
| 开发支持 | MSVC 批处理构建、Vulkan 验证层、日志和断言、拖拽图片转 DDS |

Pong 中球的运动、碰撞、计分、回合管理与右球拍控制尚未实现，定位为 **Vulkan 渲染学习项目与 Pong 原型阶段**。SDL 练习已有基础碰撞和状态机经验，不能因 Pong 尚未接入这些功能就将整体经历降为仅学过窗口。

## 2. SDL2 全量练习覆盖与技术收获

### 2.1 逐项目演进，而非仅最小窗口

本轮对 `SDL-proj` 递归枚举了 948 个文件，区分应用、第三方辅助代码、资源配置和构建产物；按内容哈希核对重复源码，并单独比较 AlienAttack 副本。全部项目经验详见 [完整清单与源码依据](SDL_PROJECT_EXPERIENCE.md)。

| 子目录 | 已接触的技术 | 当前完成度边界 |
|---|---|---|
| demo2 | SDL 初始化、窗口、Renderer、清屏与呈现 | 固定延迟显示，非持续游戏循环 |
| breakupHello | init/render/clean 职责拆分与退出事件 | 本地缺少被调用函数定义，草稿未完成 |
| GameSkeleton | Game 循环、SDL_image、纹理缓存、精灵帧动画 | 事件队列和纹理释放待完善 |
| GameObjects | GameObject/Player/Enemy、多态、shared_ptr 容器 | 智能指针未覆盖所有 SDL 资源 |
| SDLGameObjects | 纯虚接口、SDLGameObject、LoaderParams | 已有分层思路，仍依赖 SDL 与 Game 单例 |
| Movement | Vector2D、速度更新、约 60 FPS 帧率上限 | 按更新次数运动，无 delta time/固定时间步 |
| InputHandle | 键盘状态、鼠标位置、摇杆轴/按钮/死区 | 部分输入未接入，存在事件条件错误 |
| GameState | 状态栈、菜单按钮回调、暂停/结束、AABB | 所有权与更新期间状态销毁需检查 |
| DataDriveDesign | TinyXML、Creator 工厂、状态/资源/回调配置 | 异常配置和加载失败处理不完整 |
| TileMaps | TMX、Base64/zlib、图集 GID、图块/对象层、滚动 | 仅格式子集，边界与解压校验需完善 |
| AlienAttack | 射击对象、子弹/碰撞/音频/死亡/背景模块 | 接口迁移与语法问题尚存，非完成的可运行游戏 |
| AlienAttack - 副本 | 较早射击原型快照，接口与地图存在实际差异 | 已单独遍历，不重复算作完成游戏 |
| SDL-demos | 11 个主项目 solution、vcpkg 与 MSVC 配置 | 自身无应用源码；依赖与构建清单需统一 |

这组项目的主要经验是：从绘制单张图片，发展到用对象表达行为、用状态表达流程、用配置描述场景。DataDriveDesign 指数据驱动设计；TinyXML 和 Base64 是集成的辅助实现，不是个人自研解析/编码库。

### 2.2 可迁移到 Vulkan Pong 的经验

| 职责 | SDL-proj 的练习 | 当前 Vulkan Pong 与迁移方向 |
|---|---|---|
| 纹理与动画 | 图片加载、ID 缓存、源/目标矩形与动画帧 | 已有 DDS 上传和资产缓存；可进一步增加 UV 区域与动画数据 |
| 对象和场景 | 继承多态、LoaderParams、工厂与配置 | 当前 Entity/Transform/材质偏数据组织；可对照分析两种设计的取舍 |
| 输入与时间 | 键鼠/摇杆封装、帧耗时与帧率上限 | Win32 输入已接入；双方都需验证状态重置与独立于 FPS 的运动 |
| 游戏流程 | 菜单/游戏/暂停/结束状态栈 | 将状态切换思路迁移到 Pong，并完善销毁与回调时机 |
| 碰撞与玩法 | AABB、敌人反弹；射击碰撞仍在开发 | Pong 需独立实现球/球拍/边界碰撞、计分与重开 |
| 生命周期 | onEnter/onExit、纹理 ID 清理 | 推进 Vulkan 的显式资源销毁与 GPU 完成后的安全释放 |
| 渲染实现 | SDL_RenderCopyEx 与 SDL_RenderPresent | Vulkan 中显式设置内存、布局、描述符、管线、命令与同步 |

因此，下一步不必重复把 SDL 绘制、碰撞、状态机列为从未接触的主题，而应检验能否独立复现，并将其与 Vulkan 绘制解耦。射击模块和地图配置中出现的类型、接口，不直接证明对应行为已完成。

### 2.3 补充 hello 与构建工具链

同级 [hello.c](../../sdl-tutorial-01/hello.c) 是额外的 C 语言最小窗口练习，只覆盖视频初始化、400×300 窗口、事件轮询与退出清理；它不代表 SDL-proj 的全部学习范围。[CMakeLists.txt](../../sdl-tutorial-01/CMakeLists.txt) 使用 MSYS2 MinGW 编译器与 `find_package(SDL2 REQUIRED)`；其 vendored SDL 没有通过该配置自行构建，旧缓存也不能证明当前目录构建成功。

SDL-proj 主要使用 MSVC solution 与 vcpkg；AlienAttack 另有 CMake 和资源复制逻辑，但 CMake 源文件清单尚未包含全部新增模块，vcpkg 清单未声明其音频依赖 SDL2_mixer。应先统一接口、源码列表、依赖和资源路径，再用新构建目录验证。已有 Debug/Release 产物不能替代当前源码的构建/运行证据。

后续可选用 SDL 承担 Vulkan 窗口与输入层，但尚未实现；SDL 不会代替 Vulkan 的资源内存、描述符、管线与同步管理。

## 3. 已实践的 Vulkan 核心技术

### 3.1 初始化与对象关系

已串起以下对象的创建与使用：

```text
VkInstance → Win32 Surface → 物理设备与队列族选择 → VkDevice / VkQueue
    → Swapchain / ImageView → RenderPass / Framebuffer
    → DescriptorSetLayout / PipelineLayout → GraphicsPipeline
    → CommandPool / CommandBuffer → 同步对象 → 每帧绘制
```

对应知识点：

- 区分物理设备与逻辑设备：前者用于查询硬件能力，后者是创建资源和提交工作的接口。
- 区分实例扩展与设备扩展：窗口表面属于实例层，交换链属于设备层。
- 使用“先查询数量，再获取数组”的 Vulkan 枚举模式。
- 理解 Surface、Swapchain、Image、ImageView 的职责，以及 Render Pass 与 Framebuffer 的关系。
- 为交换链图像创建视图和 Framebuffer，在 Render Pass 中清屏并将最终布局设为可呈现布局。

源码入口：[vk_init](../src/renderer/vk_renderer.cpp)。当前请求的 API 版本是 **Vulkan 1.2**；安装的 SDK 版本与实际使用的 API 版本应分别记录。

当前设备选择检查了队列的呈现支持，后续还需补充 `VK_QUEUE_GRAPHICS_BIT` 等能力检查，形成完整的设备适配逻辑。

### 3.2 图形管线与命令录制

已配置三角形列表、光栅化、背面剔除、单采样、颜色写入，以及动态 Viewport/Scissor；将顶点和片段 Shader 与 Pipeline Layout、Render Pass 组合成图形管线。

每帧通过 Command Buffer 录制 Render Pass、绑定管线与索引缓冲、绑定描述符、更新 Push Constants 和发出绘制指令，然后提交到队列。

这部分应掌握的重点是：**创建管线定义 GPU 如何处理数据；录制命令描述本帧执行什么；提交队列才让 GPU 执行。**

源码入口：[vk_init / vk_render](../src/renderer/vk_renderer.cpp)、[初始化辅助函数](../src/renderer/vk_init.cpp)。

### 3.3 Buffer、Image 与显式内存管理

已封装 `Buffer` 与 `Image`，实践以下流程：

```text
创建资源对象 → 查询内存需求 → 选择内存类型 → 分配内存 → 绑定内存
CPU 写入路径：映射可见内存 → memcpy
```

- 使用 `memoryTypeBits` 与内存属性筛选可用内存类型。
- 为纹理选择带有 `DEVICE_LOCAL` 属性的内存。
- 为 Staging Buffer、UBO、SSBO 和索引缓冲选择 `HOST_VISIBLE | HOST_COHERENT` 内存，并保留映射指针供 CPU 写入。
- 区分 `vkBindBufferMemory` 的资源与内存绑定，以及 `vkMapMemory` 的 CPU 地址映射。
- 用容量检查约束缓冲区写入，认识资源大小、分配大小与对齐要求的区别。

需要保留的概念边界：`DEVICE_LOCAL` 描述内存属性，是否能映射仍取决于 `HOST_VISIBLE`；`HOST_COHERENT` 简化缓存刷新，但不会替代 CPU/GPU 访问同步。

源码入口：[vk_util.cpp](../src/renderer/vk_util.cpp)、[vk_types.h](../src/renderer/vk_types.h)。当前非映射 Buffer 的通用复制分支仍为 TODO，尚未建立完整的资源释放流程。

### 3.4 纹理上传、图像布局与访问同步

项目将 DDS 像素数据通过 Staging Buffer 上传到纹理：

```text
DDS / 内置白色像素
    → CPU 写入 Staging Buffer
    → Image: UNDEFINED → TRANSFER_DST_OPTIMAL
    → vkCmdCopyBufferToImage
    → Image: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
    → ImageView + Sampler → 片段着色器采样
```

已实践 `VkImageMemoryBarrier`、Pipeline Stage、Access Mask 和图像子资源范围，并用上传 Fence 等待复制工作完成。

应能解释：**布局转换描述图像接下来的用途，访问掩码与流水线阶段描述前后访问的同步关系。** 后续还需要逐项核验屏障参数及队列族索引设置，巩固同步正确性。

源码入口：[vk_create_image](../src/renderer/vk_renderer.cpp)、[DDS 结构](../src/dds_structs.h)。当前上传路径假设单层 RGBA8 像素，不能视为通用 DDS 加载器。

### 3.5 描述符、UBO、SSBO 与 Push Constants

项目已形成明确的 CPU/GPU 数据接口，所有描述符位于 `set = 0`：

| Binding | 描述符类型 | Shader 阶段 | 数据用途 |
|---|---|---|---|
| 0 | Uniform Buffer | Vertex | `GlobalData`：窗口宽高 |
| 1 | Storage Buffer | Vertex | `Transform[]`：位置、尺寸和材质索引 |
| 2 | Combined Image Sampler | Fragment | 精灵纹理及采样器 |
| 3 | Storage Buffer | Fragment | `MaterialData[]`：材质颜色 |

`PushData.transformIdx` 通过 Push Constants 传递，供顶点着色器定位 Transform 的起始位置。

已实践 Descriptor Set Layout、Descriptor Pool、Descriptor Set 的分配、更新与绑定，并按 `AssetTypeID` 复用描述符和纹理。每套描述符包含一个 UBO、两个 SSBO 和一个纹理采样器，池容量也需按各类型总量计算。

这部分的核心收获是：**能够根据数据规模和更新方式，在 UBO、SSBO 与 Push Constants 之间分配职责，并维护 C++ 端与 Shader 端一致的 binding、类型和布局。**

源码入口：[描述符与管线布局](../src/renderer/vk_renderer.cpp)、[shared_render_types.h](../src/renderer/shared_render_types.h)、[shader.vert](../assets/shaders/shader.vert)、[shader.frag](../assets/shaders/shader.frag)。共享结构声明能减少重复定义，但仍需验证 C++ 与 GLSL 的偏移、对齐和数组步长。

### 3.6 索引绘制与实例数据寻址

当前矩形绘制有三个值得复盘的设计：

1. 顶点着色器根据 Transform 在内部生成四个角点，不依赖传统顶点缓冲。
2. 六个索引 `{0, 1, 2, 2, 3, 0}` 组成两个三角形，通过 `gl_VertexIndex` 选择角点。
3. 通过 `pushData.transformIdx + gl_InstanceIndex` 读取 Transform 数组，并用材质索引读取颜色。

**当前已建立支持实例寻址的数据通路，但尚未实现实例合批。** `vk_render` 为每个实体创建一条 RenderCommand，每次 `vkCmdDrawIndexed` 的 `instanceCount` 为 1。后续可将兼容纹理与管线状态的实体组织为连续 Transform 区间，再用一次绘制提交多个实例。

因此，现阶段的成果适合表述为“掌握索引绘制及实例数据寻址基础”；合批减少 Draw Call 的效果需要实际实现并测量。

### 3.7 GLSL、SPIR-V 与二维图形基础

- 启动时调用 `glslc` 将 GLSL 编译为 SPIR-V，再创建 Shader Module 与图形管线。
- 顶点着色器将像素位置按 `2 × position / screenSize - 1` 转换为裁剪坐标的 x/y；当前 `w = 1`，透视除法后得到对应 NDC 坐标。
- 使用 UV 插值完成纹理映射，使用 `flat` 接收整数材质索引。
- 片段着色器采样纹理，并乘以材质颜色；对 alpha 为 0 的片段执行 `discard`。
- 理解三角形绕序、Viewport 与背面剔除之间的关系。

当前颜色混合关闭，`discard` 只提供透明像素裁剪；半透明混合、投影矩阵、深度缓冲和 3D 相机适合后续专题学习。

源码入口：[vk_shader_util.cpp](../src/renderer/vk_shader_util.cpp)、[shader.vert](../assets/shaders/shader.vert)、[shader.frag](../assets/shaders/shader.frag)。

### 3.8 帧同步、交换链呈现与调试

当前帧流程为：

```text
CPU 等待上一帧 Fence → 重置 Fence → 更新 Transform / Material 数据
    → Acquire 交换链图像 → 重置并录制 Command Buffer
    → Submit（等待 acquireSemaphore，发出 submitSemaphores[imgIdx]，关联 Fence）
    → Present（等待 submitSemaphores[imgIdx]）
```

已实践的关键区别：

- Fence 让 CPU 等待 GPU 完成，保护命令缓冲复用和共享数据写入。
- Semaphore 为获取图像、渲染提交和呈现建立执行依赖。
- 获取图像使用一个 Semaphore，渲染完成 Semaphore 按交换链图像分别保存。
- 图形提交在 `COLOR_ATTACHMENT_OUTPUT` 阶段等待获取图像信号。
- 启用 `VK_LAYER_KHRONOS_validation` 与 Debug Messenger，结合 `VK_CHECK`、日志和断点定位 API 使用问题。

当前仅有一个渲染 Command Buffer 与一个 in-flight Fence，属于单帧在途设计；交换链有多张图像不等于已经实现 CPU/GPU 多帧并行。多帧资源隔离、交换链异常恢复与完整生命周期管理仍需继续学习。

## 4. 同步积累的 C/C++、平台与工程技术

| 技术方向 | 已实践内容 | 可迁移能力 |
|---|---|---|
| C++ 数据组织 | 结构体、枚举、union、位运算、固定容量数组和索引 | 设计清晰的数据结构，理解内存布局与容量边界 |
| SDL 阶段的 C++ 架构 | 继承/纯虚接口、shared_ptr 对象容器、Vector2D 运算符、状态栈和 Creator 工厂 | 理解对象职责、配置与运行时对象的映射；原始指针和 SDL 句柄所有权仍需完善 |
| 场景与资源集成 | SDL_image、TinyXML、Base64/zlib、SDL_mixer 接口草稿 | 追踪配置到资源与对象的加载流程，区分库集成和算法原创 |
| 实体与组件标记 | `Entity` 保存 Transform，`comMask` 标记球和球拍身份 | 用数据区分实体行为，形成组件化思路；当前为简化模型 |
| 资源与材质复用 | 按资产 ID 查找图像/描述符，按资产 ID 与颜色查找材质 | 分离“纹理资源”和“材质参数”，避免重复创建 |
| Win32 平台层 | 窗口注册与创建、回调、非阻塞消息泵、客户区尺寸查询 | 理解事件驱动窗口与持续游戏循环的衔接 |
| 输入抽象 | 将 Win32 按键消息转换为 `KeyID`，保存 `isDown` 与转换计数 | 区分持续按住和按下/释放事件 |
| 二进制资源 | Win32 文件读取、DDS 头部与像素读取、白色默认纹理 | 理解文件数据到渲染资源的转换过程 |
| 资产工具链 | DEBUG 下接收文件拖拽，调用 `texconv` 输出单 mip RGBA8 DDS | 将外部工具接入开发流程；尚未实现自动注册或热重载 |
| 构建与诊断 | MSVC、链接 Vulkan/Win32 库、日志、断言与调试输出 | 区分编译、链接、Shader 编译与运行时错误 |
| SDL 工具链对照 | SDL-proj 的 MSVC solution/vcpkg 与 AlienAttack CMake；hello 的 MinGW/GDB | 理解编译器/调试器搭配、多项目依赖及构建列表一致性；当前可复现性待验证 |

当前 `build.bat` 的编译入口是 `src/platform/win32_platform.cpp`，通过包含其他 `.cpp` 形成单翻译单元；它不是独立编译各模块的构建方式。脚本包含本机 Visual Studio 路径，并依赖 `VULKAN_SDK`、`lib/glslc.exe` 和资产导入时的 `lib/texconv.exe`。

主要源码：[游戏与材质](../src/game/game.cpp)、[平台层](../src/platform/win32_platform.cpp)、[输入状态](../src/input.h)、[输入查询](../src/input.cpp)、[资源加载](../src/assets/assets.cpp)、[构建脚本](../build.bat)。

## 5. 用于学习计划的阶段安排

SDL 已有较完整的阶段练习，建议先复现关键模块并解决所有权、时间步和配置一致性，再将 AABB/状态机经验迁移到 Vulkan Pong。Vulkan 学习主线继续聚焦正确性、生命周期、完整玩法与性能。阶段完成以自己的修改和验证记录为依据。

| 阶段 | 学习与实现任务 | 完成标准 |
|---|---|---|
| 0. 整理与验证 SDL 经验 | 按 13 个目录补教程/个人改动记录；优先独立复现输入、状态机和 XML/TMX；修正所有权与时间步；AlienAttack 先统一接口及构建列表 | 区分练习/依赖/快照；新构建目录可复现选定项目；状态切换、松键、地图边缘和错误配置有验证记录 |
| 1. 巩固正确性 | 核验队列 graphics/present 能力、扩展与交换链参数；核对 Barrier 的 stage/access/layout 与队列族索引；处理 Acquire/Present 返回值 | 能解释每项能力检查及同步参数；正常启动和绘制无验证层错误，不把可恢复状态统一当作断点错误 |
| 2. 完成生命周期与窗口恢复 | 建立 Vulkan 资源销毁路径；释放上传 Fence/临时 Command Buffer、CPU 文件数据和文件句柄；处理 resize、最小化和交换链重建 | 连续缩放、最小化恢复及退出不崩溃；退出前 GPU 工作完成，资源按依赖顺序释放 |
| 3. 迁移 SDL 经验，完成可玩的 Pong | 引入 delta time 或固定时间步，处理输入转换计数；独立复现 AABB 与菜单/暂停状态，实现球运动、边界、碰撞、计分和重开 | 不同帧率下速度基本一致；输入边沿不跨帧残留；完成一局并重开，游戏逻辑与 Vulkan 绘制职责分离 |
| 4. 实现实例合批 | 按兼容纹理/管线状态组织批次；连续存储 Transform，正确设置起始索引和 instanceCount；保留所需绘制顺序 | 同批多个实体由一次 Draw Call 绘制；记录实体数、批次数、Draw Call 数；图像结果与优化前一致 |
| 5. 学习多帧并行与测量 | 为每个在途帧分配 Command Buffer、Fence 和动态数据；明确帧索引与交换链图像索引；使用 RenderDoc 或 GPU 时间戳 | 连续运行无资源覆盖与同步错误；能用抓帧结果或计时数据说明优化效果 |
| 6. 扩展图形与工程能力 | 分别练习 alpha blending、sRGB/UNORM、Mipmap；补充 DDS 格式/长度校验、共享结构布局检查与可复现构建 | 各专题都有最小演示和验证记录；半透明效果正确，非法资产有明确错误，换环境可按文档构建 |

完成上述基础后，可选择投影矩阵与 3D 相机、深度测试、Compute Shader、Dynamic Rendering 或 Synchronization2 作为后续专题。它们属于扩展目标，当前项目尚未覆盖。

可选衔接任务：保留现有 Vulkan 渲染逻辑，用 SDL 创建支持 Vulkan 的窗口、获取所需实例扩展并创建 Surface，再迁移输入与尺寸变化事件。先核对 SDL2 对应 API，确保正常绘制、窗口恢复和清理；完成前仅记为计划，不能借上游 `testvulkan.c` 的存在宣称已实现。

使用官方或第三方示例时，每次只选一个主题，按“读懂数据流 → 运行原例 → 在自己的最小程序复现 → 改一个行为 → 记录验证”的顺序推进。原例运行与个人复现分别记录，示例库里的光追、PBR、Compute 等内容不自动纳入已掌握清单。

## 6. “已经掌握”的自测标准

学习复盘时，优先检查是否能脱离原代码解释并完成小修改：

- [ ] 指出哪些文件是教程练习、哪些是上游库或示例，并说明自己修改了什么；来源不明的项目保留待补充记录。
- [ ] 独立写出 SDL2 的初始化、窗口创建、事件轮询和退出清理，并解释失败路径。
- [ ] 从 SDL_image 加载追踪到纹理缓存、动画源矩形与 Renderer 提交，解释纹理释放时机。
- [ ] 解释 GameObject/SDLGameObject 的职责、shared_ptr 管理范围及原始 SDL 资源的所有权。
- [ ] 说明键盘状态、鼠标事件、摇杆死区的处理，修正输入条件并验证松键和低帧率场景。
- [ ] 独立实现菜单/游戏/暂停状态栈与 AABB，解释回调中切换状态的生命周期风险。
- [ ] 通过工厂新增一种 XML 配置对象，追踪 TMX 的 GID 解码、图集定位与视口偏移，并验证非法配置。
- [ ] 区分帧率上限、delta time 和固定时间步；说明 AlienAttack 哪些模块仍未接通。
- [ ] 对照 SDL Renderer 与 Vulkan 资源、管线、提交及呈现流程，解释两套实现各自承担的职责。
- [ ] 说明 CMake 实际从哪里找到 SDL2，区分 vendored 源码、构建缓存与真正链接的库。
- [ ] 画出从窗口创建到队列呈现的对象关系，并说明主要对象的创建与销毁顺序。
- [ ] 解释 Surface、Swapchain、ImageView、Render Pass 和 Framebuffer 各自解决什么问题。
- [ ] 说明 Buffer/Image 为什么需要单独分配和绑定内存，以及 Map 与 Bind 的区别。
- [ ] 复述纹理上传流程，解释两次布局转换的目的，并核对相应 Stage 与 Access Mask。
- [ ] 从实体 Transform 追踪到 SSBO，再追踪到顶点位置；从材质索引追踪到片段颜色。
- [ ] 给材质增加一个参数，同时修改共享结构、CPU 数据上传和 Shader，核对布局并验证效果。
- [ ] 解释 `gl_VertexIndex`、`gl_InstanceIndex`、索引缓冲与 Push Constants 在当前矩形绘制中的作用。
- [ ] 解释为什么当前每实体一次绘制，以及如何组织真正的实例合批。
- [ ] 说明 Fence、Semaphore 与 Pipeline Barrier 的不同职责，以及哪些资源仍不能被 CPU 提前改写。
- [ ] 读取一条验证层错误，定位对应对象、参数或生命周期问题，并记录修复前后的验证结果。

每完成一个阶段，可记录四项内容：**实现了什么、为什么这样设计、怎样验证、还存在哪些限制**。这能把“跟随代码完成”逐步转化为“能够独立设计和排错”。

## 7. 可复用的学习成果描述

> 通过 SDL2 教程阶段项目，实践纹理缓存与精灵动画、C++ 继承多态、向量运动和输入管理，进一步学习状态栈、菜单交互、AABB 碰撞、XML 对象工厂以及 TMX 地图加载与滚动；在未完成的 AlienAttack 原型中探索子弹、音频、碰撞与死亡状态的组合。结合 Vulkan Pong，重点实践 C++、Win32、Vulkan 图形管线、显式内存、纹理上传与布局转换，通过 UBO、SSBO、描述符集和 Push Constants 连接实体变换与材质，使用索引绘制完成二维精灵渲染。当前已建立实例寻址通路，合批与多帧并行仍为后续目标。教程、依赖与示例单独注明来源，个人贡献通过独立改动和验证记录说明。

对外介绍具体实现时，应补充教程来源及自己的贡献；当前材料支持总结学习范围与代码结构，尚不足以断言所有代码独立原创或所有技术均已熟练掌握。

详细概念与历史复盘可继续参考：[Vulkan 学习笔记](VULKAN_LEARNING.md)、[学习会话记录](SESSION_HISTORY.md)。本总结以当前源码为准，历史笔记中的示例和阶段状态需结合现有实现阅读。
