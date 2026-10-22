# SDL-proj 全量项目经验与学习复盘

本次按 `E:/projects/github/SDL-proj` 全目录递归核对，日期为 2026-10-19。覆盖每个子项目的应用源码、项目配置、XML/TMX 资源描述，以及 TinyXML/Base64 的来源声明；没有把旧版 SDL hello 作为全部 SDL 学习经历。本文与 [SDL/Vulkan 综合总结](PROJECT_SUMMARY.md) 配套，作为学习计划和项目经验描述的依据。

检查方式为源码与配置静态阅读，未重新编译、运行或验证游戏效果。“练习过”表示目录中有对应代码；“掌握”还需要独立解释、修改和验证。根据学习背景，将这些目录归为 SDL2 教程学习与阶段练习；当前缺少原教程链接和完整改动记录，不能将整套代码认定为原创，也不能将所有应用代码认定为官方示例。

## 1. 完整覆盖清单

不含 `.git/.vs`，共枚举 948 个文件：280 份应用 C++ 源码/头文件、30 份第三方辅助源码/头文件、172 份资源与配置、466 份构建生成文件。280 份应用源码按内容哈希去重后为 152 份；相同内容按映射复用阅读结果，不把跨项目复制视为新增技术成果。PNG、音频等二进制资源只核对文件和引用关系，不据此认定美术或音频创作经验。

| 子目录 | 应用源码/头文件 | 第三方辅助源码 | 资源/配置 | 本阶段经验重点 |
|---|---:|---:|---:|---|
| demo2 | 1 | 0 | 4 | SDL 初始化、Renderer 清屏与呈现 |
| breakupHello | 1 | 0 | 3 | 初始化/绘制/清理的函数拆分草稿 |
| GameSkeleton | 5 | 0 | 7 | 游戏循环、纹理管理、精灵帧动画 |
| GameObjects | 11 | 0 | 7 | 继承、多态、shared_ptr 对象集合 |
| SDLGameObjects | 13 | 0 | 8 | 抽象对象接口、SDL 实现、LoaderParams |
| Movement | 14 | 0 | 8 | Vector2D、速度更新、帧率上限 |
| InputHandle | 16 | 0 | 8 | 键盘、鼠标、摇杆输入与死区 |
| GameState | 31 | 0 | 16 | 状态栈、菜单回调、暂停/结束、AABB |
| DataDriveDesign | 36 | 6 | 17 | XML 数据驱动、对象工厂、资源与回调配置 |
| TileMaps | 45 | 8 | 19 | TMX、Base64/zlib、图块与对象层、滚动 |
| AlienAttack | 58 | 8 | 49 | 射击游戏原型、音频、子弹/碰撞/死亡状态探索 |
| AlienAttack - 副本 | 49 | 8 | 19 | 较早阶段快照，单独比较实现差异 |
| SDL-demos | 0 | 0 | 6 | 多项目 solution、vcpkg、构建配置 |

表中第三方文件包括 TinyXML/tinystr、Base64；不含生成目录。资源/配置合计为 171，另有根目录 `.gitignore` 一份，构成上述 172 份。12 个源码目录包括副本；`SDL-demos.sln` 聚合的是 11 个主项目，`SDL-demos` 自身没有应用源码。

## 2. 每个项目的技术经验

### 2.1 demo2：从窗口到实际绘制

使用 `SDL_Init(SDL_INIT_VIDEO)` 初始化，创建 640×480 窗口和 Renderer，通过设置绘制颜色、`SDL_RenderClear`、`SDL_RenderPresent` 显示红色画面；使用 `SDL_GetError` 检查初始化与创建失败。

可总结的经验是认识窗口、渲染上下文、后备缓冲清屏与提交显示的区别。代码以 `SDL_Delay(5000)` 保持画面，没有持续事件循环，窗口/Renderer 的显式销毁也不完整，适合定位为最小渲染练习。

源码：[main.cpp](../../../SDL-proj/demo2/main.cpp)。

### 2.2 breakupHello：函数职责拆分的过渡练习

将主流程拆成 `init`、`render`、`clean`，用运行标志与 `SDL_QUIT` 控制退出，体现从单文件直写流程向函数职责划分的尝试。

当前目录仅有一个应用源文件，未找到这些函数的声明与定义。因此只能总结为结构拆分草稿，不能列为已完成的窗口程序或完整封装成果。

源码：[main.cpp](../../../SDL-proj/breakupHello/main.cpp)。

### 2.3 GameSkeleton：循环、纹理缓存与帧动画

