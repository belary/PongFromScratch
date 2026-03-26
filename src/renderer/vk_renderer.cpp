#include <vulkan/vulkan.h>
#ifdef WINDOWS_BUILD
// ============================================================================
// Windows 平台头文件包含顺序（学习重点）
// ============================================================================
//
// 为什么顺序很重要？—— C++ 编译器的"所见即所得"原则
//
// 编译器从上到下逐行解析代码，只能"看到"之前已经定义/包含的内容
//
// 依赖链：
//   vulkan_win32.h  →  使用了 HWND, HANDLE, HINSTANCE 等类型
//                         ↓
//                    这些类型定义在 <windows.h> 中
//
// 错误示例：
//   #include <vulkan_win32.h>  ❌ 编译器：HWND 是什么？不知道！报错！
//   #include <windows.h>       ← 现在定义 HWND 了，但上面已经编译失败了
//
// 正确示例（当前代码）：
//   #include <windows.h>       ← 先定义 Windows 类型
//   #include <vulkan_win32.h>  ✅ 编译器：我知道 HWND 是什么，继续解析
//
// vulkan_win32.h 内部简化示例：
//   typedef struct VkWin32SurfaceCreateInfoKHR {
//       HWND hwnd;              // ⚠️ 需要先定义 HWND
//       HINSTANCE hinstance;    // ⚠️ 需要先定义 HINSTANCE
//   } VkWin32SurfaceCreateInfoKHR;
//
// 为什么教程可能没这个问题？
//   1. 其他头文件间接包含了 <windows.h>
//   2. 使用了预编译头（stdafx.h, pch.h）
//   3. 不同编译器的包含顺序检查更宽松
// ============================================================================

#include <windows.h>
#include <vulkan/vulkan_win32.h>

#elif LINUX_BUILD
#endif

#include <iostream>

#include "vk_init.cpp"

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

#define VK_CHECK(result)                                                                           \
    if (result != VK_SUCCESS)                                                                      \
    {                                                                                              \
        std::cout << "Vulkan Error: " << result << std::endl;                                      \
        __debugbreak();                                                                            \
        return false;                                                                              \
    }

