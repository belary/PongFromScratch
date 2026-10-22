# Findings

- 项目已拆分为 renderer、game、assets 和 Win32 平台模块；AGENTS.md 的 main.cpp 描述与当前目录不同，需以 build.bat 为准。
- 已有 docs/VULKAN_LEARNING.md 和 docs/SESSION_HISTORY.md，体量较大，适合保留为详细参考。
- 工作区已有 src/game/game.cpp 与 src/platform/win32_platform.cpp 修改；此次仅新增文档。
- 已核对 set 0 的 binding 0/1/2/3 分别为全局 UBO、Transform SSBO、纹理采样器、Material SSBO；PushData 提供实例起始偏移。
- 顶点在 shader 内生成，使用六个索引绘制矩形；详细核对后确认 vk_render 逐实体生成命令，instanceCount 固定为 1，尚未实际合批。
- 游戏创建左右球拍和球，W/S 控制左球拍；尚无球运动、碰撞、计分和 delta time。
- Win32 层包含窗口消息、输入状态、文件读取和 texconv 拖拽导入；build.bat 编译平台入口，采用 include cpp 的单翻译单元方式。
- 请求 Vulkan API 1.2；运行启动时调用 lib/glslc.exe 编译着色器。SDK 版本不能当作已使用的 API 版本。
- 当前单个渲染命令缓冲区与 in-flight Fence；交换链每张图像独立 submit Semaphore，不能称为多帧并行。
- 待完善：队列 graphics 能力检查、交换链重建、退出资源清理、输入每帧状态重置、DDS 格式校验及真正实例合批。

## SDL 补充核对
- SDL 位于同级 ../sdl-tutorial-01，应用层仅 hello.c 与 CMakeLists.txt；basic 目录为空。
- hello.c 为 SDL2 最小窗口练习：WinMain、SDL_Init、创建 400×300 窗口、SDL_PollEvent、SDL_QUIT、销毁窗口和退出；未见渲染器、纹理、音频或游戏逻辑。
- CMake 使用硬编码 MSYS2 MinGW GCC/G++ 路径，find_package(SDL2 REQUIRED) 与 SDL2::SDL2；未 add_subdirectory(vendored/SDL)，不能称为自行构建 SDL 库。
- vendored/SDL README 与 LICENSE 明确为 SDL 上游代码；其中 src/test/示例内容不能计为个人项目成果。
- hello.c 未见作者或教程来源声明，且目录无 Git 历史可供核对，归类为来源待确认的教程练习，不能推断为原创或官方原样示例。
- 广泛搜索误包含 vendored 与大型第三方示例；后续限定应用层、上游 README/LICENSE 及少量代表性示例核对。
- SDL 上游 test/README 与 test/COPYING 声明为 SDL 功能演示/测试；testvulkan.c、testspriteminimal.c 文件头有 Sam Lantinga 版权声明，不能据此推断用户已实现 SDL/Vulkan 集成或精灵系统。
- 同级 vulkan-samples/Vulkan README 指向 SaschaWillems/Vulkan，并另行链接 Khronos 官方 Vulkan-Samples；需区分第三方示例库与官方示例库。

## SDL-proj 全量核对
- 正确目录 E:/projects/github/SDL-proj：13 个项目/配置目录；12 个有 C++ 源码（含副本），SDL-demos 是 solution/依赖配置目录，无应用源码。
- 全量递归清单（不含 .git/.vs）948 项：280 应用源码、30 TinyXML/Base64 辅助源码、172 资产或配置、466 生成文件。应用源码按哈希去重后 152 份，以分块读取核对所有独特内容。
- Git 记录包括 texture manager、polymorphism、shared_ptr、movement、input、FSM、data-driven、tilemap、AlienAttack 的增量学习；不能因此推断整套代码原创。
- demo2 有 renderer 清屏显示；breakupHello 调用 init/render/clean 但本地未见定义；GameSkeleton 已有纹理和六帧动画。
- GameObjects 使用 shared_ptr 多态对象列表；SDLGameObjects 分离抽象接口与 SDL 实现；Movement 引入 Vector2D 运动；InputHandle 覆盖键鼠和摇杆。
- AlienAttack 与副本含未完成接口迁移、重复构造函数、缺失状态类型、碰撞分支语法错误等；只能描述为学习中的射击系统代码，不能称为完整可运行游戏。
- TinyXML 作者 Lee Thomason；Base64 作者 René Nyffenegger，均有明确文件头声明，属于依赖集成。
- 已检查全部非生成 XML/TMX：DataDriveDesign 等使用四类状态配置；TileMaps 与副本地图为 60×15、32 像素瓦片；AlienAttack 两张地图为 150×15，包含碰撞层和更多对象类型，配置存在不代表类型已实现。
- 已检查所有 vcxproj 的 ClCompile 路径，未发现引用缺失文件；SDL-demos 本身无编译项，solution 聚合 11 个主项目。AlienAttack 的 vcxproj 包含子弹/碰撞/滚动背景，但 CMake 源列表遗漏这些模块；vcpkg 清单未声明 SDL2_mixer。
- AlienAttack 副本共 57 份应用/辅助源码，与主目录 44 份相同、13 份不同；主目录另增 9 份源码。应单列快照差异，不计为第二个完成的射击游戏。
