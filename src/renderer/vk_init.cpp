// Vulkan 初始化辅助函数
// 此文件提供用于初始化 Vulkan 对象的工具函数

#include <vulkan/vulkan.h>

/**
 * 创建命令缓冲区开始信息结构
 *
 * 此函数配置 Vulkan 命令缓冲区的开始参数，常用于：
 * - 立即提交的命令（如资源初始化、拷贝等一次性操作）
 * - 不需要重录制的临时命令
 *
 * 使用场景示例：
 * - 初始化阶段上传纹理数据到 GPU 显存
 * - 拷贝顶点/索引数据到缓冲区
 * - 执行图像布局转换（Image Layout Transition）
 * - 其他只需要执行一次的临时操作
 *
 * @return 预配置的 VkCommandBufferBeginInfo 结构体
 */
VkCommandBufferBeginInfo cmd_begin_info() {
    // 初始化结构体为零（所有字段默认为 0）
    VkCommandBufferBeginInfo info = {};

    // 标识此结构体的类型（Vulkan 安全机制，用于版本控制）
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // 设置命令缓冲区使用标志：
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 表示此命令缓冲区：
    //  1. 只会被录制一次
    //  2. 执行后不会重用
    //  3. 适用于一次性操作（如初始资源上传、布局转换等）
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    return info;
}