#include "mesh.h"

#include "hellovulkan.h"
#include "VisualServer.h"
#include "texture.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "libs/tiny_obj_loader/tiny_obj_loader.h"

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

int Mesh::addUniqueTriangle(int p_lastIndex, const Vertex p_vertices[3]){
	vertices.push_back(p_vertices[0]);
	vertices.push_back(p_vertices[1]);
	vertices.push_back(p_vertices[2]);

	triangles.push_back(Triangle({++p_lastIndex, ++p_lastIndex, ++p_lastIndex}));
	return p_lastIndex;
}

// Utility loadObj
void set_vertex_position(Vertex& r_vertex, tinyobj::index_t& p_index, tinyobj::attrib_t p_attributes){
	r_vertex.pos = {
		p_attributes.vertices[(p_index.vertex_index*3)+0],
		p_attributes.vertices[(p_index.vertex_index*3)+1],
		p_attributes.vertices[(p_index.vertex_index*3)+2]};
}

// Utility loadObj
void set_vertex_uv(Vertex& r_vertex, tinyobj::index_t& p_index, tinyobj::attrib_t p_attributes){
	if( -1<p_index.texcoord_index ){
		// Since vulkan texture coords start from  top left corner and obj format start from bottom left
		// I need invert it using "1. - p_attributes.texcoords[(p_index.texcoord_index*2)+1]" to make it correct
		r_vertex.textCoord = {p_attributes.texcoords[(p_index.texcoord_index*2)+0],
							  1. - p_attributes.texcoords[(p_index.texcoord_index*2)+1]};
	}else{
		r_vertex.textCoord = {0, 0};
	}
}

bool Mesh::loadObj(const string &p_path){
	tinyobj::attrib_t attrib;
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	string err;

	if( !tinyobj::LoadObj(&attrib, &shapes, &materials, &err, p_path.c_str()) ){
		print(string("[ERROR] Model loading failed: ") + err);
		return false;
	}

	int lastIndex = -1;
	for(int i = shapes.size() -1; 0<=i; --i){ // Each shape

		vector<tinyobj::index_t> &lIndices = shapes[i].mesh.indices; // Contains index of UV, vertex position, normal
		const size_t indexCount(lIndices.size());

		for( int j = 0; j<indexCount; j += 3){ // Each triangle of shape

			int vertexId = j+0;
			Vertex vert[3];

			set_vertex_position(vert[0], lIndices[vertexId], attrib);
			set_vertex_uv(vert[0], lIndices[vertexId], attrib);

			++vertexId;
			set_vertex_position(vert[1], lIndices[vertexId], attrib);
			set_vertex_uv(vert[1], lIndices[vertexId], attrib);

			++vertexId;
			set_vertex_position(vert[2], lIndices[vertexId], attrib);
			set_vertex_uv(vert[2], lIndices[vertexId], attrib);

			lastIndex = addUniqueTriangle(lastIndex, vert);
		}
	}

	return true;
}




