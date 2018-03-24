#ifndef MESH_H
#define MESH_H

#include "hellovulkan.h"

class VulkanServer;
class Mesh;
class Texture;

// This struct is used to know handle the memory of mesh
struct MeshHandle{
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
};

struct Vertex{
	glm::vec3 pos;
	glm::vec4 color;

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
		attr[1].location = 1;
		attr[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attr[1].offset = offsetof(Vertex, color);
		return attr;
	}
};

struct Triangle{
	uint32_t vertices[3];
};

class Mesh{
	friend class VulkanServer;
	unique_ptr<MeshHandle> meshHandle;
	glm::mat4 transformation;
	Texture* colorTexture;

public:
	vector<Vertex> vertices;
	vector<Triangle> triangles;

public:
	Mesh();
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