/**
 * Vulkan 调试回调函数
 *
 * 此函数被 Vulkan Validation Layer 调用，用于接收和显示调试信息。
 * 当应用程序违反 Vulkan 规范或存在潜在问题时，验证层会调用此函数。
 *
 * -----
 * 宏定义说明（学习重点）：
 *
 * VKAPI_ATTR:
 *   - 在 Windows 上定义为 __declspec(dllexport) 或空
 *   - 用于控制函数的导出/导入属性
 *   - 确保函数在 DLL 边界正确工作
 *
 * VKAPI_CALL:
 *   - 在 Windows 上定义为 __stdcall（WINAPI）
 *   - 在 Linux/macOS 上通常为空（使用默认调用约定）
 *   - __stdcall 调用约定：调用者压栈，被调者清理栈（固定参数）
 *   - C++ 默认 __cdecl：调用者压栈，调用者清理栈（可变参数）
 *
 * 为什么需要这些宏？
 *   Vulkan 需要确保跨平台、跨编译器的二进制兼容性
 *   Windows API 统一使用 __stdcall，所以 Vulkan 在 Windows 上也必须使用
 *
 * -----
 *
 * @param msgServerity  消息严重性级别（Error/Warning/Info/Verbose）
 * @param msgFlags      消息类型（通用/性能/规范验证）
 * @param pCallbackData 包含详细调试信息的结构体（pMessage 字段包含错误消息）
 * @param pUserData     用户自定义数据（创建 messenger 时传入的指针）
 *
 * @return VkBool32     返回 VK_TRUE 表示应该终止 Vulkan 调用
 *                      返回 VK_FALSE 表示继续执行（通常使用此选项）
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT msgServerity, VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    // 输出验证层的错误/警告消息到控制台
    std::cout << "Validation Error: " << pCallbackData->pMessage << std::endl;

    // 返回 false 表示不终止 Vulkan 调用，让程序继续执行
    // 这样可以一次性看到所有验证错误，而不是在第一个错误处停止
    return false;
}

typedef struct VkContext
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapChain;

    uint32_t scImgCount;
    VkImage scImages[5];

    int graphicsIdx;

} VkContext;

bool vk_init(VkContext* vkContext, void* window)
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong From Scratch";
    appInfo.pEngineName = "Pong Engine";

    // Vulkan 实例扩展列表
    //
    // 扩展说明：
    // 1. VK_KHR_win32_surface (Windows): 创建 Vulkan 窗口表面所需
    // 2. VK_EXT_debug_utils: 调试消息功能
    // 3. VK_KHR_surface: 跨平台表面抽象层
    //
    // 为什么需要这些扩展？
    // - Vulkan 核心不包含平台特定的窗口系统集成
    // - 需要通过扩展来启用 Win32/Linux 等平台支持
    //
    char* extensions[] = {
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME, // Windows 平台表面扩展
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME, // 调试工具扩展
        VK_KHR_SURFACE_EXTENSION_NAME      // 表面抽象层扩展
    };

    char* layers[] = {"VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = ArraySize(extensions); // ⚠️ 关键：必须设置扩展数量！
    createInfo.enabledLayerCount = ArraySize(layers);
    createInfo.ppEnabledLayerNames = layers;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &vkContext->instance));

    // 动态加载 Vulkan 扩展函数
    //
    // Vulkan 核心函数（如 vkCreateInstance）直接链接到 Vulkan 驱动
    // 扩展函数（如 vkCreateDebugUtilsMessengerEXT）需要运行时动态加载
    //
    // vkGetInstanceProcAddr:
    //   - 根据函数名字符串返回函数指针
    //   - 如果扩展不存在，返回 NULL
    //
    // PFN_ 前缀说明：
    //   - PFN = Pointer to FuNction（Vulkan 命名约定）
    //   - 所有 Vulkan 函数指针类型都有 PFN_ 前缀
    //   - 例如：PFN_vkCreateDebugUtilsMessengerEXT
    //
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        vkContext->instance, "vkCreateDebugUtilsMessengerEXT");
    // 检查扩展是否可用（可选扩展可能不存在）
    if (vkCreateDebugUtilsMessengerEXT)
    {
        // 配置调试消息接收器
        VkDebugUtilsMessengerCreateInfoEXT debug = {};
        debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        // messageSeverity: 控制接收哪些严重级别的消息
        //   - ERROR: 错误（必须处理）
        //   - WARNING: 警告（建议修复）
        //   - INFO: 信息（可选）
        //   - VERBOSE: 详细诊断（开发调试用）
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        // messageType: 控制接收哪些类型的消息
        //   - GENERAL: 通用消息（非规范相关的）
        //   - VALIDATION: 规范验证错误（违反 Vulkan 规范）
        //   - PERFORMANCE: 性能警告（非最优用法）
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        // 设置回调函数指针
        //
        // 为什么需要强制转换？
        // ----------------------
        // 1. PFN_vkDebugUtilsMessengerCallbackEXT 是 Vulkan 定义的函数指针类型：
        //    typedef VkBool32 (VKAPI_PTR *PFN_vkDebugUtilsMessengerCallbackEXT)(...);
        //
        // 2. VKAPI_PTR 在 Windows 上展开为 __stdcall（与 VKAPI_CALL 一致）
        // 3. 但 MSVC 编译器推断函数指针时可能使用 __cdecl（C++ 默认）
        // 4. __stdcall 和 __cdecl 的调用约定不同，编译器认为类型不兼容
        // 5. 因此需要显式转换告诉编译器："我知道这是安全的，请转换"
        //
        // 这是 Windows + MSVC 特有的问题，GCC/Clang 通常不需要
        debug.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)vk_debug_callback;

        // 创建调试消息接收器
        // 参数说明：
        //   1. instance: Vulkan 实例
        //   2. &debug: 创建信息结构体
        //   3. 0: 分配器（NULL = 使用默认分配器）
        //   4. &debugMessenger: 输出参数，返回 messenger 句柄
        vkCreateDebugUtilsMessengerEXT(vkContext->instance, &debug, 0, &vkContext->debugMessenger);
    }
    // 注意：即使调试扩展加载失败也不应该阻止程序运行
    // 调试功能是可选的，失败时只打印警告，继续执行

    // create surface
    {
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(
            vkCreateWin32SurfaceKHR(vkContext->instance, &surfaceInfo, 0, &vkContext->surface));
#elif LINUX_BUILD

#endif
    }

    // ============================================================================
    // 选择物理设备（GPU）和查找支持渲染的队列族
    // ============================================================================
    //
    // Vulkan 核心概念：
    //
    // 1. Physical Device（物理设备）
    //    - 代表系统中的 GPU（可能有多个：集显 + 独显）
    //    - VkPhysicalDevice 是 GPU 的句柄，不是创建对象，只是引用
    //
    // 2. Queue Family（队列族）
    //    - GPU 中不同类型的工作队列分组
    //    - 常见类型：Graphics（渲染）、Compute（计算）、Transfer（数据传输）
    //    - 同一族内的队列共享资源，不同族之间相对独立
    //
    // 3. 为什么需要检查 Surface 支持？
    //    - 不是所有队列都能向窗口渲染
    //    - 有些 GPU 的计算队列不支持图形输出
    //    - 必须找到既支持图形操作又能渲染到 surface 的队列族
    //
    // 4. Vulkan 两阶段查询模式（常见模式）
    //    - 第一次调用：获取数量（传 NULL 作为数组指针）
    //    - 第二次调用：获取数据（传实际数组地址）
    //
    // ============================================================================
    {
        // 初始化队列索引为无效值（-1）
        vkContext->graphicsIdx = -1;

        // 第一步：枚举系统中的所有 GPU
        uint32_t gpuCount = 0;
        VkPhysicalDevice gpus[10];

        // 两阶段查询：先获取 GPU 数量
        // vkEnumeratePhysicalDevices 参数：
        //   1. instance: Vulkan 实例
        //   2. pPhysicalDeviceCount: 输入/输出参数，返回 GPU 数量
        //   3. pPhysicalDevices: NULL 表示只查询数量
        VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &gpuCount, 0));

        // 第二次调用：获取所有 GPU 句柄
        // 现在 gpuCount 包含了实际数量，可以分配数组并获取数据
        VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &gpuCount, gpus));

        // 第二步：遍历每个 GPU，查找合适的队列族
        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];
            uint32_t queueFamilyCount = 0;

            // 获取该 GPU 的队列族数量
            VkQueueFamilyProperties queueProp[10];

            // 两阶段查询：先获取队列族数量
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);

            // 第二次调用：获取所有队列族的属性
            // VkQueueFamilyProperties 包含：
            //   - queueFlags: 该队列支持的操作类型（GRAPHICS, COMPUTE, TRANSFER）
            //   - queueCount: 该族的队列数量
            //   - timestampValidBits: 时间戳精度
            //   - minImageTransferGranularity: 图像传输粒度
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProp);

            // 第三步：遍历该 GPU 的所有队列族，查找支持 surface 的队列
            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                // 检查该队列族是否支持向 surface 渲染
                VkBool32 surfaceSupport = VK_FALSE;

                // vkGetPhysicalDeviceSurfaceSupportKHR 检查特定队列族能否在指定 surface 上渲染
                // 参数：
                //   1. physicalDevice: GPU 句柄
                //   2. queueFamilyIndex: 要检查的队列族索引
                //   3. surface: 窗口表面
                //   4. pSupported: 输出参数，VK_TRUE = 支持，VK_FALSE = 不支持
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkContext->surface,
                                                              &surfaceSupport));

                // 如果找到第一个支持 surface 的队列族，选择它
                if (surfaceSupport)
                {
                    vkContext->graphicsIdx = j; // 记录队列族索引
                    vkContext->gpu = gpu;       // 选择这个 GPU
                    break;                      // 跳出内层循环
                }
            }

            // 如果已找到合适的队列族，跳出外层循环
            // 注意：这里简化了逻辑，只选择第一个找到的
            // 实际项目中可能需要更复杂的策略（如选择独显而非集显）
            if (vkContext->graphicsIdx >= 0)
            {
                break;
            }
        }

        // 检查是否成功找到支持渲染的队列族
        if (vkContext->graphicsIdx < 0)
        {
            // 没有找到支持 surface 的队列族
            // 可能原因：
            //   - GPU 驱动不支持 Vulkan
            //   - 没有支持图形输出的队列
            //   - surface 创建失败
            return false;
        }
    }

    // ============================================================================
    // 创建逻辑设备（Logical Device）
    // ============================================================================
    //
    // Vulkan 两种设备概念：
    //
    // 1. Physical Device（物理设备）
    //    - 代表 GPU 硬件
    //    - 通过 vkEnumeratePhysicalDevices 查询
    //    - 不能直接使用，只用于查询信息
    //
    // 2. Logical Device（逻辑设备）
    //    - 应用程序与 GPU 交互的接口
    //    - 通过 vkCreateDevice 创建
    //    - 指定要使用的队列族和扩展
    //    - 类似于"打开设备"，获得操作权限
    //
    // 为什么需要逻辑设备？
    // - 允许同一个 GPU 被多个应用程序使用
    // - 每个应用可以创建不同的逻辑设备配置
    // - 提供隔离和资源管理
    //
    // ============================================================================
    {
        // 队列优先级：[0.0, 1.0] 范围内的浮点数
        // 当多个队列竞争资源时，驱动根据优先级调度
        // 1.0 = 最高优先级
        float queuePriority = 1.0f;

        // 配置要创建的队列
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkContext->graphicsIdx; // 使用之前找到的支持渲染的队列族
        queueInfo.queueCount = 1;                            // 创建 1 个队列（足够大多数应用使用）
        queueInfo.pQueuePriorities = &queuePriority;         // 设置队列优先级

        // 设备级扩展：在逻辑设备上启用的扩展
        //
        // VK_KHR_swapchain: 交换链扩展
        //   - 交换链（Swapchain）是用于显示图像的机制
        //   - 管理一系列图像缓冲区（前后缓冲）
        //   - 与显示同步（避免撕裂）
        //   - 这是渲染到窗口必需的扩展
        //
        char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        // 创建逻辑设备
        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.enabledExtensionCount = ArraySize(extensions); // 启用的扩展数量
        deviceInfo.ppEnabledExtensionNames = extensions;          // 扩展名称列表
        deviceInfo.pQueueCreateInfos = &queueInfo;                // 队列创建信息
        deviceInfo.queueCreateInfoCount = 1; // ⚠️ 关键：必须设置队列创建信息数量！

        // vkCreateDevice: 创建逻辑设备
        // 参数：
        //   1. physicalDevice: 要使用的 GPU
        //   2. pCreateInfo: 创建信息（队列、扩展、特性等）
        //   3. pAllocator: 内存分配器（NULL = 使用默认）
        //   4. pDevice: 输出参数，返回逻辑设备句柄
        VK_CHECK(vkCreateDevice(vkContext->gpu, &deviceInfo, 0, &vkContext->device));

        // 从逻辑设备获取队列句柄
        //
        // 为什么需要单独获取？
        // - 队列是在设备创建时自动创建的
        // - 但需要获取句柄才能提交命令
        // - 同一个队列族可以创建多个队列，需要指定索引
        //
        // vkGetDeviceQueue 参数：
        //   1. device: 逻辑设备
        //   2. queueFamilyIndex: 队列族索引（我们之前选择的 graphicsIdx）
        //   3. queueIndex: 队列在族内的索引（我们只创建了 1 个，所以是 0）
        //   4. pQueue: 输出参数，返回队列句柄
        vkGetDeviceQueue(vkContext->device, vkContext->graphicsIdx, 0, &vkContext->graphicsQueue);
    }

    // ============================================================================
    // 创建交换链（Swapchain）
    // ============================================================================
    //
    // 什么是交换链？
    // --------------
    // 交换链是 Vulkan 用于显示图像的机制，管理一系列图像缓冲区。
    //
    // 类比：传统动画制作
    // - 画家在多张透明纸上画连续的动作
    // - 快速翻动纸张，产生动画效果
    // - 交换链就是这些"纸张"的管理器
    //
    // 为什么需要交换链？
    // ------------------
    // 1. 避免撕裂（Screen Tearing）
    //    - 撕裂：显示器正在显示图像时，GPU 又更新了内容
    //    - 交换链确保只在显示器刷新时切换图像（垂直同步）
    //
    // 2. 提高性能
    //    - GPU 渲染到下一帧缓冲区的同时，显示器显示当前帧
    //    - 并行工作，提高帧率
    //
    // 3. 灵活的图像数量
    //    - 双缓冲（2 张图像）：标准配置
    //    - 三缓冲（3 张图像）：更高帧率，但延迟稍高
    //
    // 工作流程：
    // ---------
    // GPU 渲染到 Image A → GPU 渲染到 Image B → 显示 Image A → 显示 Image B → 循环...
    //    (后台缓冲)              (后台缓冲)            (前台缓冲)      (前台缓冲)
    //
    // ============================================================================
    {
        // ----------------------------------------------------------------------
        // 第一步：查询 surface 支持的图像格式
        // ----------------------------------------------------------------------
        //
        // 为什么需要查询？
        // - 不同显示器/窗口系统支持不同的像素格式
        // - 常见格式：
        //   * VK_FORMAT_B8G8R8_SRGB: 8位 BGR，sRGB 颜色空间（Windows 常用）
        //   * VK_FORMAT_R8G8B8_SRGB: 8位 RGB，sRGB 颜色空间
        //
        // VkSurfaceFormatKHR 包含：
        //   - format: 像素格式（颜色通道顺序和位深）
        //   - colorSpace: 颜色空间（sRGB, HDR 等）
        //
        uint32_t formatCount = 0;
        VkSurfaceFormatKHR surfaceFormats[10];

        // 两阶段查询：先获取支持的格式数量
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext->gpu, vkContext->surface,
                                                      &formatCount, 0));

        // 第二次调用：获取所有支持的格式
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext->gpu, vkContext->surface,
                                                      &formatCount, surfaceFormats));

        // 选择合适的格式
        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            // VK_FORMAT_B8G8R8_SRGB: Windows 标准格式
            // B8G8R8: 蓝 8位，绿 8位，红 8位（注意顺序是 BGR）
            // SRGB: sRGB 颜色空间，适合显示（经过 gamma 校正）
            if (format.format == VK_FORMAT_B8G8R8_SRGB)
            {
                vkContext->surfaceFormat = format;
                break;
            }
        }

        // ----------------------------------------------------------------------
        // 第二步：查询 surface 能力（capabilities）
        // ----------------------------------------------------------------------
        //
        // VkSurfaceCapabilitiesKHR 包含：
        //   - minImageCount / maxImageCount: 支持的图像数量范围
        //   - currentExtent: 当前窗口大小（理想图像尺寸）
        //   - minImageExtent / maxImageExtent: 支持的图像尺寸范围
        //   - maxImageArrayLayers: 支持的图层数（立体渲染需要 >1）
        //   - supportedTransforms: 支持的图像变换（旋转、翻转等）
        //   - currentTransform: 当前变换（通常为无变换）
        //
        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkContext->gpu, vkContext->surface,
                                                           &surfaceCaps));

        // ----------------------------------------------------------------------
        // 第三步：计算图像数量
        // ----------------------------------------------------------------------
        //
        // 为什么是 minImageCount + 1？
        // - minImageCount: 最低要求（通常为 2，双缓冲）
        // - +1: 额外一张图像，让 GPU 可以提前开始下一帧
        //   避免等待显示器完成当前帧
        //
        // 举例：
        //   minImageCount = 2（双缓冲）
        //   我们使用 3 张图像（三缓冲）
        //   GPU 可以同时渲染、等待显示、正在显示
        //
        uint32_t imgCount = surfaceCaps.minImageCount + 1;

        // 确保不超过最大值
        // maxImageCount = 0 表示没有限制（除了内存）
        if (surfaceCaps.maxImageCount > 0 && imgCount > surfaceCaps.maxImageCount)
        {
            imgCount = surfaceCaps.maxImageCount;
        }

        // ----------------------------------------------------------------------
        // 第四步：创建交换链
        // ----------------------------------------------------------------------
        VkSwapchainCreateInfoKHR scCreateInfo = {};
        scCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        // surface: 要关联的窗口表面
        scCreateInfo.surface = vkContext->surface;

        // minImageCount: 交换链中的图像数量
        scCreateInfo.minImageCount = imgCount;

        // imageFormat: 图像的像素格式（使用之前选择的格式）
        scCreateInfo.imageFormat = vkContext->surfaceFormat.format;

        // imageColorSpace: 颜色空间（包含在 surfaceFormat 中）
        scCreateInfo.imageColorSpace = vkContext->surfaceFormat.colorSpace;

        // imageExtent: 图像尺寸（使用窗口当前尺寸）
        scCreateInfo.imageExtent = surfaceCaps.currentExtent;

        // imageArrayLayers: 图层数量
        // 1 = 普通 2D 渲染
        // 2+ = 立体渲染（VR、3D 电影等需要）
        scCreateInfo.imageArrayLayers = 1;

        // imageUsage: 图像用途标志
        // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT: 用作颜色附件（渲染目标）
        // 其他选项：
        //   - VK_IMAGE_USAGE_TRANSFER_DST_BIT: 可以作为复制目标（用于后处理）
        //   - VK_IMAGE_USAGE_STORAGE_BIT: 可以作为存储图像（计算着色器）
        scCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // preTransform: 图像变换
        // surfaceCaps.currentTransform: 当前窗口的变换（通常为无变换）
        // 如果窗口旋转了（如移动设备），需要相应变换图像
        scCreateInfo.preTransform = surfaceCaps.currentTransform;

        // compositeAlpha: Alpha 合成模式
        // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR: 不透明（窗口不透明）
        // 其他选项：
        //   - VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR: 预乘 Alpha
        //   - VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR: 后乘 Alpha
        scCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // 创建交换链
        // vkCreateSwapchainKHR 参数：
        //   1. device: 逻辑设备
        //   2. pCreateInfo: 创建信息（上面配置的所有参数）
        //   3. pAllocator: 内存分配器
        //   4. pSwapchain: 输出参数，返回交换链句柄
        VK_CHECK(vkCreateSwapchainKHR(vkContext->device, &scCreateInfo, 0, &vkContext->swapChain));

        // ----------------------------------------------------------------------
        // 第五步：获取交换链图像句柄
        // ----------------------------------------------------------------------
        //
        // 为什么需要再次获取？
        // - 创建交换链时，Vulkan 自动创建了图像
        // - 但我们需要获取这些图像的句柄才能渲染到它们
        //
        // VkImage: 代表 GPU 内存中的图像资源
        // 类比：画布的句柄，我们需要知道画布在哪里才能画画
        //
        VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device, vkContext->swapChain,
                                         &vkContext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device, vkContext->swapChain,
                                         &vkContext->scImgCount, vkContext->scImages));

        // 现在 vkContext->scImages[] 数组包含了所有交换链图像的句柄
        // 后续渲染时会使用这些图像作为渲染目标
    }
    
    return true;
}