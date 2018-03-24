#ifndef TEXTURE_H
#define TEXTURE_H

#include "hellovulkan.h"

class VisualServer;
class VulkanServer;

class Texture{

	VulkanServer* vulkanServer;
	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;
	VkSampler imageSampler;
	VkDescriptorSet descriptorSet;

	int width;
	int height;
	int channels_of_image;

	bool ready;

public:
	Texture(VisualServer* p_visualServer);
	~Texture();
	bool load(const string &p_path);

	bool isReady() const { return ready; }
	const VkDescriptorSet getDescriptorSet() const {return descriptorSet;}

private:
	bool _createSampler();
	void clear();

	bool allocateConfigureDescriptorSet();
};

#endif // TEXTURE_H
