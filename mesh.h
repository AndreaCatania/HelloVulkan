#ifndef MESH_H
#define MESH_H

#include "hellovulkan.h"

class VisualServer;
class VulkanServer;
class Mesh;
class Texture;

// This struct is used to know handle the memory of mesh
class MeshHandle{
public:

	VulkanServer* vulkanServer;
	const Mesh *mesh;

	size_t verticesSize;
	VkDeviceSize verticesBufferOffset;
	VkBuffer vertexBuffer;
	VmaAllocation vertexAllocation;

	size_t indicesSize;
	VkDeviceSize indicesBufferOffset;
	VkBuffer indexBuffer;
	VmaAllocation indexAllocation;

	uint32_t meshUniformBufferOffset;
	bool hasTransformationChange;
	bool isInScene;

	VkDescriptorSet imageDescriptorSet;

	MeshHandle(VulkanServer *p_vulkanServer);
	~MeshHandle();

	void clear();
	bool prepare();
	bool allocateImagesDescriptorSet();
	void updateImages();
};

struct Vertex{
	glm::vec3 pos;
	glm::vec2 textCoord; // UV

	static VkVertexInputBindingDescription getBindingDescription(){
		VkVertexInputBindingDescription desc = {};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static array<VkVertexInputAttributeDescription, 2> getAttributesDescription(){
		array<VkVertexInputAttributeDescription, 2> attr;
		attr[0].binding = 0;
		attr[0].location = 0;
		attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attr[0].offset = offsetof(Vertex, pos);

		attr[1].binding = 0;
		attr[1].location = 2;
		attr[1].format = VK_FORMAT_R32G32_SFLOAT;
		attr[1].offset = offsetof(Vertex, textCoord);

		return attr;
	}
};

struct Triangle{
	uint32_t vertices[3];
};

class Mesh{
	friend class VulkanServer;
	friend class MeshHandle;

	static Texture* defaultTexture;

	unique_ptr<MeshHandle> meshHandle;
	glm::mat4 transformation;
	Texture* colorTexture;

public:
	vector<Vertex> vertices;
	vector<Triangle> triangles;

public:
	static Texture *getDefaultTeture(VulkanServer *v);

public:
	Mesh(VisualServer* p_visualServer);
	~Mesh();

	// Return the size in bytes of vertices
	const VkDeviceSize verticesSizeInBytes() const {
		return sizeof(Vertex) * vertices.size();
	}

	// return the size in bytes of triangles indices
	const VkDeviceSize indicesSizeInBytes() const {
		return sizeof(Triangle) * triangles.size();
	}

	const uint32_t getCountIndices() const{
		return triangles.size() * 3;
	}

	void setTransform(const glm::mat4 &p_transformation);
	const glm::mat4& getTransform() const {
		return transformation;
	}

	void setColorTexture(Texture* p_colorTexture);
	const Texture* getColorTexture() const { return colorTexture; }
};

#endif // MESH_H
