#include "mesh.h"

#include "VisualServer.h"
#include "texture.h"

MeshHandle::MeshHandle(VulkanServer* p_vulkanServer)
	: vulkanServer(p_vulkanServer),
	  imageDescriptorSet(VK_NULL_HANDLE)
{}

MeshHandle::~MeshHandle(){

}

void MeshHandle::clear(){
	if(VK_NULL_HANDLE!=imageDescriptorSet){
		vkFreeDescriptorSets(vulkanServer->device, vulkanServer->meshImagesDescriptorPool, 1, &imageDescriptorSet);
		imageDescriptorSet = VK_NULL_HANDLE;
	}
}

bool MeshHandle::allocateImagesDescriptorSet(){
	// Allocate dynamic buffer of meshes
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vulkanServer->meshImagesDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vulkanServer->meshImagesDescriptorSetLayout;

	VkResult res = vkAllocateDescriptorSets(vulkanServer->device, &allocInfo, &imageDescriptorSet );
	if( res!=VK_SUCCESS ){
		return false;
	}
	return true;
}

void MeshHandle::updateImages(){

	if(!mesh->colorTexture)
		return;

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = mesh->colorTexture->imageSampler;
	imageInfo.imageView = mesh->colorTexture->imageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Set image
	VkWriteDescriptorSet writeDesc = {};
	writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDesc.dstSet = imageDescriptorSet;
	writeDesc.dstBinding = 0;
	writeDesc.descriptorCount = 1;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDesc.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(vulkanServer->device, 1, &writeDesc, 0, nullptr);
}

Mesh::Mesh(VisualServer* p_visualServer)
	: colorTexture( nullptr )
{
	meshHandle = unique_ptr<MeshHandle>(new MeshHandle(p_visualServer->getVulkanServer()));
	meshHandle->mesh = this;
	transformation = glm::mat4(1.f);
	meshHandle->allocateImagesDescriptorSet();
}

Mesh::~Mesh(){}

void Mesh::setTransform(const glm::mat4 &p_transformation){
	transformation = p_transformation;
	meshHandle->hasTransformationChange = true;
}

void Mesh::setColorTexture(Texture* p_colorTexture){
	colorTexture = p_colorTexture;
	meshHandle->updateImages();
}
