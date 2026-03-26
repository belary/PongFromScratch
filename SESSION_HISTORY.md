# Vulkan 学习会话历史

## 2025-03-26 - 第1天会话

### 学习目标
从零开始构建 Vulkan 渲染器，学习 Vulkan API 的核心概念。

### 完成内容

#### 1. 代码注释和文档化
- ✅ 为 `vk_init.cpp` 添加详细中文注释
- ✅ 为 `vk_renderer.cpp` 添加详细学习注释
- ✅ 解释每个函数的作用和工作原理

#### 2. 环境配置问题解决
- ✅ 解决头文件包含顺序问题（`windows.h` vs `vulkan_win32.h`）
- ✅ 配置 `.clang-format` 避免格式化破坏包含顺序
- ✅ 设置 Allman 风格大括号（单独一行）

#### 3. 核心概念学习

**已理解的概念：**
- ✅ VkInstance vs VkDevice 的区别
- ✅ Physical Device vs Logical Device
- ✅ Queue Family（队列族）的概念
- ✅ Surface vs Swapchain vs Image 的关系
- ✅ Vulkan 两级扩展系统（实例扩展 vs 设备扩展）
- ✅ 为什么 Swapchain 是设备扩展

**关键理解：**
```cpp
Surface = "可以显示图像的地方"（窗口的抽象）
Swapchain = 管理图像缓冲区的机制（前后缓冲）
Image = 实际的图像数据（GPU 内存）
```

#### 4. 初始化流程实现
- ✅ 创建 VkInstance
- ✅ 启用调试消息接收器（Validation Layer）
- ✅ 选择物理设备（GPU）
- ✅ 查找支持渲染的队列族
- ✅ 创建逻辑设备（VkDevice）
- ✅ 创建交换链（VkSwapchain）
- ✅ 获取交换链图像（VkImage）

### 遇到的错误和解决方案

#### 错误 1：扩展未启用
```
Validation Error: vkCreateWin32SurfaceKHR(): extension not enabled
```
**原因**：忘记设置 `enabledExtensionCount`
**解决**：添加 `createInfo.enabledExtensionCount = ArraySize(extensions);`

#### 错误 2：队列创建信息数量为 0
```
Validation Error: vkCreateDevice(): queueCreateInfoCount is 0
```
**原因**：忘记设置 `queueCreateInfoCount`
**解决**：添加 `deviceInfo.queueCreateInfoCount = 1;`

#### 错误 3：Windows 类型未定义
```
error C3646: "hwnd": 未知重写说明符
```
**原因**：包含 `vulkan_win32.h` 前没有包含 `windows.h`
**解决**：调整包含顺序

#### 错误 4：函数指针类型转换
```
error C2440: 无法转换为 PFN_vkDebugUtilsMessengerCallbackEXT
```
**原因**：Windows + MSVC 的调用约定差异
**解决**：显式类型转换

### 当前代码状态

#### 文件结构
```
src/
├── platform/
│   └── win32_platform.cpp    (Windows 平台代码)
└── renderer/
    ├── vk_init.cpp            (Vulkan 初始化辅助函数)
    └── vk_renderer.cpp        (Vulkan 渲染器核心)
```

#### VkContext 结构
```cpp
typedef struct VkContext
{
    VkInstance instance;                // Vulkan 实例
    VkDebugUtilsMessengerEXT debugMessenger;  // 调试消息接收器
    VkSurfaceKHR surface;               // 窗口表面
    VkSurfaceFormatKHR surfaceFormat;   // 表面格式
    VkPhysicalDevice gpu;               // 物理设备
    VkDevice device;                    // 逻辑设备
    VkQueue graphicsQueue;              // 图形队列
    VkSwapchainKHR swapChain;           // 交换链
    uint32_t scImgCount;                // 交换链图像数量
    VkImage scImages[5];                // 交换链图像数组
    int graphicsIdx;                    // 图形队列族索引
} VkContext;
```

### 已创建的文档

#### 1. VULKAN_LEARNING.md
完整的学习笔记，包含：
- 环境配置
- 头文件包含顺序
- 核心概念解释
- 初始化流程
- 常见错误与解决
- 两级扩展系统详解

#### 2. .clang-format
格式化配置：
- Allman 风格大括号
- 禁用 include 排序
- 4 空格缩进

### 关键代码片段

#### 两阶段查询模式
```cpp
// 第一次：获取数量
vkGetXXX(device, &count, NULL);

// 第二次：获取数据
vkGetXXX(device, &count, array);
```

#### 扩展启用模式
```cpp
// 实例扩展
char* instance_extensions[] = {
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME
};

// 设备扩展
char* device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
```

### 下一步学习方向

#### 待完成的初始化步骤
- [ ] 创建渲染通道（Render Pass）
- [ ] 创建帧缓冲（Framebuffer）
- [ ] 创建命令缓冲（Command Buffer）
- [ ] 创建同步对象（Semaphore/Fence）

#### 待学习的概念
- [ ] Pipeline Layout
- [ ] Graphics Pipeline 创建
- [ ] 着色器模块（Shader Module）
- [ ] 顶点缓冲（Vertex Buffer）
- [ ] 描述符集（Descriptor Set）

#### 优化方向
- [ ] 添加更多错误检查
- [ ] 实现资源清理函数
- [ ] 添加帧率控制
- [ ] 实现窗口大小调整

### 重要记忆点

#### 1. 调用约定（Windows + MSVC）
```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL callback(...) {
    // VKAPI_CALL = __stdcall (Windows)
    // 需要强制转换函数指针
}
debug.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)callback;
```

#### 2. 头文件包含顺序
```cpp
#include <windows.h>              // ← 必须在最前
#include <vulkan/vulkan_win32.h>
```

#### 3. 两阶段查询
```cpp
vkGetXXX(..., &count, NULL);      // 获取数量
vkGetXXX(..., &count, array);     // 获取数据
```

#### 4. 为什么 Swapchain 是设备扩展
- Swapchain 包含 VkImage 资源
- VkImage 属于逻辑设备
- 不同设备支持不同特性
- 同一 GPU 可创建多个独立交换链

### 编译和运行

#### 构建命令
```bash
build.bat
```

#### 运行
```bash
main.exe
```

### 调试技巧

1. **使用 Validation Layer**
   - 已启用 VK_LAYER_KHRONOS_validation
   - 所有错误会打印到控制台

2. **使用 Visual Studio 调试器**
   - F5 启动调试
   - 在关键位置设置断点

3. **常见退出代码**
   - 0 = 正常退出
   - -2 = 窗口创建失败
   - -3 = Vulkan 初始化失败

### 参考资料

- [Vulkan 规范](https://registry.khronos.org/vulkan/specs/1.3/html/)
- [Vulkan 教程](https://vulkan-tutorial.com/)
- Vulkan SDK 版本：1.4.341.1

---

## 下次会话计划

1. 创建渲染通道（Render Pass）
2. 创建图形管线（Graphics Pipeline）
3. 编写着色器（GLSL）
4. 创建帧缓冲（Framebuffer）
5. 实现基本的三角形渲染

---

**会话时间**：约 2 小时
**学习进度**：Vulkan 初始化完成 ✅
**下一步**：渲染管线和着色器
