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
    // uint32_t fileSize;
    //     DDSFile* data = (DDSFile*)platform_read_file("assets/textures/cakez.DDS", &fileSize);
    //     uint32_t textureSize = data->header.Width * data->header.Height * 4;
    //     memcpy(vkContext->stagingBuffer.data, &data->dataBegin, textureSize);

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

    // 过滤出GPU上适合图片的内存类型索引
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.memoryTypeIndex =
        vk_get_memory_type_index(gpu, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VK_CHECK(vkAllocateMemory(device, &allocInfo, 0, &image.memory));
    VK_CHECK(vkBindImageMemory(device, image.image, image.memory, 0));

    return image;
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