以 `Game` 组织 `init / handleEvents / update / render / clean`，将游戏更新和绘制分开。`TextureManager` 使用字符串 ID 到 `SDL_Texture*` 的映射，完成 `IMG_Load → SDL_CreateTextureFromSurface → SDL_FreeSurface`；`draw/drawFrame` 通过源矩形和目标矩形控制图片区域、位置、尺寸与翻转。

动画通过 `SDL_GetTicks()/100 % 6` 选择六帧精灵图，可用于解释“图片资源、动画帧、屏幕绘制位置”三者的区别。该阶段已涉及 SDL_image、资源复用和基础动画，而非只有窗口创建。

限制是事件处理每帧只取一次事件，纹理释放路径仍需补齐；单例和原始 SDL 句柄不等同于完整 RAII 管理。

源码：[Game.cpp](../../../SDL-proj/GameSkeleton/Game.cpp)、[TextureManager.cpp](../../../SDL-proj/GameSkeleton/TextureManager.cpp)。

### 2.4 GameObjects：用多态统一更新与绘制

定义 `GameObject` 公共接口，由 `Player/Enemy` 派生，分别实现运动和动画行为；`Game` 用 `std::vector<std::shared_ptr<GameObject>>` 保存对象，通过统一接口遍历更新与绘制。

可总结为 C++ 继承、虚函数派发、容器和智能指针的实际练习：增加对象种类时，主循环不必逐类分支。`shared_ptr` 解决的是对象共享所有权，纹理、窗口等 SDL 资源仍需要单独释放，不能据此宣称整个程序已无泄漏。

源码：[Game.cpp](../../../SDL-proj/GameObjects/Game.cpp)、[GameObject.h](../../../SDL-proj/GameObjects/GameObject.h)、[Player.cpp](../../../SDL-proj/GameObjects/Player.cpp)。

### 2.5 SDLGameObjects：抽象接口与 SDL 绘制实现

`GameObject` 提供纯虚接口，`SDLGameObject` 承担公共位置、尺寸、纹理和绘制行为，`Player/Enemy` 再实现具体行为。`LoaderParams` 封装初始化参数，使对象创建不必重复传递一长串位置、大小和纹理参数。

这一阶段的经验是把“游戏对象需要哪些操作”与“用 SDL 如何实现这些操作”分开，复用公共绘制代码。当前实现仍通过 `Game` 获取 SDL Renderer，尚不能称为可替换后端的完整跨平台引擎。

源码：[GameObject.h](../../../SDL-proj/SDLGameObjects/GameObject.h)、[SDLGameObject.cpp](../../../SDL-proj/SDLGameObjects/SDLGameObject.cpp)、[LoaderParams.h](../../../SDL-proj/SDLGameObjects/LoaderParams.h)。

### 2.6 Movement：向量运算与游戏时间基础

实现 `Vector2D` 的坐标访问、长度、归一化和算术/复合赋值运算符，以 `position += velocity` 更新对象位置；练习浮点数、常量引用、值返回与运算符重载。

主循环使用 `SDL_GetTicks` 测量本帧耗时，再延迟剩余时间，目标为约 60 FPS。这里学习的是帧率上限控制；速度仍按每次更新累加，没有乘 `delta time`，也没有固定时间步累积器。后续需验证不同帧率下运动速度的一致性，以及零向量归一化等边界。

源码：[Vector2D.h](../../../SDL-proj/Movement/Vector2D.h)、[SDLGameObject.cpp](../../../SDL-proj/Movement/SDLGameObject.cpp)、[main.cpp](../../../SDL-proj/Movement/main.cpp)。

### 2.7 InputHandle：统一输入状态与多种设备

`InputHandler` 循环处理事件，通过 `SDL_GetKeyboardState` 与 scancode 查询持续按键；保存鼠标位置，枚举并打开摇杆，记录按钮、左右摇杆轴，利用 10000 的死区阈值过滤轴噪声，并包含方向帽事件处理。

可总结为输入事件到可查询状态的转换，以及持续按住、按下/释放、模拟轴和死区的区别。`Player::update` 中实际接入键盘控制；摇杆 `handleInput` 函数已写出，但未在该更新路径调用，鼠标跟随代码在此阶段被注释，不能一概描述为全部设备已完整联调。

代码还存在 `event.type = SDL_JOYBUTTONUP` 的赋值条件、设备标识直接作容器索引等问题；需继续处理每帧速度重置、按钮释放、无设备和设备变化场景。这些是输入模块的后续验证任务。

