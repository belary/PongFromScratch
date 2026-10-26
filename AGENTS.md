# AGENTS.md

## 学习目标与协作方式

本项目用于学习 C++、Win32 和 Vulkan。用户明确要求：重点是学习 Vulkan，由用户亲手实现，助手不要默认代写游戏。

- 用中文交流，结合具体文件和函数解释原理、数据流与调用顺序。
- 助手负责拆解任务、引导阅读、解释概念、给小范围提示、审查修改和协助调试。
- 除非用户明确要求，不直接实现玩法、批量重构或给出整套答案。示例应简短并解释原因。
- 一次推进一个可验证的小步骤，说明学习点和验收方式，根据用户实际结果继续。学习计划不等于自动实施授权。
- 保留用户修改，不擅自重置、提交或推送。未执行的构建和验证须明确标注。

## 仓库与起点

- 上游：https://github.com/belary/PongFromScratch
- 本地：`D:\projects\PongFromScratch`
- 本次签出：`docs_learning_summary`，起点 `28d28ac4c89499c91c5a297822f1f0d1a38f1cb7`。
- 历史会话《拆解本周工作集》：https://chatgpt.com/c/6adf55bd-57e0-83e8-973b-00b636ed832a
- 学习计划：https://github.com/belary/tutorials/blob/master/learning/2026-learning-plan.md
- `main` 是早期入口；`tos_01` 是另一实验，不自动切换或合并。

## 阅读入口与结构

- `docs/VULKAN_LEARNING.md`、`docs/PROJECT_SUMMARY.md`：已有学习记录和项目说明。
- `docs/SESSION_HISTORY.md`、`docs/SDL_PROJECT_EXPERIENCE.md`：历史进度与经验。
- `src/platform/win32_platform.cpp`：实际编译入口，窗口、消息循环、输入和主循环；通过 include 组合多个 `.cpp`。
- `src/input.h`、`src/input.cpp`：输入状态。
- `src/game/game.cpp`：实体、材质与游戏更新。
- `src/renderer/`：Vulkan 初始化、绘制、Shader 和工具。
- `src/renderer/shared_render_types.h`：CPU / Shader 共享数据布局。
- `assets/`：渲染资源。

## 构建与验证

使用 Windows + MSVC + Vulkan SDK。在仓库根目录通过 `cmd /c build.bat` 构建，运行 `./main.exe`。

`build.bat` 当前硬编码 VS 2022 Community 的 `vcvars64.bat` 路径，并依赖 `VULKAN_SDK`；Shader 编译工具路径也需检查。先核实本机配置，不声称签出即能构建。VS Code 调试使用 MSVC 对应的 `cppvsdbg`。

## 近期学习路线与边界

2026-10-26 的历史会话建议：运行基线 → 输入闭环 → 时间步和球拍 → 球运动与碰撞 → 计分、回合复位和重开 → 整体验收。这是学习路线，本次签出没有实现或验证这些功能。

- 玩法状态放在 CPU 侧，不随意修改共享 GPU Transform 布局。
- 重开只重置状态，避免重复创建实体和材质。
- 暂缓 Vulkan 资源生命周期全面整理、交换链重建、多帧并行、SDL3 迁移、实例合批和额外玩法。遇到阻塞先解释原因，再与用户确定学习范围。
