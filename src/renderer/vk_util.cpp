#include "defines.h"
#include "vk_types.h"


#include <string.h>

uint32_t vk_get_memory_type_index(VkPhysicalDevice gpu, VkMemoryRequirements memRequirements,
                                  VkMemoryPropertyFlags memProps)
{
    uint32_t typeIdx = INVALID_IDX;

    // 获取GPU所提供的内存需求
    VkPhysicalDeviceMemoryProperties gpuMemProps;
    vkGetPhysicalDeviceMemoryProperties(gpu, &gpuMemProps);

    // 过滤出GPU上适合图片的内存类型索引
    for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
    {
        if (memRequirements.memoryTypeBits & (1 << i) &&
            (gpuMemProps.memoryTypes[i].propertyFlags & memProps) == memProps)
        {
            typeIdx = i;
            break;
        }
    }

    CAKEZ_ASSERT(typeIdx != INVALID_IDX, "Failed to find proper type index for Memeory Properties");
    return typeIdx;
}

Image vk_allocate_image(VkDevice device, VkPhysicalDevice gpu, uint32_t width, uint32_t height,
                        VkFormat format)
{
    Image image = {};

    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent = {width, height, 1};
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VK_CHECK(vkCreateImage(device, &imgInfo, 0, &image.image));

    // 获取图片的内存需求
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image.image, &memRequirements);

    // 过滤出GPU本地内存类型索引（vram)
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.memoryTypeIndex =
        vk_get_memory_type_index(gpu, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VK_CHECK(vkAllocateMemory(device, &allocInfo, 0, &image.memory));
    VK_CHECK(vkBindImageMemory(device, image.image, image.memory, 0));

    return image;
}

// 往buffer写数据的逻辑顺序：
// 创建Buffer(元数据) -> Bind内存 -> Map内存拿到指针 -> memcpy数据
Buffer vk_allocate_buffer(VkDevice device, VkPhysicalDevice gpu, uint32_t size,
VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memProps)
{
    // 这里确保每次都是一个新的对象（从而包括多个vkDeviceMemory, vkBuffer等）
    Buffer buffer = {};
    buffer.size = size;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = bufferUsage;
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, 0, &buffer.buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vk_get_memory_type_index(gpu, memRequirements, memProps);
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VK_CHECK(vkAllocateMemory(device, &allocInfo, 0, &buffer.memory));

    // only map memory the CPU can write
    if (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) 
    {
        // 将GPU端的虚拟内存地址(Device Memory)映射到CPU端的虚拟内存地址空间(Host Address)
        // 建立的是 CPU 虚拟地址 <--> GPU 内存（Device Memory） 之间的映射。
        VK_CHECK(vkMapMemory(device, buffer.memory, 0, memRequirements.size, 0, &buffer.data));
    }

    // 将Buffer对象与GPU的DeviceMemory建立映射关系
    // VkBuffer 对象 <--> GPU 内存（Device Memory） 之间的映射。
    VK_CHECK(vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0));
    return buffer;
}

void vk_copy_to_buffer(Buffer* buffer, void* data, uint32_t size)
{
    CAKEZ_ASSERT(buffer->size >= size, "Buffer too small %d for data %d", buffer->size, size);

    if (buffer->data)
    {
        memcpy(buffer->data, data, size);
    }
    else 
    {
        //todo : implement, copy data using command buffer
    }

}


void vk_upload_dds(VkDevice device, VkPhysicalDevice gpu, VkCommandPool commandPool,
    VkQueue graphicsQueue, Buffer* buffer, char* ddsFileName, Image* image )
{
    uint32_t fileSize;
        DDSFile* ddsFile = (DDSFile*)platform_read_file(ddsFileName, &fileSize);
        uint32_t textureSize = ddsFile->header.Width * ddsFile->header.Height * 4;
        vk_copy_to_buffer(buffer, &ddsFile->dataBegin, textureSize);
        *image =vk_allocate_image(device, gpu, ddsFile->header.Width,
                              ddsFile->header.Height, VK_FORMAT_B8G8R8A8_SRGB);
        

        //      command
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAlloc = cmd_alloc_info(commandPool);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAlloc, &cmd));
        VkCommandBufferBeginInfo beginInfoFire = cmd_begin_info();
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfoFire));

        //      range
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.layerCount = 1;
        range.levelCount = 1;

        //      barrier
        VkImageMemoryBarrier imgMemBarrier = {};
        imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgMemBarrier.image = image->image;
        imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemBarrier.subresourceRange = range; // 共用

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &imgMemBarrier);

        VkBufferImageCopy copyRegionFire = {};
        copyRegionFire.imageExtent = {ddsFile->header.Width, ddsFile->header.Height, 1};
        copyRegionFire.imageSubresource.layerCount = 1;
        copyRegionFire.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdCopyBufferToImage(cmd, buffer->buffer, image->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegionFire);

        imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMemBarrier.newLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 准备好给shader读取了
        imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1,
                             &imgMemBarrier);

        VK_CHECK(vkEndCommandBuffer(cmd));
        
        VkFence uploadFence;
        VkFenceCreateInfo fenceInfo = fence_info();
        VK_CHECK(vkCreateFence(device, &fenceInfo, 0, &uploadFence));
        
        VkSubmitInfo submitInfo = submit_info(&cmd);
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadFence));
        VK_CHECK(vkWaitForFences(device, 1, &uploadFence, true, UINT64_MAX));
        vkDestroyFence(device, uploadFence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

void vk_create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageView* view)
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VK_CHECK(vkCreateImageView(device, &viewInfo, 0, view));
}