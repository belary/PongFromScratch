#pragma once

#include <vulkan/vulkan.h>

// ============================================================================
// 图像资源包装结构体
// ============================================================================
//
// Vulkan 中的图像资源涉及多个对象，需要分别管理：
// - VkImage: 图像对象本身（描述格式、尺寸等）
// - VkImageView: 图像视图（定义如何访问图像，如 mipmap 层级）
// - VkDeviceMemory: 绑定的内存（实际存储像素数据）
//
// 这个结构体将三者组合在一起，方便统一管理
//
struct Image
{
    VkImage image; // 图像对象：Vulkan 的图像资源句柄
                   // 定义了图像的格式、尺寸、用途等
                   // 但不包含实际的像素数据
                   //
                   // 类比：相机的"照片规格"（尺寸、格式等）

    VkImageView view; // 图像视图：定义如何访问图像
                      // 指定要访问图像的哪个部分：
                      // - mipmap 层级
                      // - 数组层
                      // - 颜色通道（R/G/B/A 或深度/模板）
                      //
                      // 类比：照片的"裁剪框"（只看某一部分）

    VkDeviceMemory memory; // 设备内存：绑定到图像的 GPU 内存
                           // 存储实际的像素数据
                           // 必须满足图像的内存需求（大小、对齐等）
                           //
                           // 类比：相机的"存储卡"（实际保存照片的地方）
                           //
                           // 注意：一个内存可以绑定多个图像（通过偏移）
};

// ============================================================================
// 缓冲区资源包装结构体
// ============================================================================
//
// Vulkan 中的缓冲区资源涉及多个对象，需要分别管理：
// - VkBuffer: 缓冲区对象本身（描述大小、用途等）
// - VkDeviceMemory: 绑定的内存（实际存储数据）
// - void*: 映射后的 CPU 指针（用于写入数据）
//
// 这个结构体将三者组合在一起，方便统一管理
//
struct Buffer
{
    VkBuffer buffer; // 缓冲区对象：Vulkan 的缓冲区资源句柄
                     // 定义了缓冲区的大小、用途等
                     // 用途包括：顶点数据、索引数据、Uniform、Staging 等
                     //
                     // 常见用途标志：
                     // - VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: 顶点数据
                     // - VK_BUFFER_USAGE_INDEX_BUFFER_BIT: 索引数据
                     // - VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT: Uniform 缓冲
                     // - VK_BUFFER_USAGE_TRANSFER_SRC_BIT: 传输源（Staging）
                     //
                     // 类比：快递箱的"规格标签"（大小、用途等）

    VkDeviceMemory memory; // 设备内存：绑定到缓冲区的 GPU 内存
                           // 存储缓冲区的实际数据
                           // 必须满足缓冲区的内存需求
                           //
                           // 类比：快递箱的"存储空间"（实际放东西的地方）
                           //
                           // 注意：一个内存可以绑定多个缓冲区（通过偏移）

    void* data; // CPU 映射指针：用于 CPU 访问缓冲区内容
                // 通过 vkMapMemory 获取
                // 只对 HOST_VISIBLE 内存有效
                //
                // 使用场景：
                // - Staging Buffer: CPU 写入数据 → GPU 复制
                // - Dynamic Uniform: CPU 每帧更新 Uniform 数据
                //
                // 类比：快递箱的"钥匙"（CPU 可以打开并放入东西）
                //
                // 注意：
                // - 不是所有缓冲区都有 data（DEVICE_LOCAL 内存无法映射）
                // - 写入后可能需要 vkFlushMappedMemoryRanges（非 COHERENT）
};
