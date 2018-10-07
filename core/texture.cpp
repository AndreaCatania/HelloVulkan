#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb/stb_image.h"

#include "VisualServer.h"

Texture::Texture(VisualServer *p_visualServer) :
		Texture(p_visualServer->getVulkanServer()) {}

Texture::Texture(VulkanServer *p_vulkanServer) :
		vulkanServer(p_vulkanServer),
		image(VK_NULL_HANDLE),
		imageMemory(VK_NULL_HANDLE),
		imageView(VK_NULL_HANDLE),
		imageSampler(VK_NULL_HANDLE),
		channels_of_image(4) // RGB Alpha
{}

Texture::~Texture() {
	clear();
}

bool Texture::load(const string &p_path) {

	clear();

	VkBuffer imageBuffer;
	VmaAllocation allocation;
	VmaAllocator allocator;

	// Load image inside buffer
	{
		int real_channels_of_image;
		unsigned char *imageData = stbi_load(p_path.c_str(), &width, &height, &real_channels_of_image, channels_of_image);

		if (nullptr == imageData) {
			print("[ERROR] Image file can't be loaded, may be corrupted or extension not supported: " + p_path);
			return false;
		}

		VkDeviceSize size = width * height * channels_of_image;

		// Load inside buffer
		if (!vulkanServer->createImageLoadBuffer(size, imageBuffer, allocation, allocator)) {
			print("[ERROR] Failed to create load buffer during image loading");
			stbi_image_free(imageData);
			return false;
		}

		void *data;
		vmaMapMemory(allocator, allocation, &data);
		memcpy(data, imageData, size);
		vmaUnmapMemory(allocator, allocation);

		stbi_image_free(imageData);
	}

	bool success = false;
	// Create image
	if (vulkanServer->createImageTexture(width, height, image, imageMemory)) {

		// Create image view
		if (vulkanServer->createImageViewTexture(image, imageView)) {

			if (vulkanServer->transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {

				// fill image
				VkCommandBuffer commandBuffer;
				if (vulkanServer->allocateCommand(commandBuffer)) {
					vulkanServer->beginOneTimeCommand(commandBuffer);

					VkBufferImageCopy region = {};
					region.bufferOffset = 0;
					region.bufferRowLength = 0;
					region.bufferImageHeight = 0;
					region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					region.imageSubresource.mipLevel = 0;
					region.imageSubresource.baseArrayLayer = 0;
					region.imageSubresource.layerCount = 1;
					region.imageOffset = { 0, 0, 0 };
					region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

					vkCmdCopyBufferToImage(commandBuffer,
							imageBuffer,
							image,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							1,
							&region);

					if (vulkanServer->endCommand(commandBuffer)) {
						if (vulkanServer->submitWaitCommand(commandBuffer)) {
							if (vulkanServer->transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
								if (_createSampler()) {
									success = true;
								}
							}
						}
					}
					vulkanServer->freeCommand(commandBuffer);
				}
			}
		}
	}

	// Destroy support buffer
	vulkanServer->destroyBuffer(allocator, imageBuffer, allocation);
	if (!success) {
		// cleanup in case of errors
		clear();
	}
	return success;
}

bool Texture::_createSampler() {

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.maxAnisotropy = 16;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	return VK_SUCCESS == vkCreateSampler(vulkanServer->device, &samplerCreateInfo, nullptr, &imageSampler);
}

void Texture::clear() {
	if (VK_NULL_HANDLE != imageSampler) {
		vkDestroySampler(vulkanServer->device, imageSampler, nullptr);
		imageSampler = VK_NULL_HANDLE;
	}
	if (VK_NULL_HANDLE != imageView) {
		vulkanServer->destroyImageView(imageView);
	}
	if (VK_NULL_HANDLE != image) {
		vulkanServer->destroyImage(image, imageMemory);
	}
}
