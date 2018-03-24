#include "mesh.h"

#include "hellovulkan.h"
#include "VisualServer.h"
#include "texture.h"

MeshHandle::MeshHandle(Mesh* p_mesh, VulkanServer* p_vulkanServer)
	: mesh(p_mesh),
	  vulkanServer(p_vulkanServer),
	  vertexBuffer(VK_NULL_HANDLE),
	  vertexAllocation(VK_NULL_HANDLE),
	  indexBuffer(VK_NULL_HANDLE),
	  indexAllocation(VK_NULL_HANDLE),
	  imageDescriptorSet(VK_NULL_HANDLE)
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
		return false;
	}

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
		imageInfo.sampler = vulkanServer->visualServer->getDefaultTeture()->imageSampler;
		imageInfo.imageView = vulkanServer->visualServer->getDefaultTeture()->imageView;
	}

	vkUpdateDescriptorSets(vulkanServer->device, 1, &writeDesc, 0, nullptr);
}

Mesh::Mesh()
	: colorTexture( nullptr )
	, meshHandle(nullptr)
	, transformation(1.f)
{}

Mesh::~Mesh(){}

void Mesh::setColorTexture(Texture* p_colorTexture){
	colorTexture = p_colorTexture;
	if(meshHandle)
		meshHandle->updateImages();
}

void Mesh::setTransform(const glm::mat4 &p_transformation){
	transformation = p_transformation;
	if(meshHandle)
		meshHandle->hasTransformationChange = true;
}