源码：[InputHandler.cpp](../../../SDL-proj/InputHandle/InputHandler.cpp)、[Player.cpp](../../../SDL-proj/InputHandle/Player.cpp)。

### 2.8 GameState：状态栈、菜单交互与基础碰撞

围绕 `GameState` 组织 `update/render/onEnter/onExit/getStateId`，由 `GameStateMachine` 提供 `push/pop/change`；实现菜单、游戏、暂停和结束状态。暂停用压栈覆盖当前状态，继续游戏弹栈，菜单按钮使用悬停/点击帧及函数指针回调。

`PlayState` 包含矩形 AABB 重叠检测，碰撞后切换 GameOver；还可看到鼠标跟随、键盘覆盖速度和敌人竖直反弹的练习。这说明已经接触游戏流程管理与基础碰撞，后续 Pong 的相应任务应定位为迁移、修正与独立复现。

生命周期仍需完善：对象列表存在原始指针直接 `clear()`，基类析构和回调中立即切换/删除当前状态的路径也需检查。不能将状态栈功能存在等同于资源与状态切换已完全安全。

源码：[GameStateMachine.cpp](../../../SDL-proj/GameState/GameStateMachine.cpp)、[MenuButton.cpp](../../../SDL-proj/GameState/MenuButton.cpp)、[PlayState.cpp](../../../SDL-proj/GameState/PlayState.cpp)、[PauseState.cpp](../../../SDL-proj/GameState/PauseState.cpp)。

### 2.9 DataDriveDesign：XML 驱动的对象创建与状态配置

通过 `GameObjectFactory` 注册类型名与 Creator，将对象创建和 `load(LoaderParams*)` 初始化分开。`StateParser` 使用 TinyXML 读取状态下的 `TEXTURES/OBJECTS`，从 XML 获取位置、尺寸、纹理 ID、帧数、动画速度与回调 ID。

配置包含 MENU、PLAY、PAUSE、GAMEOVER，使用 MenuButton、Player、Enemy、AnimatedGraphic 等对象类型；菜单通过回调表将配置 ID 绑定到实际函数，状态记录已加载纹理 ID 并在退出时释放。

经验重点是“配置 → 解析 → 工厂 → 对象初始化 → 状态运行”的完整数据流，能够通过修改配置改变场景对象，减少硬编码。这里的 Data-Driven Design 指数据驱动设计，不是领域驱动设计。TinyXML 的集成使用也不等于 XML 解析器的原创实现。

空节点、缺失属性、未注册类型、非法回调下标和加载失败回滚仍需验证，避免把当前解析器称为健壮的通用资源系统。

源码：[GameObjectFactory.cpp](../../../SDL-proj/DataDriveDesign/GameObjectFactory.cpp)、[StateParser.cpp](../../../SDL-proj/DataDriveDesign/StateParser.cpp)、[MainMenuState.cpp](../../../SDL-proj/DataDriveDesign/MainMenuState.cpp)、[test.xml](../../../SDL-proj/DataDriveDesign/assets/test.xml)。

### 2.10 TileMaps：TMX 解析、图块坐标与分层场景

`LevelParser` 读取 TMX 地图尺寸、瓦片尺寸、图集和对象组；图层数据经过 Base64 解码与 zlib 解压，从 GID 序列写入二维地图。图集的 first GID、列数、margin/spacing 等信息用于计算纹理源矩形。

`Level` 保存图层，`TileLayer` 负责图块渲染与横向滚动，`ObjectLayer` 负责工厂生成对象的更新/绘制；`PlayState` 实际加载 `assets/map1.tmx`。该资源为 60×15 个瓦片、每块 32×32 像素，图层使用 Base64/zlib 编码，并包含 Player 对象组。

可复盘的重点是世界坐标、瓦片索引、屏幕偏移之间的转换：整除得到起始列，取余得到平滑滚动偏移，只遍历视口附近图块。此处是地图视口偏移练习，尚不构成完整的相机系统。

仍需补充视口越界、空 GID、多个图集边界、解压长度和 XML 失败检查；当前实现覆盖所用地图格式子集，不能写成完整支持所有 TMX 特性。

源码：[LevelParser.cpp](../../../SDL-proj/TileMaps/LevelParser.cpp)、[TileLayer.cpp](../../../SDL-proj/TileMaps/TileLayer.cpp)、[ObjectLayer.cpp](../../../SDL-proj/TileMaps/ObjectLayer.cpp)、[map1.tmx](../../../SDL-proj/TileMaps/assets/map1.tmx)。

### 2.11 AlienAttack：射击游戏系统的组合探索

