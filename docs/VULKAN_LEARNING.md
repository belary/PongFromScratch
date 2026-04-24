# Vulkan 学习笔记

从零开始构建 Vulkan 渲染器的学习笔记，记录遇到的问题、解决方案和核心概念理解。

> 📁 **文档位置**：本文件是 Vulkan 学习的知识库，包含核心概念、API 说明和常见错误解决方案。
>
> 📚 **相关文档**：
> - [SESSION_HISTORY.md](./SESSION_HISTORY.md) - 学习会话历史记录
> - [src/renderer/vk_renderer.cpp](../src/renderer/vk_renderer.cpp) - 主要实现代码
> - [src/renderer/vk_init.cpp](../src/renderer/vk_init.cpp) - 初始化辅助函数
> - [assets/shaders/](../assets/shaders/) - 着色器源码

## 目录

- [环境配置](#环境配置)
- [头文件包含顺序](#头文件包含顺序)
- [Vulkan 核心概念](#vulkan-核心概念)
  - [物理设备 vs 逻辑设备](#物理设备-vs-逻辑设备)
  - [队列族 (Queue Family)](#队列族-queue-family)
  - [Surface vs Swapchain vs Image](#surface-vs-swapchain-vs-image)
  - [Vulkan 的两级扩展系统](#vulkan的两级扩展系统)
- [初始化流程](#初始化流程)
- [常见错误与解决](#常见错误与解决)

---

## 环境配置

### 开发环境

- **编译器**: MSVC (Microsoft Visual C++) 19.44
- **平台**: Windows 10 Pro
- **Vulkan SDK**: 1.4.341.1
- **IDE**: Visual Studio Code
- **构建系统**: 批处理脚本 (build.bat)

### MinGW vs MSVC

| 特性 | MinGW | MSVC (本项目) |
|------|-------|---------------|
| 编译器 | GCC | cl.exe |
| 调试器 | GDB | Windows 调试器 |
| 调试类型 | `cppdbg` | `cppvsdbg` |
| 开源 | ✅ 是 | ❌ 否 |

---

## 头文件包含顺序

### 关键规则

在 Windows 平台上使用 Vulkan，**必须先包含 `<windows.h>`**，再包含 `<vulkan/vulkan_win32.h>`。

### 为什么顺序重要？

C++ 编译器遵循"所见即所得"原则：从上到下解析代码，只能识别之前已定义/包含的内容。

#### 依赖链

```
vulkan_win32.h  →  使用了 HWND, HANDLE, HINSTANCE 等类型
                       ↓
                  这些类型定义在 <windows.h> 中
```

#### 错误示例

```cpp
#include <vulkan/vulkan_win32.h>  // ❌ 编译器：HWND 是什么？报错！
#include <windows.h>
```

#### 正确示例

```cpp
#include <windows.h>              // ← 先定义 Windows 类型
#include <vulkan/vulkan_win32.h>  // ✅ 编译器：继续解析
```

### 格式化工具问题

**clang-format** 会自动按字母顺序重排 `#include`，可能破坏依赖顺序。

#### 推荐解决方案：禁用 include 排序

在 `.clang-format` 中设置：

```yaml
SortIncludes: false
```

**原因**：某些头文件有严格的依赖顺序，手动控制更安全。

---

## Vulkan 核心概念

### 物理设备 vs 逻辑设备

#### Physical Device（物理设备）

- 代表系统中的 GPU 硬件
- `VkPhysicalDevice` 是 GPU 的**句柄**，不是创建对象
- 通过 `vkEnumeratePhysicalDevices` 查询
- 一个系统可能有多个 GPU（集显 + 独显）
- **不能直接使用**，只用于查询信息

#### Logical Device（逻辑设备）

- 应用程序与 GPU 交互的接口
- 通过 `vkCreateDevice` 创建
- 指定要使用的队列族和扩展
- 类似于"打开设备"，获得操作权限

#### 为什么需要逻辑设备？

1. **多应用共享 GPU**：同一个 GPU 可以被多个应用使用
2. **隔离性**：每个应用可以创建不同的逻辑设备配置
3. **资源管理**：逻辑设备拥有和管理资源（队列、内存、图像等）

#### 类比

```
Physical Device = 显卡硬件
Logical Device  = 应用程序"打开"显卡的句柄（类似文件句柄）
```

---

### 队列族 (Queue Family)

#### 概念

GPU 中不同类型的工作队列分组。

#### 常见类型

- **Graphics**: 渲染队列（绘图、着色）
- **Compute**: 计算队列（物理模拟、AI）
- **Transfer**: 数据传输队列（内存拷贝）
- **Sparse**: 稀疏内存管理

#### 特点

- 同一族内的队列共享资源
- 不同族之间相对独立
- 一个队列族可以创建多个队列

#### 为什么需要检查 Surface 支持？

不是所有队列都能向窗口渲染：
- ✅ 渲染队列通常支持
- ⚠️ 某些集显可能不支持所有功能
- ❌ 计算专用队列不能渲染画面

#### 代码示例

```cpp
VkBool32 surfaceSupport = VK_FALSE;
vkGetPhysicalDeviceSurfaceSupportKHR(gpu, queueFamilyIndex, surface, &surfaceSupport);

if (surfaceSupport) {
    // 找到可以渲染到窗口的队列族
    graphicsIdx = queueFamilyIndex;
    selectedGpu = gpu;
}
```

---

### Surface vs Swapchain vs Image

#### 三者关系

```
┌─────────────────────────────────────┐
│  Surface = 墙/画布底板               │  ← "可以显示图像的地方"
│  ┌─────────┐                        │
│  │ Image 1 │ ← Swapchain 管理的图像  │
│  └─────────┘                        │
│  ┌─────────┐                        │
│  │ Image 2 │                        │
│  └─────────┘                        │
└─────────────────────────────────────┘
```

#### VkSurfaceKHR（窗口表面）

- **不是图像**，而是"显示目标"的抽象
- 代表：Windows 窗口 / Linux 窗口 / Android 屏幕
- 提供**跨平台抽象**

```cpp
// Windows
vkCreateWin32SurfaceKHR(instance, &win32Info, ..., &surface);

// Linux
vkCreateXcbSurfaceKHR(instance, &xcbInfo, ..., &surface);
```

#### VkSwapchainKHR（交换链）

- 管理一系列图像缓冲区
- 与显示同步（避免撕裂）
- 工作流程：渲染 A → 显示 A → 渲染 B → 显示 B

#### VkImage（图像）

- 实际的图像资源
- GPU 内存中的像素数据
- 归逻辑设备所有

---

### 命令池与命令缓冲

#### 什么是命令池？

命令池（Command Pool）是用于分配和管理命令缓冲区的内存管理器。

#### 类比理解

```
Command Pool  = 纸张仓库（管理一堆"画纸"）
     ↓ 分配
Command Buffer = 画纸（在上面记录绘制命令）
     ↓ 提交给
Queue         = 画家（执行命令缓冲区上的命令）
```

#### 为什么需要命令池？

1. **内存管理效率**
   - 命令缓冲区从池中分配，避免频繁的内存分配/释放
   - 批量管理命令缓冲区的生命周期

2. **队列族绑定**
   - 每个命令池必须绑定到特定的队列族
   - 从该池分配的命令缓冲区只能提交到对应的队列

3. **性能优化**
   - 驱动可以针对特定队列族优化命令缓冲区分配
   - 重置和回收更高效

#### 命令池与队列族的关系

```
逻辑设备
    ↓
队列族 0 (Graphics)
    ├─ Queue 0
    ├─ Queue 1
    └─ Command Pool (绑定到此队列族)
        ├─ Command Buffer 0 → 可提交到 Queue 0 或 1
        └─ Command Buffer 1 → 可提交到 Queue 0 或 1
```

**重要限制**：从图形队列族的命令池分配的缓冲区**只能**提交到图形队列，不能提交到计算或传输队列。

#### 命令池的创建

```cpp
// 配置命令池创建信息
VkCommandPoolCreateInfo poolInfo = {};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

// 绑定到队列族（关键！）
poolInfo.queueFamilyIndex = graphicsIdx;  // 绑定到图形队列族

// 可选标志
// - VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 短生命周期，频繁重录
// - VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: 允许单独重置缓冲区
poolInfo.flags = 0;  // 使用默认行为

// 创建命令池
vkCreateCommandPool(device, &poolInfo, 0, &commandPool);
```

#### 命令池的生命周期

```
1. 创建命令池（绑定到队列族）
   vkCreateCommandPool(...)
   ↓
2. 从池中分配命令缓冲区
   vkAllocateCommandBuffers(...)
   ↓
3. 记录命令到缓冲区
   vkBeginCommandBuffer(...) → 记录命令 → vkEndCommandBuffer(...)
   ↓
4. 提交缓冲区到队列执行
   vkQueueSubmit(queue, commandBuffer, ...)
   ↓
5. 执行完成后，缓冲区可重用
   ↓
6. 销毁命令池（自动释放所有缓冲区）
   vkDestroyCommandPool(...)
```

#### 常见标志说明

| 标志 | 用途 | 使用场景 |
|------|------|----------|
| 无标志 | 默认行为 | 通用命令记录 |
| `TRANSIENT_BIT` | 短生命周期 | 每帧重新录制的命令 |
| `RESET_COMMAND_BUFFER_BIT` | 允许单独重置 | 需要频繁重录单个缓冲区 |

#### 命令池 vs 命令缓冲区

| 特性 | 命令池 | 命令缓冲区 |
|------|--------|-----------|
| **作用** | 管理和分配缓冲区 | 记录 GPU 命令 |
| **生命周期** | 通常应用程序全程存在 | 可以重录和重用 |
| **数量** | 每个队列族一个或多个 | 从池中分配，可以有多个 |
| **绑定** | 绑定到队列族 | 从池分配，隐式继承绑定 |

---

### 信号量与同步

#### 什么是信号量？

信号量（Semaphore）是用于 **GPU 内部同步**的原语，用于协调不同操作之间的执行顺序。

#### 类比理解

```
信号量 = GPU 的接力棒传递机制
```

```
餐厅上菜流程：
  厨师做菜 → 服务员上菜 → 顾客用餐
     ↓          ↓          ↓
  渲染画面   显示图像   开始下一帧
     ↓          ↓
  submit    present
  Semaphore   操作
```

**信号量 = "我可以继续了吗？"的确认机制**

#### 为什么需要信号量？

Vulkan 是异步架构，GPU 操作需要时间完成：

```
1. 获取交换链图像（从显示队列）
   ↓ 需要等待
2. 渲染到图像（GPU 执行绘制命令）
   ↓ 需要等待
3. 显示图像（提交给显示队列）
```

**没有信号量的问题**：
- 可能在图像还在使用时就尝试获取
- 可能在渲染未完成时就尝试显示
- 导致画面撕裂、闪烁或崩溃

#### 双信号量模式（标准渲染循环）

##### acquireSemaphore（获取信号量）

- **作用**：等待交换链图像可用
- **流程**：`vkAcquireNextImageKHR(waitFor=acquireSemaphore)` → 图像准备好 → 信号量触发
- **确保**：不会获取到还在显示的图像

##### submitSemaphore（提交信号量）

- **作用**：等待渲染完成
- **流程**：渲染完成 → 信号量触发 → `vkQueueSubmit(signal=submitSemaphore)` → 显示
- **确保**：渲染完成后才显示图像

#### 完整的渲染同步链

```
┌─────────────────────────────────────────────────────────┐
│ 1. vkAcquireNextImageKHR(acquireSemaphore)              │
│    ↓ 等待：上一帧显示完成，图像可用                       │
│    ↓ 触发：acquireSemaphore                               │
│                                                          │
│ 2. vkQueueSubmit(                                        │
│      waitFor=acquireSemaphore,   // 等待图像获取        │
│      signal=submitSemaphore      // 完成后触发          │
│    )                                                      │
│    ↓ 执行：GPU 渲染到图像                                │
│    ↓ 触发：submitSemaphore                                │
│                                                          │
│ 3. vkQueuePresentKHR(waitFor=submitSemaphore)           │
│    ↓ 等待：渲染完成                                       │
│    ↓ 触发：下一帧的 acquireSemaphore                      │
│    ↓ 显示：图像显示到屏幕                                 │
└─────────────────────────────────────────────────────────┘
```

#### 渲染循环代码示例

```cpp
while (running) {
    // 1. 获取图像（等待上一帧完成）
    uint32_t imageIndex;
    vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,              // 超时时间（无限等待）
        acquireSemaphore,        // 等待这个信号量
        VK_NULL_HANDLE,          // 无围栏（fence）
        &imageIndex              // 返回获取的图像索引
    );

    // 2. 提交渲染命令
    VkSubmitInfo submitInfo = {};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &acquireSemaphore;  // 等待图像获取完成
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &submitSemaphore; // 完成后触发这个信号量
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

    // 3. 显示图像（等待渲染完成）
    VkPresentInfoKHR presentInfo = {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &submitSemaphore;  // 等待渲染完成
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(queue, &presentInfo);
}
```

#### 信号量的类型

##### 二进制信号量（Binary Semaphore）

- **状态**：只有两种状态 - 触发或未触发
- **用途**：标准渲染同步
- **创建**：`VkSemaphoreCreateInfo`（flags = 0）

```cpp
VkSemaphoreCreateInfo semaInfo = {};
semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
semaInfo.flags = 0;  // 二进制信号量

vkCreateSemaphore(device, &semaInfo, 0, &semaphore);
```

##### 时间线信号量（Timeline Semaphore）

- **状态**：递增的整数值
- **用途**：更精细的多阶段依赖控制
- **创建**：需要指定类型（Vulkan 1.2+）

```cpp
VkSemaphoreTypeCreateInfo typeInfo = {};
typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
typeInfo.initialValue = 0;

VkSemaphoreCreateInfo semaInfo = {};
semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
semaInfo.pNext = &typeInfo;

vkCreateSemaphore(device, &semaInfo, 0, &semaphore);
```

#### 双缓冲 vs 三缓冲

| 配置 | 信号量对 | 命令缓冲区 | 优点 | 缺点 |
|------|----------|-----------|------|------|
| **双缓冲** | 2 对 | 2 个 | 延迟低 | 帧率受显示刷新率限制 |
| **三缓冲** | 3 对 | 3 个 | 帧率更高，GPU 更少空闲 | 延迟稍高 |

```cpp
// 双缓冲示例
VkSemaphore acquireSemaphores[2];
VkSemaphore submitSemaphores[2];
VkCommandBuffer commandBuffers[2];

uint32_t frameIndex = 0;
while (running) {
    // 使用 frameIndex % 2 来选择信号量对
    uint32_t current = frameIndex % 2;

    vkAcquireNextImageKHR(..., acquireSemaphores[current], ...);
    vkQueueSubmit(..., acquireSemaphores[current], submitSemaphores[current], ...);
    vkQueuePresentKHR(..., submitSemaphores[current], ...);

    frameIndex++;
}
```

#### 信号量 vs 围栏（Fence）

| 特性 | 信号量（Semaphore） | 围栏（Fence） |
|------|---------------------|---------------|
| **作用范围** | GPU 内部 | CPU ↔ GPU |
| **用途** | GPU 操作间同步 | CPU 等待 GPU 完成 |
| **状态重置** | 自动（二进制） | 需要手动重置 |
| **典型场景** | 渲染管线同步 | 资源销毁、超时控制 |

```cpp
// 信号量：GPU 内部同步
vkQueueSubmit(
    waitFor=semaphore1,   // GPU 等 GPU
    signal=semaphore2     // GPU 通知 GPU
);

// 围栏：CPU 等待 GPU
vkQueueSubmit(
    ...,
    fence  // CPU 可以等待这个 fence
);
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);  // CPU 等待
```

#### 关键要点

1. **GPU 同步**：信号量是 GPU 内部的，不是 CPU
2. **异步执行**：允许 GPU 并行处理多个操作
3. **避免撕裂**：确保图像完整后才显示
4. **双信号量**：分别同步"获取"和"渲染"两个阶段
5. **自动管理**：二进制信号量状态自动切换

#### 信号量数量设计：为什么 acquireSemaphore 只要 1 个，submitSemaphores 需要 N 个？

这是很多初学者的困惑点。关键在于理解 GPU 的流水线并行渲染。

##### 数量对比

```cpp
VkSemaphore acquireSemaphore;        // 1 个（所有帧共享）
VkSemaphore submitSemaphores[5];     // 5 个（每个交换链图像一个）
```

##### 为什么 acquireSemaphore 只需 1 个？

```
每帧渲染流程：
vkAcquireNextImageKHR(..., acquireSemaphore, ...);  // ← 阻塞等待
vkWaitForFences(...);  // ← 等待上一帧完成
// ... 渲染命令 ...
vkQueueSubmit(..., &acquireSemaphore, ...);  // ← 等待获取完成
```

**原因：**
1. `vkAcquireNextImageKHR` 会**阻塞**直到有图像可用
2. CPU 在这里等待，不会有并发竞争
3. 获取完成后，acquireSemaphore 可以安全复用
4. Fence 确保了 CPU 不会超前 GPU 太多

**类比：**
```
acquireSemaphore = 餐厅叫号机
  - 你等待叫号（阻塞）
  - 叫到你的号了（信号量触发）
  - 你进去用餐
  - 叫号机可以复用给下一个人
```

##### 为什么 submitSemaphores 需要 N 个（N = 交换链图像数量）？

```
GPU 可以同时处理多帧（流水线并行）：

帧 N:   获取 image[0] → 渲染中 → 提交 submitSemaphores[0] ✗ 还没完成
帧 N+1: 获取 image[1] → 渲染中 → 提交 submitSemaphores[1] ✗ 还没完成
帧 N+2: 获取 image[2] → 渲染中 → 提交 submitSemaphores[2] ✗ 还没完成
```

**如果只用 1 个 submitSemaphore：**
```
帧 N:   使用 submitSemaphore → 等待触发...
帧 N+1: 使用同一个 submitSemaphore → ❌ 冲突！还在等待中
```

**每个图像一个 submitSemaphore：**
```
帧 N:   使用 submitSemaphores[0] → 独立等待
帧 N+1: 使用 submitSemaphores[1] → 独立等待
帧 N+2: 使用 submitSemaphores[2] → 独立等待
```

**原因：**
1. GPU 流水线并行：多帧同时在不同阶段处理
2. 每个图像独立状态：需要追踪各自的渲染完成状态
3. 避免竞争：不同帧不会相互干扰

##### GPU 流水线时间线

```
时间轴（ms）：
t=0:   帧 N   vkAcquire(image[0]) → 开始渲染
t=5:   帧 N+1 vkAcquire(image[1]) → 开始渲染
t=10:  帧 N+2 vkAcquire(image[2]) → 开始渲染
t=15:  帧 N   渲染完成 → 触发 submitSemaphores[0] → 呈现
t=20:  帧 N+1 渲染完成 → 触发 submitSemaphores[1] → 呈现
```

##### 状态追踪表

| 图像索引 | 当前状态 | 使用的 submitSemaphore |
|----------|----------|------------------------|
| image[0] | 正在渲染 | submitSemaphores[0] |
| image[1] | 等待呈现 | submitSemaphores[1] |
| image[2] | 正在呈现 | submitSemaphores[2] |
| image[3] | 可用（空闲） | submitSemaphores[3] |
| image[4] | 可用（空闲） | submitSemaphores[4] |

##### 核心区别总结

| 特性 | acquireSemaphore | submitSemaphores |
|------|------------------|------------------|
| **数量** | 1 个 | N 个（N = 交换链图像数） |
| **原因** | 获取操作阻塞，CPU 串行执行 | GPU 并行渲染，每帧独立追踪 |
| **重用方式** | 立即复用 | 每个 GPU 帧独立使用 |
| **竞争风险** | 无（CPU 串行） | 有（GPU 并行） |

##### 驱动分配 imgIdx 机制（关键理解）

**核心理解**：虽然 GPU 并行渲染多帧，但 CPU 通过驱动返回的 imgIdx 来选择对应的信号量，确保不会冲突。

```
CPU 的角色：根据 imgIdx 选择信号量
GPU 的角色：并行渲染多帧，每帧独立追踪
驱动的角色：分配可用的 imgIdx，避免冲突
```

##### 完整的调度流程

```
1. CPU 调用 vkAcquireNextImageKHR
   ↓
2. 驱动检查哪个图像可用（不在显示、不在渲染）
   ↓
3. 驱动返回 imgIdx（比如 2）
   ↓
4. CPU 使用 submitSemaphores[imgIdx]（即 submitSemaphores[2]）
   ↓
5. GPU 渲染完成后，触发 submitSemaphores[2]
   ↓
6. CPU 呈现图像，image[2] 重新变为可用
```

##### 驱动如何避免冲突？

**驱动维护图像状态表：**

```
时刻 T0 的状态：

| 图像 | 状态 | 信号量状态 |
|------|------|-----------|
| image[0] | 占用（正在渲染） | submitSemaphores[0] 正在使用 |
| image[1] | 占用（正在显示） | submitSemaphores[1] 正在使用 |
| image[2] | 可用 | submitSemaphores[2] 空闲 |
| image[3] | 可用 | submitSemaphores[3] 空闲 |
| image[4] | 占用（刚刚提交） | submitSemaphores[4] 正在使用 |

CPU: vkAcquireNextImageKHR(..., &imgIdx)
   ↓
驱动检查：只返回可用的图像
   ↓
返回 imgIdx = 2 或 3（驱动选择）
   ↓
CPU 使用 submitSemaphores[2]（如果 imgIdx=2）
```

##### 关键机制：确保 imgIdx 对应的信号量总是可用的

```
为什么不会冲突？

1. 驱动只返回"可用"的图像索引
2. 可用的图像 = 不在显示 + 不在渲染
3. 如果图像在渲染，对应的 submitSemaphores[i] 必然正在使用
4. 因此驱动不会返回这个 imgIdx
5. CPU 使用 submitSemaphores[imgIdx] 时，该信号量必然是空闲的
```

##### 时间线示例

```
时间线（GPU 并行 + 驱动调度）：

t=0ms CPU: vkAcquireNextImageKHR → 驱动返回 imgIdx=0
       CPU: vkQueueSubmit(..., submitSemaphores[0])
       GPU: 开始渲染 image[0]
       驱动: 标记 image[0] 为"占用"

t=16ms CPU: vkAcquireNextImageKHR → 驱动返回 imgIdx=1
       CPU: vkQueueSubmit(..., submitSemaphores[1])
       GPU: 同时渲染 image[0] 和 image[1]
       驱动: 标记 image[1] 为"占用"

t=32ms CPU: vkAcquireNextImageKHR → 驱动返回 imgIdx=2
       CPU: vkQueueSubmit(..., submitSemaphores[2])
       GPU: 同时渲染 image[0], image[1], image[2]
       驱动: 标记 image[2] 为"占用"

t=48ms GPU: image[0] 渲染完成 → 触发 submitSemaphores[0]
       GPU: 呈现 image[0]
       驱动: 标记 image[0] 为"可用" ← 下一帧可以返回这个 imgIdx

t=64ms GPU: image[1] 渲染完成 → 触发 submitSemaphores[1]
       GPU: 呈现 image[1]
       驱动: 标记 image[1] 为"可用"
```

##### 代码中的关键点

```cpp
// 每帧渲染流程

// 1. 驱动分配 imgIdx（CPU 不能指定）
uint32_t imgIdx;
vkAcquireNextImageKHR(..., &imgIdx);
//                         ^^^^^^
//                         驱动选择一个可用的图像索引

// 2. CPU 根据 imgIdx 选择对应的信号量
VkSubmitInfo submitInfo = {};
submitInfo.signalSemaphoreCount = 1;
submitInfo.pSignalSemaphores = &submitSemaphores[imgIdx];
//                                        ^^^^^^^
//                                        根据驱动返回的索引选择
//                                        这个信号量必然是可用的

// 3. GPU 渲染完成后，触发 submitSemaphores[imgIdx]
vkQueueSubmit(..., fence);
//          ^^^^
//          GPU 完成后会触发这个 fence，CPU 可以等待

// 4. 呈现时也使用同一个 imgIdx
vkQueuePresentKHR(..., &submitSemaphores[imgIdx]);
//                              ^^^^^^^
//                              等待这个图像的渲染完成
```

##### 显示顺序机制（重要理解）

**核心理解**：虽然 CPU 提交和 GPU 渲染的顺序可能是乱的，但显示顺序必须严格。

```
CPU 提交顺序（可能乱序）：
帧 A: submitSemaphores[2] → image[2]
帧 B: submitSemaphores[0] → image[0]
帧 C: submitSemaphores[1] → image[1]

GPU 渲染完成顺序（可能乱序）：
submitSemaphores[0] 先完成
submitSemaphores[2] 其次完成  
submitSemaphores[1] 最后完成

但显示顺序（必须严格）：
image[0] → image[1] → image[2] → image[3] → image[4] → image[0] → ...
```

##### 为什么显示顺序不能乱？

```
如果乱序显示：
显示器: image[2] → image[0] → image[1]
结果: 画面跳变、闪烁、时间旅行效果 ❌

正确顺序显示：
显示器: image[0] → image[1] → image[2]
结果: 流畅的动画 ✓
```

##### 交换链的显示循环

```
交换链维护严格的显示顺序：

┌─────────────────────────────────────────────────────┐
│ 显示循环（60 FPS = 每 16.6ms 切换一次）             │
├─────────────────────────────────────────────────────┤
│ T=0ms:    显示 image[0]                             │
│ T=16.6ms: 显示 image[1]                             │
│ T=33.3ms: 显示 image[2]                             │
│ T=50ms:   显示 image[3]                             │
│ T=66.6ms: 显示 image[4]                             │
│ T=83.3ms: 显示 image[0]（循环）                     │
└─────────────────────────────────────────────────────┘
```

##### vkQueuePresentKHR 如何确保顺序？

**关键机制：交换链内部维护显示队列**

```
交换链内部状态：
┌───────────────────────────────────────────────┐
│ 显示队列: [0, 1, 2, 3, 4]                   │
│           ↑ 当前显示到 image[1]              │
│                                              │
│ 下一帧必须显示 image[2]，不能跳             │
└───────────────────────────────────────────────┘

vkQueuePresentKHR 内部流程：
1. 等待 submitSemaphores[imgIdx] 触发（渲染完成）
2. 检查 imgIdx 是否在显示队列的当前位置
3. 如果轮到了，立即显示
4. 如果没轮到，等待（图像在队列中排队）
```

##### 完整的时间线示例

```
时刻 | CPU 提交 | GPU 渲染 | 显示器 | 说明
-----|----------|----------|--------|------
t=0  | 帧 A: submitSemaphores[2] | 开始渲染 image[2] | 显示 image[0] | 
t=5  | 帧 B: submitSemaphores[0] | 同时渲染 image[2], image[0] | 显示 image[0] |
t=10 | 帧 C: submitSemaphores[1] | 同时渲染 3 个图像 | 显示 image[0] |
t=16 | - | submitSemaphores[0] 完成 | 显示 image[1] | 0 的轮次结束
t=20 | - | submitSemaphores[2] 完成 | 显示 image[1] | 2 还没轮到，等待
t=25 | - | submitSemaphores[1] 完成 | 显示 image[1] | 1 还没轮到，等待
t=33 | - | 所有帧都完成 | 显示 image[2] | 轮到 2 了
```

##### 三个顺序的对比

| 阶段 | 顺序 | 谁控制？ |
|------|------|----------|
| **CPU 提交** | 可能乱序（根据 imgIdx 可用性） | 驱动 |
| **GPU 渲染** | 可能乱序（不同帧完成时间不同） | GPU |
| **显示顺序** | **必须严格（0 → 1 → 2 → 3 → 4 → 0）** | **交换链** |

##### 为什么这样设计不会出错？

```
虽然我们以任意顺序提交和渲染帧，
但交换链保证显示顺序正确：

类比：电影院排座
- 你有 3 张电影票，座位号是 [0, 1, 2]
- 你可以任意顺序入场（先入 1 号座，再入 0 号座）
- 但电影按顺序播放，你不能跳着看

同样的：
- 我们可以任意顺序渲染帧
- 但显示器按顺序显示（0 → 1 → 2 → ...）
- 交换链和显示系统保证这个顺序
```

##### 关键要点

1. **提交顺序 ≠ 显示顺序**
   - CPU 可以按任意顺序提交帧（根据 imgIdx 可用性）
   - 但显示顺序由交换链严格控制

2. **渲染完成顺序 ≠ 显示顺序**
   - GPU 可以按任意顺序完成渲染
   - 但完成的帧必须等待轮到才显示

3. **交换链的责任**
   - 维护显示队列（当前显示到第几个图像）
   - 确保显示顺序正确（0 → 1 → 2 → ... → 0）
   - 处理排队和等待

4. **我们不需要担心**
   - 只要正确使用 vkQueuePresentKHR
   - 交换链会自动处理显示顺序
   - 我们只管提交，显示顺序由系统保证

##### 实际代码示例

```cpp
// 初始化
VkSemaphore acquireSemaphore;           // 1 个
VkSemaphore submitSemaphores[5];        // 5 个
VkFence inFlightFence;                  // 1 个（CPU 等待 GPU）

// 每帧渲染
vkWaitForFences(device, 1, &inFlightFence, ...);  // 等待上一帧
vkResetFences(device, 1, &inFlightFence);

uint32_t imageIndex;
vkAcquireNextImageKHR(..., acquireSemaphore, ..., &imageIndex);

vkQueueSubmit(
    ...,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &acquireSemaphore,  // 共享的获取信号量
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &submitSemaphores[imageIndex],  // 每个图像独立
    .fence = inFlightFence
);

vkQueuePresentKHR(..., &submitSemaphores[imageIndex]);
```

---

### Vulkan 的两级扩展系统

Vulkan 有实例级和设备级两种扩展，作用范围不同。

#### 架构层次

```
┌─────────────────────────────────────────────────────┐
│ VkInstance (实例)                                    │
│  - 代表 Vulkan 与应用程序的连接                       │
│  - 扩展：跨平台、全局功能                            │
│  - 例如：窗口表面、调试工具                          │
│                                                      │
│   ┌──────────────────────────────────────────┐      │
│   │ VkPhysicalDevice (物理设备 = GPU)         │      │
│   │   - 硬件本身，不创建对象                   │      │
│   │                                           │      │
│   │   ┌────────────────────────────────┐      │      │
│   │   │ VkDevice (逻辑设备)            │      │      │
│   │   │   - 应用程序操作 GPU 的接口     │      │      │
│   │   │   - 扩展：设备特定功能          │      │      │
│   │   │   - 一个 GPU 可以创建多个设备   │      │      │
│   │   └────────────────────────────────┘      │      │
│   └──────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────┘
```

#### 实例扩展（Instance Extensions）

在创建 `VkInstance` 时启用，用于全局功能。

| 扩展名 | 作用 | 说明 |
|--------|------|------|
| `VK_KHR_surface` | 跨平台表面抽象层 | 所有平台都需要 |
| `VK_KHR_win32_surface` | Windows 窗口表面 | Windows 平台 |
| `VK_EXT_debug_utils` | 调试消息功能 | 开发调试用 |

```cpp
// 创建 Instance 时启用
char* instance_extensions[] = {
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME
};

VkInstanceCreateInfo createInfo = {};
createInfo.ppEnabledExtensionNames = instance_extensions;
createInfo.enabledExtensionCount = ArraySize(instance_extensions);

vkCreateInstance(&createInfo, nullptr, &instance);
```

#### 设备扩展（Device Extensions）

在创建 `VkDevice` 时启用，用于设备特定功能。

| 扩展名 | 作用 | 说明 |
|--------|------|------|
| `VK_KHR_swapchain` | 交换链（显示图像） | 渲染到窗口必需 |
| `VK_KHR_ray_tracing` | 光线追踪 | 高级渲染特性 |
| `VK_KHR_fragment_shading_rate` | 片段着色率 | 性能优化 |

```cpp
// 创建 Device 时启用
char* device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkDeviceCreateInfo deviceInfo = {};
deviceInfo.ppEnabledExtensionNames = device_extensions;
deviceInfo.enabledExtensionCount = ArraySize(device_extensions);

vkCreateDevice(gpu, &deviceInfo, 0, &device);
```

#### 为什么 Swapchain 是设备扩展？

这是很多初学者的困惑点。原因如下：

1. **资源归属**
   - 交换链包含 `VkImage` 资源
   - 图像属于逻辑设备，不是实例
   - 设备负责管理图像内存

2. **设备特性**
   - 不同设备支持不同的交换链特性
   - HDR、可变刷新率等因设备而异
   - 需要针对每个设备查询和启用

3. **多实例支持**
   - 同一个 GPU 可以创建多个独立的交换链
   - 如果是实例扩展，就无法区分

```cpp
// 同一个 GPU，多个设备
VkPhysicalDevice gpu;

VkDevice device1;
VkDevice device2;

VkSwapchainKHR swapchain1;  // device1 的交换链
VkSwapchainKHR swapchain2;  // device2 的交换链（独立）

// 查询能力时也是针对设备的
VkSurfaceCapabilitiesKHR caps;
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);
// ↑ 不同 GPU 返回不同的能力
```

#### 两级扩展对比表

| 特性 | 实例扩展 | 设备扩展 |
|------|----------|----------|
| **启用时机** | 创建 VkInstance 时 | 创建 VkDevice 时 |
| **作用范围** | 全局、跨平台 | 设备特定 |
| **典型用途** | 窗口系统集成、调试 | 渲染特性、高级功能 |
| **资源管理** | 无资源拥有资源 | 拥有和管理资源 |
| **数量** | 一个程序一个实例 | 一个实例可以有多个设备 |

#### 记忆方法

类比连锁餐厅：

```
餐厅总部 = VkInstance (实例)
  └─ 扩展：外卖服务、会员系统
  └─ 全局功能，所有分店共享

分店 A = VkDevice 1
  └─ 扩展：早餐菜单、儿童餐区
  └─ 资源：厨房、员工、餐桌

分店 B = VkDevice 2
  └─ 扩展：24小时营业、咖啡吧
  └─ 资源：厨房、员工、餐桌（独立）

餐桌流转系统 = Swapchain
  └─ 为什么是分店扩展？
  └─ 因为每个分店的餐桌、客流、营业时间都不同！
  └─ 每个分店需要自己的餐桌管理系统
```

---

## 初始化流程

### Vulkan 应用初始化步骤

```
1. 创建 VkInstance
   - 启用实例扩展（窗口表面、调试工具）
   - 启用验证层
   ↓
2. 创建 VkSurface（窗口表面）
   - 连接 Vulkan 和窗口系统
   ↓
3. 选择物理设备（GPU）
   - 枚举所有 GPU
   - 查询设备能力
   ↓
4. 查找支持渲染的队列族
   - 检查队列族是否支持 surface
   ↓
5. 创建逻辑设备（VkDevice）
   - 启用设备扩展（交换链）
   - 创建队列
   ↓
6. 获取队列句柄（VkQueue）
   ↓
7. 创建交换链（VkSwapchain）
   - 查询 surface 格式和能力
   - 配置图像数量和属性
   ↓
8. 获取交换链图像（VkImage）
   ↓
9. 创建命令池（VkCommandPool）
   - 绑定到图形队列族
   - 用于分配命令缓冲区
   ↓
10. 创建同步对象（VkSemaphore）
   - acquireSemaphore：同步图像获取
   - submitSemaphore：同步渲染完成
   ↓
11. 创建渲染通道（VkRenderPass）
   - 配置附件（颜色、深度、模板）
   - 配置子阶段（Subpasses）
   - 定义加载/存储操作
   ↓
12. 创建帧缓冲（Framebuffer）
   - 将交换链图像绑定到渲染通道
```

### 关键代码模式

#### 两阶段查询模式

Vulkan API 常用"两次调用"模式：

```cpp
// 第一次：获取数量
vkGetXXX(device, &count, NULL);

// 第二次：获取数据
vkGetXXX(device, &count, array);
```

**示例**：

```cpp
// 获取物理设备
uint32_t gpuCount = 0;
VkPhysicalDevice gpus[10];

vkEnumeratePhysicalDevices(instance, &gpuCount, NULL);
vkEnumeratePhysicalDevices(instance, &gpuCount, gpus);

// 获取交换链图像
uint32_t imageCount = 0;
VkImage images[10];

vkGetSwapchainImagesKHR(device, swapchain, &imageCount, NULL);
vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images);
```

#### 渲染循环模式（Render Loop）

每帧渲染的标准流程：

```cpp
bool vk_render(VkContext* vkContext) {
    // 1. 获取交换链图像
    uint32_t imgIdx;
    vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,              // 超时（无限等待）
        acquireSemaphore,        // 图像可用时触发
        VK_NULL_HANDLE,
        &imgIdx
    );

    // 2. 分配命令缓冲区
    VkCommandBuffer cmdBuffer;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

    // 3. 开始录制命令
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // 4. 记录渲染命令（使用 RenderPass）
    //
    // 旧方法（不推荐）：
    //   - vkCmdClearColorImage() + 手动布局转换
    //
    // 新方法（推荐）：
    //   - vkCmdBeginRenderPass() → 自动处理布局转换和清除
    //   - vkCmdSetViewport()
    //   - vkCmdSetScissor()
    //   - vkCmdBindPipeline()
    //   - vkCmdDraw()
    //   - vkCmdEndRenderPass() → 自动转换到 PRESENT_SRC
    //
    VkClearValue clearColor = {1, 1, 0, 1};  // 黄色
    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = framebuffers[imgIdx];
    rpInfo.renderArea.extent = screenSize;
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmdBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0, 0, screenSize.width, screenSize.height, 0, 1};
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, screenSize};
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);  // 绘制 3 个顶点（三角形）

    vkCmdEndRenderPass(cmdBuffer);

    // 5. 结束录制
    vkEndCommandBuffer(cmdBuffer);

    // 6. 提交到队列
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &acquireSemaphore;  // 等待图像获取
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &submitSemaphore;  // 完成后触发
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

    // 7. 呈现图像
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &submitSemaphore;  // 等待渲染完成
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imgIdx;
    vkQueuePresentKHR(queue, &presentInfo);

    return true;
}
```

**渲染循环的七个步骤**：

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | 获取图像 | 等待可用的交换链图像 |
| 2 | 分配命令缓冲区 | 从命令池分配 |
| 3 | 开始录制 | `vkBeginCommandBuffer` |
| 4 | 记录命令 | 绘制、清除等操作 |
| 5 | 结束录制 | `vkEndCommandBuffer` |
| 6 | 提交队列 | 提交命令，等待/触发信号量 |
| 7 | 呈现图像 | 显示到屏幕 |

**同步流程**：

```
acquireSemaphore → 渲染命令 → submitSemaphore → 显示
     (等待)          (执行)        (触发)       (等待)
```

#### VkImageSubresourceRange 详解

清除图像时需要指定子资源范围，精确控制清除哪部分。

##### aspectMask（图像方面）

一张图像可以有多个"方面"：

| 标志 | 含义 | 使用场景 |
|------|------|----------|
| `COLOR_BIT` | 颜色数据 | 清除颜色缓冲 |
| `DEPTH_BIT` | 深度数据 | 清除深度缓冲 |
| `STENCIL_BIT` | 模板数据 | 清除模板缓冲 |
| `METADATA_BIT` | 元数据 | 稀疏纹理用 |

```cpp
// 清除颜色图像
range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

// 同时清除深度和模板
range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
```

##### levelCount（Mip 层级数量）

Mip Mapping（多级渐远纹理）的概念：

```
Level 0: ████████ 1024x1024  ← 原始尺寸（最大）
Level 1: ████     512x512    ← 一半尺寸
Level 2: ██       256x256    ← 四分之一尺寸
Level 3: █        128x128
...
```

```cpp
// 只清除 Level 0（交换链图像通常只有 1 个 mip 层级）
range.levelCount = 1;

// 清除所有 mip 层级
range.levelCount = VK_REMAINING_MIP_LEVELS;
```

##### layerCount（图层数量）

图层用于不同的渲染场景：

| 图层数 | 用途 |
|--------|------|
| 1 | 普通窗口渲染 |
| 2 | VR 立体渲染（左眼 + 右眼）|
| 6 | 立方体纹理（6 个面）|

```cpp
// 普通窗口渲染
range.layerCount = 1;

// VR 立体渲染
range.layerCount = 2;

// 所有图层
range.layerCount = VK_REMAINING_ARRAY_LAYERS;
```

---

### 图像布局转换辅助函数

#### 项目实现方式

项目使用 `transition_image_layout` 辅助函数封装 Pipeline Barrier：

```cpp
// 位置：vk_renderer.cpp 第 129-165 行
void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;     // 当前布局
    barrier.newLayout = newLayout;     // 目标布局
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;

    // 根据转换类型设置访问掩码和管线阶段
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // 第一次使用图像
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
```

#### 使用流程

```cpp
// 渲染循环中的调用顺序：
vkBeginCommandBuffer(cmdBuffer, &beginInfo);          // ← 1. 开始录制

transition_image_layout(cmdBuffer,                        // ← 2. 转换布局
                        image,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

vkCmdClearColorImage(cmdBuffer,                            // ← 3. 清除（布局正确）
                      image,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // ← 必须匹配转换后的布局
                      &color, 1, &range);

vkEndCommandBuffer(cmdBuffer);
```

#### 关键点

| 要点 | 说明 |
|------|------|
| **封装意义** | 简化代码、预定义正确参数、提高可读性 |
| **调用顺序** | 必须在 `vkBeginCommandBuffer` 之后 |
| **布局一致性** | 清除时使用的布局必须与转换后的布局一致 |

#### Vulkan SDK 1.4 的强制要求

**核心问题**：为什么必须显式转换布局？

Vulkan SDK 1.4 强制要求必须显式处理图像从"无状态"到"可工作状态"的转换。

##### VK_IMAGE_LAYOUT_UNDEFINED 的问题

```
交换链图像刚创建或刚获取时：
  内容是 "未定义" (UNDEFINED)
  ↓
  不能直接告诉 Vulkan 它是 TRANSFER_DST_OPTIMAL
  ↓
  必须执行 Pipeline Barrier 手动转换！
```

**原因**：
- Vulkan 驱动不知道图像之前的状态
- 直接使用可能导致未定义行为
- SDK 1.4 加强验证，要求显式转换

##### 两个关键转换点

**转换 1：UNDEFINED → TRANSFER_DST_OPTIMAL**

```
目的：让图像准备好接收清除/复制操作
时机：清除操作之前
原因：刚获取的交换链图像处于 UNDEFINED 状态
```

```cpp
// 第一次使用图像时必须转换
transition_image_layout(cmdBuffer, image,
    VK_IMAGE_LAYOUT_UNDEFINED,           // ← 当前：未定义
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); // ← 目标：传输目标

// 现在可以安全清除
vkCmdClearColorImage(..., VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ...);
```

**转换 2：TRANSFER_DST_OPTIMAL → PRESENT_SRC_KHR**

```
目的：让图像准备好显示
时机：呈现操作之前
原因：清除后图像处于传输目标布局，不能直接显示
```

```cpp
// 清除完成后，如果要显示，需要转换回呈现布局
transition_image_layout(cmdBuffer, image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // ← 当前：传输目标
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);      // ← 目标：呈现源

// 现在可以安全呈现
vkQueuePresentKHR(...);
```

##### 完整的生命周期

```
图像创建 → UNDEFINED
    ↓
第一次获取 → Pipeline Barrier → TRANSFER_DST_OPTIMAL
    ↓                               ↓
清除操作 ----------------------→ 操作完成
    ↓                               ↓
（可选）呈现前 → Pipeline Barrier → PRESENT_SRC_KHR
    ↓
显示到屏幕
```

##### 为什么 Vulkan 这样要求？

1. **性能优化**：驱动可以根据布局优化内存访问
2. **安全性**：显式状态转换避免未定义行为
3. **可追踪性**：每个布局转换都是明确的同步点

##### 常见错误

```cpp
// ❌ 错误：直接使用 UNDEFINED 布局
vkCmdClearColorImage(..., VK_IMAGE_LAYOUT_UNDEFINED, ...);
// 报错：布局必须是 TRANSFER_DST_OPTIMAL 或 GENERAL

// ❌ 错误：不转换就呈现
vkCmdClearColorImage(..., VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ...);
vkQueuePresentKHR(...);  // 图像还是 TRANSFER_DST 布局！
// 报错：呈现需要 PRESENT_SRC_KHR 布局

// ✅ 正确：显式转换每个状态
transition_image_layout(..., UNDEFINED, TRANSFER_DST);     // 准备清除
vkCmdClearColorImage(..., TRANSFER_DST_OPTIMAL, ...);       // 清除
transition_image_layout(..., TRANSFER_DST, PRESENT_SRC);     // 准备显示
vkQueuePresentKHR(...);                                     // 呈现
```

---

#### 完整渲染循环（带围栏同步）

```cpp
bool vk_render(VkContext* vkContext) {
    // 0. 等待上一帧完成
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fence);

    // 1. 获取图像
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquireSemaphore, ..., &imgIdx);

    // 2. 分配命令缓冲区
    vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

    // 3. 开始录制
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // 4. 记录命令（清除、绘制等）
    transition_image_layout(..., UNDEFINED, TRANSFER_DST);
    vkCmdClearColorImage(..., TRANSFER_DST_OPTIMAL, ...);
    vkEndCommandBuffer(cmdBuffer);

    // 5. 提交到队列
    VkSubmitInfo submitInfo = {};
    submitInfo.pWaitSemaphores = &acquireSemaphore;   // 等待图像
    submitInfo.pWaitDstStageMask = &COLOR_ATTACHMENT_OUTPUT;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.pSignalSemaphores = &submitSemaphore; // 完成触发
    vkQueueSubmit(queue, 1, &submitInfo, fence);       // GPU 完成触发 fence

    // 6. 呈现图像
    VkPresentInfoKHR presentInfo = {};
    presentInfo.pWaitSemaphores = &submitSemaphore;     // 等待渲染完成
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imgIdx;
    vkQueuePresentKHR(queue, &presentInfo);

    return true;
}
```

**关键点**：
- 第 0 步必须在最前（等待上一帧）
- `vkQueueSubmit` 必须传入 fence（不是 NULL）
- `VK_FENCE_CREATE_SIGNALED_BIT` 让第一帧不需要等待

---

### 图像布局转换（Image Layout Transition）

#### 什么是图像布局？

Vulkan 中图像在不同用途下需要不同的"布局"（Layout）：

| 布局 | 用途 | 典型操作 |
|------|------|----------|
| `UNDEFINED` | 未定义 | 初始状态 |
| `PRESENT_SRC_KHR` | 呈现源 | 显示到屏幕 |
| `TRANSFER_DST_OPTIMAL` | 传输目标 | 清除、复制 |
| `COLOR_ATTACHMENT_OPTIMAL` | 颜色附件 | 渲染 |
| `SHADER_READ_ONLY_OPTIMAL` | 着色器只读 | 纹理采样 |
| `GENERAL` | 通用 | 任何操作（性能较差）|

#### 为什么需要转换？

```
交换链图像获取时: PRESENT_SRC_KHR（显示用）
         ↓ 必须转换
vkCmdClearColorImage 要求: TRANSFER_DST_OPTIMAL（传输目标）
         ↓ 转换完成
可以执行清除操作
```

#### 如何转换：Pipeline Barrier

```cpp
VkImageMemoryBarrier barrier = {};
barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;       // 当前布局
barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;   // 目标布局
barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.image = image;
barrier.subresourceRange = range;

vkCmdPipelineBarrier(
    cmdBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,      // 源阶段
    VK_PIPELINE_STAGE_TRANSFER_BIT,          // 目标阶段
    0, 0, NULL, 0, NULL,
    1, &barrier                             // 图像屏障
);
```

#### 围栏（Fence）vs 信号量（Semaphore）

| 特性 | 信号量 | 围栏 |
|------|--------|------|
| **作用范围** | GPU 内部 | CPU ↔ GPU |
| **重置方式** | 自动（二进制） | 手动（vkResetFences） |
| **用途** | 管线阶段同步 | 帧间同步 |
| **创建标志** | 无 | `VK_FENCE_CREATE_SIGNALED_BIT` |

**为什么渲染循环需要围栏？**

```cpp
// 问题：vkAcquireNextImageKHR 要求信号量没有待完成的操作
// 如果上一帧还在执行，acquireSemaphore 仍然处于"已触发"状态

// 解决：
while (running) {
    // 1. CPU 等待上一帧完成
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    // 2. 重置围栏（手动！）
    vkResetFences(device, 1, &fence);

    // 3. 获取图像
    vkAcquireNextImageKHR(..., acquireSemaphore, ...);

    // ... 录制命令 ...

    // 4. 提交时传入围栏，GPU 完成后自动触发
    vkQueueSubmit(queue, 1, &submitInfo, fence);

    // 5. 呈现
    vkQueuePresentKHR(...);
}
```

---

### 渲染通道（Render Pass）

#### 什么是渲染通道？

渲染通道是 Vulkan 中描述"如何渲染"的蓝图，定义了渲染的完整流程。

```
Render Pass = 整个渲染过程的描述
Subpass    = 具体渲染步骤
Attachment = 渲染目标（画布）
```

#### 类比理解

```
绘画过程：
1. 准备画布（附件设置）
2. 画背景（Subpass 0）
3. 画人物（Subpass 1）
4. 签名盖章（完成）

Vulkan 渲染：
1. 创建 Render Pass（定义附件和流程）
2. vkCmdBeginRenderPass（开始渲染）
3. vkCmdDrawIndexed（执行绘制）
4. vkCmdEndRenderPass（结束渲染）
```

#### 附件（Attachment）类型

| 附件类型 | 作用 | 常见用途 |
|----------|------|----------|
| **Color** | 存储像素颜色 | 渲染结果显示 |
| **Depth** | 存储深度信息 | 深度测试、3D 渲染 |
| **Stencil** | 存储模板信息 | 模板测试、遮罩效果 |

#### 加载和存储操作

##### LoadOp（附件开始时的操作）

| 操作 | 说明 | 使用场景 |
|------|------|----------|
| `CLEAR` | 清除附件为特定值 | 每帧重新开始 |
| `LOAD` | 保留之前的内容 | 增量渲染、分帧渲染 |
| `DONT_CARE` | 不关心内容 | 性能优化 |

##### StoreOp（渲染完成后的操作）

| 操作 | 说明 | 使用场景 |
|------|------|----------|
| `STORE` | 保存结果到内存 | 正常渲染 |
| `DONT_CARE` | 不保存 | 临时附件 |

#### 图像布局在 Render Pass 中的作用

```
initialLayout → [Subpass 渲染] → finalLayout
     ↓                            ↓
UNDEFINED                      PRESENT_SRC
                        ATTACHMENT
                        OPTIMAL
```

#### 创建和使用示例

```cpp
// 创建
VkRenderPassCreateInfo rpInfo = {};
rpInfo.pAttachments = attachments;
rpInfo.attachmentCount = 1;
rpInfo.pSubpasses = &subpassDesc;
rpInfo.subpassCount = 1;
vkCreateRenderPass(device, &rpInfo, NULL, &renderPass);

// 使用
vkCmdBeginRenderPass(cmdBuffer, renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
vkCmdDrawIndexed(cmdBuffer, ...);
vkCmdEndRenderPass(cmdBuffer);
```

---

### 帧缓冲（Framebuffer）

#### 什么是帧缓冲？

帧缓冲将 Render Pass 中定义的附件与实际的图像视图绑定起来。

```
Render Pass  = 作画计划书（定义要用到哪些画布）
Framebuffer  = 准备好的实际画布（指向真实的图像）
ImageView    = 画布的视图（如何查看/访问图像）
```

#### 为什么需要 Framebuffer？

Render Pass 只是描述"需要什么类型的附件"，而 Framebuffer 提供"具体的图像"。

一个 Render Pass 可以配合多个 Framebuffer 使用。

#### Framebuffer 创建流程

```
1. 为每个交换链图像创建 ImageView
   ↓
2. 创建 Framebuffer（绑定 ImageView 到 Render Pass 附件）
   ↓
3. 渲染时根据当前图像索引选择对应的 Framebuffer
```

#### 渲染时的使用

```
vkAcquireNextImageKHR → 获取 imageIdx
   ↓
vkCmdBeginRenderPass → 使用 framebuffers[imageIdx]
   ↓
[渲染命令]
   ↓
vkCmdEndRenderPass
   ↓
vkQueuePresentKHR → 显示 scImages[imageIdx]
```

#### 关键概念

| 概念 | 说明 |
|------|------|
| **附件绑定** | Framebuffer 将 ImageView 绑定到 Render Pass 的附件槽位 |
| **一个图像一个 Framebuffer** | 每个交换链图像都需要独立的 Framebuffer |
| **尺寸匹配** | Framebuffer 尺寸必须与附件图像尺寸一致 |

---

### 着色器（Shader）

#### 什么是着色器？

着色器是在 GPU 上运行的小程序，用于计算顶点位置和像素颜色。

```
顶点着色器 (Vertex Shader)  → 处理顶点 → 输出位置
                             ↓
                          光栅化
                             ↓
片段着色器 (Fragment Shader) → 处理像素 → 输出颜色
```

#### GLSL 版本与 Vulkan 版本

| GLSL 版本 | Vulkan 版本 | SPIR-V 版本 |
|-----------|-------------|-------------|
| 450       | 1.0         | 1.0         |
| 460       | 1.2         | 1.5         |

#### 顶点着色器（Vertex Shader）

**作用**：
- 处理每个顶点的数据
- 将顶点从模型空间转换到裁剪空间
- 传递数据到片段着色器

**内置变量**：
- `gl_VertexIndex`：当前顶点的索引
- `gl_Position`：顶点的最终位置（输出）

**坐标系统（Vulkan NDC）**：
```
      +X
       │
       │
       │
       └──── +Y (向下)
      ╱
     ╱
    +Z（指向屏幕外）

范围：[-1, 1] × [-1, 1] × [-1, 1]
```
uint32_t 1
uint32_t 2
1111
DOWRD 3 

**重要区别** - Vulkan：Y 轴正方向向下（与帧缓冲区坐标系一致）

- OpenGL：Y 轴正方向向上

**示例代码**：
```glsl
#version 450

vec2 vertices[3] = vec2[3](
    vec2(-0.5, 0.5),   // 左上
    vec2(0, -0.5),     // 底部中间
    vec2(0.5, 0.5)     // 右上
);

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
}
```

#### 片段着色器（Fragment Shader）

**作用**：
- 为每个像素计算最终颜色
- 处理纹理、光照、材质效果

**内置变量**：
- `gl_FragCoord`：当前像素的窗口坐标（输入）
- `gl_FragDepth`：深度值（输出）

**输出格式**：
```glsl
layout(location = 0) out vec4 fragColor;
//                         ↑         ↑
//                      位置索引    RGBA 颜色

void main() {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);  // 红色
}
```

#### 颜色表示

| 颜色 | vec4 值 | 说明 |
|------|---------|------|
| 黑色 | `(0, 0, 0, 1)` | RGB 全为 0 |
| 白色 | `(1, 1, 1, 1)` | RGB 全为 1 |
| 红色 | `(1, 0, 0, 1)` | 只有 R 通道 |
| 绿色 | `(0, 1, 0, 1)` | 只有 G 通道 |
| 蓝色 | `(0, 0, 1, 1)` | 只有 B 通道 |
| 黄色 | `(1, 1, 0, 1)` | R + G |
| 青色 | `(0, 1, 1, 1)` | G + B |
| 品红 | `(1, 0, 1, 1)` | R + B |

#### 渲染管线流程

```
vkCmdDraw
   ↓
[顶点着色器] - 处理 3 个顶点
   ↓
[图元装配] - 组装三角形
   ↓
[光栅化] - 三角形 → 像素
   ↓
[片段着色器] - 计算每个像素颜色
   ↓
[帧缓冲] - 写入图像
```

---

### 光栅化状态（Rasterization State）

#### 什么是光栅化？

光栅化是将几何图形（三角形、线等）转换为片段（像素）的过程。

```
三角形 (3 个顶点)  → 光栅化  →  像素阵列（例如 100×100 个像素）
```

#### 正面和背面（Front Face vs Back Face）

**三角形像一张"薄纸"，有两个面：**

```
     观察者（你）
        ↓
    ┌─────────┐
    │  屏幕  │
    └─────────┘
        ↓
  ┌───────────┐
  │ 背面      │ ← 朝向屏幕里面
  │ ─────────│
  │ 正面      │ ← 朝向观察者
  └───────────┘
```

#### 如何判断正面/背面？

**看顶点绘制顺序（在观察者眼里）：**

```
顺时针 (CW)  → 正面 → 朝向观察者
逆时针 (CCW) → 背面 → 背向观察者
```

**示例 1：顺时针三角形（正面）**

```
    v0 ●
        │ \
        │   \  v0 → v1 → v2 是顺时针
        │     \
    v1 ●-------● v2

    ✓ 正面朝向观察者 → 渲染
```

**示例 2：逆时针三角形（背面）**

```
    v2 ●
        │ \
        │   \  v2 → v1 → v0 是顺时针（相当于 v0→v1→v2 逆时针）
        │     \
    v1 ●-------● v0

    ✗ 背面朝向观察者 → 剔除（不渲染）
```

#### Vulkan 的 Y 轴方向

```
Vulkan: Y 轴向下
        +X
         │
         │
         │
         └──── +Y (向下)
        ╱
       ╱
      +Z（指向屏幕外）
```

#### 顶点顺序判断示例

```glsl
// 顺时针三角形（Vulkan 中为正面）
vec2 vertices[3] = vec2[3](
    vec2(-0.5, 0.5),   // v0: 左下
    vec2(0, -0.5),     // v1: 顶部
    vec2(0.5, 0.5)     // v2: 右下
);
```

```
        v1 (0, -0.5)
             │
             │
             │
v0 (-0.5, 0.5) ────── v2 (0.5, 0.5)

v0 → v1 → v2: 顺时针 → 正面
```

#### 背面剔除（Back Face Culling）

**作用**：不渲染背对观察者的三角形，提升性能。

| 配置 | 效果 |
|------|------|
| `frontFace = CLOCKWISE` | 顺时针 = 正面 |
| `frontFace = COUNTER_CLOCKWISE` | 逆时针 = 正面 |
| `cullMode = BACK` | 剔除背面 |
| `cullMode = FRONT` | 剔除正面 |
| `cullMode = NONE` | 不剔除 |

**代码示例：**

```cpp
VkPipelineRasterizationStateCreateInfo rasterizationState = {};
rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;  // 顺时针为正面
rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;      // 剔除背面
```

#### 2D 渲染中为什么需要正面/背面？

1. **性能优化**：避免渲染看不见的面
2. **透明度问题**：防止背面干扰正面
3. **2D 中的 3D 元素**：旋转卡片、翻转精灵等

**纯 2D 图形**：如果所有三角形顶点顺序一致，背面剔除不会剔除任何东西。

---

## 常见错误与解决

### 1. 扩展未启用错误

**错误信息**：
```
Validation Error: vkCreateWin32SurfaceKHR(): extension VK_KHR_win32_surface not enabled
```

**原因**：创建 Instance 时忘记设置 `enabledExtensionCount`

**解决方案**：
```cpp
VkInstanceCreateInfo createInfo = {};
createInfo.ppEnabledExtensionNames = extensions;
createInfo.enabledExtensionCount = ArraySize(extensions);  // ← 必须设置
```

---

### 2. 队列创建信息数量错误

**错误信息**：
```
Validation Error: vkCreateDevice(): pCreateInfo->queueCreateInfoCount is 0
```

**原因**：创建逻辑设备时忘记设置队列创建信息数量

**解决方案**：
```cpp
VkDeviceCreateInfo deviceInfo = {};
deviceInfo.pQueueCreateInfos = &queueInfo;
deviceInfo.queueCreateInfoCount = 1;  // ← 必须设置
```

---

### 3. 类型转换错误（Windows + MSVC）

**错误信息**：
```
error C2440: "=": 无法从回调函数类型转换为 PFN_vkDebugUtilsMessengerCallbackEXT
```

**原因**：Windows + MSVC 的调用约定差异

**解决方案**：
```cpp
debug.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)vk_debug_callback;
```

**详细解释**：
- `VKAPI_CALL` 在 Windows 上是 `__stdcall`
- MSVC 推断函数指针时使用 `__cdecl`（C++ 默认）
- 两种调用约定类型不兼容，需要显式转换

---

### 4. Windows 类型未定义错误

**错误信息**：
```
error C3646: "hwnd": 未知重写说明符
error C4430: 缺少类型说明符 - 假定为 int
```

**原因**：包含 `vulkan_win32.h` 之前没有包含 `windows.h`

**解决方案**：
```cpp
#include <windows.h>              // ← 必须在前面
#include <vulkan/vulkan_win32.h>
```

---

### 5. 程序异常退出（返回 -3）

**症状**：程序启动后立即退出，返回代码 -3

**可能原因**：
1. `vk_init()` 返回 `false`
2. 调试扩展加载失败但代码直接退出
3. Instance 创建失败

**调试方法**：
```cpp
int main() {
    if (!vk_init(&vkContext, hwnd)) {
        return -3;  // ← 在这里设置断点调试
    }
}
```

---

### 6. SPIR-V 版本不兼容错误

**错误信息**：
```
Validation Error: vkCreateShaderModule(): SPIR-V Capability Shader was declared,
but one of the following requirements is required (VK_VERSION_1_0).
```

**原因**：着色器使用的 SPIR-V 版本比应用程序请求的 Vulkan API 版本更新

**版本对应关系**：

| Vulkan 版本 | SPIR-V 版本 | 发布年份 |
|-------------|-------------|----------|
| Vulkan 1.0 | SPIR-V 1.0 | 2016 |
| Vulkan 1.1 | SPIR-V 1.3 | 2018 |
| Vulkan 1.2 | SPIR-V 1.5 | 2020 |
| Vulkan 1.3 | SPIR-V 1.6 | 2022 |

**解决方案**：

在 `VkApplicationInfo` 中设置 `apiVersion`：

```cpp
VkApplicationInfo appInfo = {};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "My Application";
appInfo.apiVersion = VK_API_VERSION_1_2;  // ← 关键：必须设置
```

**替代方案**（不推荐）：重新编译着色器，使用较低的 SPIR-V 版本

```bash
# 使用 glslc 编译时指定目标 SPIR-V 版本
glslc shader.vert -o shader.vert.spv --target-spv=spv1.0
```

**为什么默认会失败？**

- `VkApplicationInfo.apiVersion` 默认为 0，Vulkan 会使用 **1.0 版本**
- 现代 GLSL 编译器（如 SDK 1.4 的 glslc）默认生成 **SPIR-V 1.5+**
- 版本不匹配导致验证层报错

---

### 6. 图像布局不正确

**错误信息**：
```
Validation Error: vkCmdClearColorImage(): Layout is PRESENT_SRC_KHR
but can only be TRANSFER_DST_OPTIMAL, SHARED_PRESENT_KHR, or GENERAL.
```

**原因**：交换链图像的布局是 `PRESENT_SRC_KHR`，但 `vkCmdClearColorImage` 要求 `TRANSFER_DST_OPTIMAL`

**解决方案**：在清除前使用 Pipeline Barrier 转换布局
```cpp
VkImageMemoryBarrier barrier = {};
barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
barrier.image = image;
vkCmdPipelineBarrier(cmdBuffer, TOP_OF_PIPE, TRANSFER, 0, 0, NULL, 0, NULL, 1, &barrier);
```

---

### 7. 缺少 TRANSFER_DST_BIT

**错误信息**：
```
Validation Error: vkCmdClearColorImage(): image was created with usage
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT (missing VK_IMAGE_USAGE_TRANSFER_DST_BIT).
```

**原因**：清除操作本质是"数据传输"，图像必须启用 `TRANSFER_DST_BIT`

**解决方案**：
```cpp
scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
```

---

### 8. 信号量未等待完成

**错误信息**：
```
Validation Error: vkAcquireNextImageKHR(): Semaphore must not have any pending operations.
```

**原因**：上一帧的 `acquireSemaphore` 还没被等待就再次使用

**解决方案**：使用围栏（Fence）同步
```cpp
// 每帧开始时等待上一帧完成
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
vkResetFences(device, 1, &fence);

// 提交时传入围栏
vkQueueSubmit(queue, 1, &submitInfo, fence);  // 不是 NULL
```

---

## 学习资源

### 官方文档
- [Vulkan 规范](https://registry.khronos.org/vulkan/specs/1.3/html/)
- [Vulkan 教程](https://vulkan-tutorial.com/)
- [Vulkan Guide](https://vulkan-guide.com/)

### 推荐书籍
- Vulkan Programming Guide
- Real-Time Rendering, 4th Edition
- GPU Gems 系列

---

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2025-03-26 | 1.0 | 初始版本，记录 Vulkan 初始化核心概念 |

---

**注**：本文档是学习过程中的笔记，内容会随着项目进展持续更新。