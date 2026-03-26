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
│  Surface = 墙/画布底板              │  ← "可以显示图像的地方"
│  ┌─────────┐                        │
│  │ Image 1 │ ← Swapchain 管理的图像 │
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