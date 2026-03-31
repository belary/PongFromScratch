# Vulkan 学习笔记

从零开始构建 Vulkan 渲染器的学习笔记，记录遇到的问题、解决方案和核心概念理解。

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

    // 4. 记录渲染命令
    // ... 记录绘制命令 ...

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