在状态机、工厂、地图与输入基础上，代码尝试组合以下模块：

| 模块 | 源码中的练习内容 | 当前边界 |
|---|---|---|
| ShooterObject / Player / Enemy | 射击对象继承、移动、死亡/爆炸动画、受伤闪烁与复活逻辑 | 基类与 load 参数接口处于迁移中，头文件与实现未完全一致 |
| Bullet / BulletHandler | 玩家/敌方子弹、创建、列表更新/绘制、离屏删除 | 部分子弹只设置速度未更新位置，清空列表的所有权需完善 |
| CollisionManager | 玩家/敌人/子弹及地图碰撞分支 | 存在未完成表达式、矩形字段混用与声明/定义不一致 |
| SoundManager | SDL_mixer 初始化、音乐/音效加载及播放接口 | 音效加载后未保存到缓存，释放路径不完整 |
| ScrollingBackground | 两组源/目标矩形交替滚动的思路 | 更新计数与循环逻辑未完成，实际接入效果未验证 |
| Level / LevelParser | 碰撞图层和玩家引用、地图对象创建 | 配置中的多种敌人/Boss 并不都有对应实现 |
| Eskeletor | 特定敌人的生命值、运动和爆炸逻辑草稿 | 局部未完成，不单列为完成的敌人系统 |

两张 TMX 地图为 150×15、32 像素瓦片，含碰撞层；map1 还列出 Turret、Glider、ShotGlider、RoofTurret、Level1Boss 等类型。类型出现在资源中，只能说明关卡配置意图，不能当作相应行为已实现的证据。

当前可定位为**射击游戏原型开发中的系统整合练习**。例如 `Enemy.h` 有重复构造函数，`Game.cpp` 引用未提供的 BetweenLevelState，碰撞代码有语法残缺，不能描述为完成并可运行的 AlienAttack 游戏。经验重点是识别模块协作、对象生命周期、接口演进与配置一致性问题。

构建也存在阶段差异：vcxproj 已列入子弹、碰撞和滚动背景源文件，CMake 源列表尚未同步；CMake 有资源复制步骤，但依赖路径仍含本机路径。本次不修复代码，只将这些问题保留为后续学习任务。

源码：[ShooterObject.cpp](../../../SDL-proj/AlienAttack/ShooterObject.cpp)、[Player.cpp](../../../SDL-proj/AlienAttack/Player.cpp)、[BulletHandler.cpp](../../../SDL-proj/AlienAttack/BulletHandler.cpp)、[CollisionManager.cpp](../../../SDL-proj/AlienAttack/CollisionManager.cpp)、[SoundManager.cpp](../../../SDL-proj/AlienAttack/SoundManager.cpp)、[ScrollingBackground.cpp](../../../SDL-proj/AlienAttack/ScrollingBackground.cpp)、[CMakeLists.txt](../../../SDL-proj/AlienAttack/CMakeLists.txt)。

### 2.12 AlienAttack - 副本：保留版本与差异复盘

本目录单独遍历并作内容比较，没有因为名字是“副本”而跳过。副本含 49 份应用源码和 8 份辅助源码；其 57 份源码中，44 份与主目录相同、13 份不同，主目录另增 9 份源码。

主目录新增 Bullet、BulletHandler、Collision/CollisionManager、ScrollingBackground 和 Eskeletor 相关文件；双方差异还涉及 GameObject/ShooterObject 接口、Player/Enemy 声明、地图碰撞信息、音频和向量接口。副本的地图仍是 60×15，与主目录的 150×15 地图不同。

因此它是有独立核对价值的较早阶段快照，可用于追踪“地图/FSM/音频与死亡逻辑骨架 → 更完整射击模块草稿”的演进；两边都存在接口不一致，不能将副本额外算作一个完成的射击游戏。

源码：[副本 ShooterObject.cpp](<../../../SDL-proj/AlienAttack - 副本/ShooterObject.cpp>)、[副本 Game.h](<../../../SDL-proj/AlienAttack - 副本/Game.h>)、[副本地图](<../../../SDL-proj/AlienAttack - 副本/assets/map1.tmx>)。

### 2.13 SDL-demos：多项目组织与依赖管理

`SDL-demos.sln` 聚合上述 11 个非副本主项目；本目录 vcxproj 无应用编译项，不能单列为新游戏。配置涉及 MSVC 的 Debug/Release 与 x86/x64，可用于复盘解决方案、项目、编译配置和库架构的关系。

