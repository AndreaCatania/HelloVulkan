#include "mesh.h"

#include "hellovulkan.h"
#include "VisualServer.h"
#include "texture.h"

MeshHandle::MeshHandle(VulkanServer* p_vulkanServer)
	: vulkanServer(p_vulkanServer),
	  vertexBuffer(VK_NULL_HANDLE),
	  vertexAllocation(VK_NULL_HANDLE),
	  indexBuffer(VK_NULL_HANDLE),
	  indexAllocation(VK_NULL_HANDLE),
	  imageDescriptorSet(VK_NULL_HANDLE),
	  isInScene(false)
{}

MeshHandle::~MeshHandle(){
	clear();
}

void MeshHandle::clear(){
	vulkanServer->destroyBuffer(vulkanServer->bufferMemoryDeviceAllocator, indexBuffer, indexAllocation);
	vulkanServer->destroyBuffer(vulkanServer->bufferMemoryDeviceAllocator, vertexBuffer, vertexAllocation);
	if(VK_NULL_HANDLE!=imageDescriptorSet){
		vkFreeDescriptorSets(vulkanServer->device, vulkanServer->meshImagesDescriptorPool, 1, &imageDescriptorSet);
		imageDescriptorSet = VK_NULL_HANDLE;
	}
}

bool MeshHandle::prepare(){

	if( !vulkanServer->createBuffer(vulkanServer->bufferMemoryDeviceAllocator,
					  mesh->verticesSizeInBytes(),
					  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  VMA_MEMORY_USAGE_GPU_ONLY,
					  vertexBuffer,
					  vertexAllocation) ){

		print("[ERROR] Vertex buffer creation error, mesh not added");
		clear();
		return false;
	}

	if( !vulkanServer->createBuffer(vulkanServer->bufferMemoryDeviceAllocator,
					  mesh->indicesSizeInBytes(),
					  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  VMA_MEMORY_USAGE_GPU_ONLY,
					  indexBuffer,
					  indexAllocation) ){

		print("[ERROR] Index buffer creation error, mesh not added");
		clear();
		return false;
	}

	verticesSize = mesh->verticesSizeInBytes();
	verticesBufferOffset = 0;
	indicesSize = mesh->indicesSizeInBytes();
	indicesBufferOffset = 0;
	meshUniformBufferOffset = vulkanServer->meshUniformBufferData.count++;
	hasTransformationChange = true;

	if( !allocateImagesDescriptorSet() ){

		print("[ERROR] Mesh images allocation failed");
		clear();
	}

	isInScene = true;
	updateImages();

	return true;
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

	if(!isInScene)
		return;

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet writeDesc = {};
	writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDesc.dstSet = imageDescriptorSet;
	writeDesc.dstBinding = 0;
	writeDesc.descriptorCount = 1;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDesc.pImageInfo = &imageInfo;

	if(mesh->colorTexture){
		imageInfo.sampler = mesh->colorTexture->imageSampler;
		imageInfo.imageView = mesh->colorTexture->imageView;
	}else{
		imageInfo.sampler = Mesh::getDefaultTeture(vulkanServer)->imageSampler;
		imageInfo.imageView = Mesh::getDefaultTeture(vulkanServer)->imageView;
	}

	vkUpdateDescriptorSets(vulkanServer->device, 1, &writeDesc, 0, nullptr);
}

// TODO destroy it
Texture* Mesh::defaultTexture = nullptr;

Texture* Mesh::getDefaultTeture(VulkanServer* v){
	if(!defaultTexture){
		defaultTexture = new Texture(v);
		defaultTexture->load("assets/default.png");
	}
	return defaultTexture;
}

Mesh::Mesh(VisualServer* p_visualServer)
	: colorTexture( nullptr )
{
	meshHandle = unique_ptr<MeshHandle>(new MeshHandle(p_visualServer->getVulkanServer()));
	meshHandle->mesh = this;
	transformation = glm::mat4(1.f);
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
