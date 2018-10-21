#ifndef MESH_H
#define MESH_H

#include "hellovulkan.h"
#include "libs/tiny_obj_loader/tiny_obj_loader.h"

class OldVisualServer;
class VulkanServer;
class Mesh;
class Texture;

// This struct is used to know handle the memory of mesh
class MeshHandle {
public:
	VulkanServer *vulkanServer;
	Mesh *mesh;

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

	VkDescriptorSet imageDescriptorSet;

	MeshHandle(Mesh *p_mesh, VulkanServer *p_vulkanServer);
	~MeshHandle();

	void clear();
	bool prepare();
	bool allocateImagesDescriptorSet();
	void updateImages();
};

struct Vertex {
	glm::vec3 pos;
	glm::vec2 textCoord; // UV

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription desc = {};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributesDescription() {
		std::array<VkVertexInputAttributeDescription, 2> attr;
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

struct Triangle {
	uint32_t indices[3];
};

class Mesh {
	friend class VulkanServer;
	friend class MeshHandle;

	MeshHandle *meshHandle;

	Texture *colorTexture;
	glm::mat4 transformation;

public:
	std::vector<Vertex> vertices;
	std::vector<Triangle> triangles;

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

	const uint32_t getCountIndices() const {
		return triangles.size() * 3;
	}

	void setColorTexture(Texture *p_colorTexture);
	const Texture *getColorTexture() const { return colorTexture; }

	void setTransform(const glm::mat4 &p_transformation);
	const glm::mat4 &getTransform() const {
		return transformation;
	}

	int addUniqueTriangle(int p_lastIndex, const Vertex p_vertices[3]);

	// Load new vertices from OBJ file
	bool loadObj(const std::string &p_path);
};

#endif // MESH_H