`vcpkg.json` 声明 SDL2、SDL2_image 和 zlib，并固定 baseline；AlienAttack 引入 SDL2_mixer，但该清单未声明它。经验应描述为依赖集成与构建配置实践，后续需统一依赖声明、源码列表、运行时 DLL 和资产路径，并在新构建目录验证可复现性。

源码：[SDL-demos.sln](../../../SDL-proj/SDL-demos/SDL-demos.sln)、[vcpkg.json](../../../SDL-proj/SDL-demos/vcpkg.json)、[SDL-demos.vcxproj](../../../SDL-proj/SDL-demos/SDL-demos.vcxproj)。

## 3. 教程、依赖与个人贡献的区分

| 范围 | 当前可确认的归属 | 项目经验中合适的说法 |
|---|---|---|
| SDL-proj 应用层 | 教程学习背景，提交记录体现逐步练习；原始教程与逐项个人改动尚未补全 | 跟随教程实践、梳理/修改某模块；用自己的改动和验证补充个人贡献 |
| TinyXML/tinystr | 文件头标明 Lee Thomason 等上游信息及许可 | 集成第三方 XML 解析库，实现游戏配置读取 |
| Base64 | 文件头标明 René Nyffenegger | 使用 Base64 辅助实现与 zlib 读取地图数据，不宣称自研编码算法 |
| SDL2 / SDL_image / SDL_mixer / zlib | 第三方依赖 | 调用和集成库接口，不将依赖库能力计为个人实现 |
| 图片、精灵图、音乐与音效 | 当前缺少完整来源/授权记录 | 使用学习素材，来源待补；不称为个人美术、动画或音乐创作 |
| AlienAttack 副本与各阶段重复文件 | 快照与演进代码 | 用于比较版本变化，不重复累计独立项目成果 |

来源示例：[TinyXML 声明](../../../SDL-proj/DataDriveDesign/tinyxml.h)、[Base64 声明](../../../SDL-proj/TileMaps/base64.h)。另一个 `sdl-tutorial-01/vendored/SDL` 中的官方库与示例，及 `vulkan-samples/Vulkan` 中的 Sascha Willems 示例，归属见综合总结；它们不能替代本目录实际应用源码的经验核对。

## 4. 经验提炼与下一步验收

整体学习路径已经从 SDL 的窗口/绘制，推进到 C++ 对象组织、输入、状态、配置和地图，再尝试组合射击游戏系统。Vulkan Pong 则进一步把纹理绘制背后的资源、管线、命令与同步显式展开。可迁移的是场景与游戏逻辑组织；SDL Renderer 与 Vulkan 的资源生命周期和提交模型仍需分别理解。

| 优先事项 | 建议练习 | 验收依据 |
|---|---|---|
| 记录真实贡献 | 为每个阶段补教程来源、运行状态、自己的修改及关键提交 | 能明确说明原例做了什么、自己新增/修正了什么 |
| 巩固 C++ 所有权 | 选择 GameState 小项目，整理状态、对象和纹理的销毁路径，处理回调中的状态切换 | 多次进入/退出/暂停无悬空访问或持续资源增长，解释析构顺序 |
| 完善输入与时间 | 修正事件赋值条件、输入状态与速度重置，加入 delta time 或固定时间步 | 松键、低帧率、连续输入行为符合预期，移动速度不随 FPS 明显变化 |
| 验证数据驱动与地图 | 增加一种工厂对象，修改 XML/TMX；测试未知类型、无效属性和解压失败 | 新对象可由配置创建，错误有定位信息；地图边缘与空块不越界 |
| 收敛 AlienAttack 原型 | 先统一接口与构建列表，再逐个接通子弹运动、碰撞、音效与死亡状态 | 新目录可构建；每个模块有可观察演示，避免只凭文件存在认定完成 |
| 迁移到 Vulkan Pong | 复用 AABB/FSM 思路，实现球拍/球碰撞、菜单、暂停、计分与重开 | 形成可玩闭环；游戏逻辑不依赖具体绘制 API，记录独立实现过程 |

可用于学习复盘的表述：

> 通过一组 SDL2 教程阶段项目，实践纹理缓存与精灵动画、C++ 继承多态和向量运动，逐步学习键鼠/摇杆输入、状态栈与菜单交互、AABB 碰撞、XML 对象工厂及 TMX 地图加载与滚动；在 AlienAttack 原型中继续探索子弹、音频、碰撞与死亡状态的组合。结合 Vulkan Pong，进一步学习显式 GPU 资源、描述符与同步。当前以教程实践和系统理解为主，AlienAttack 尚未完成；个人贡献通过后续独立修改、复现和运行记录进一步明确。
