#include <iostream>

#include "VisualServer.h"

#include <iostream>

class Ticker{

public:
	void init(){
		deltaTime = 0;
		previousTime = std::chrono::high_resolution_clock::now();
	}
	void step(){
		std::chrono::time_point<std::chrono::high_resolution_clock> currentTime = std::chrono::high_resolution_clock::now();
		deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime-previousTime).count();
		previousTime = currentTime;
	}
	float getDeltaTime(){return deltaTime;}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> previousTime;
	float deltaTime;
};

Ticker ticker;
VisualServer vm;

Mesh cubeMesh;
Mesh mesh2;
Mesh mesh3;

void cubeMaker(Mesh &mesh){
	mesh.vertices.push_back(Vertex({ { -1.0f, -1.0f,  1.0f },{ 1.0f, 0.0f, 0.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ {  1.0f, -1.0f,  1.0f },{ 1.0f, 0.0f, 0.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ {  1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 1.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ { -1.0f,  1.0f,  1.0f },{ 0.0f, 1.0f, 0.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ { -1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ {  1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ {  1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f, 1.f } }));
	mesh.vertices.push_back(Vertex({ { -1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f, 1.f } }));
	mesh.triangles.push_back(Triangle({0,1,2}));
	mesh.triangles.push_back(Triangle({2,3,0}));
	mesh.triangles.push_back(Triangle({1,5,6}));
	mesh.triangles.push_back(Triangle({6,2,1}));
	mesh.triangles.push_back(Triangle({7,6,5}));
	mesh.triangles.push_back(Triangle({5,4,7}));
	mesh.triangles.push_back(Triangle({4,0,3}));
	mesh.triangles.push_back(Triangle({3,7,4}));
	mesh.triangles.push_back(Triangle({4,5,1}));
	mesh.triangles.push_back(Triangle({1,0,4}));
	mesh.triangles.push_back(Triangle({3,2,6}));
	mesh.triangles.push_back(Triangle({6,7,3}));
}

void ready(){

	// Update camera view
	vm.getVulkanServer().getCamera().setNearFar(0.1, 100.);


	glm::mat4 camTransform;
	camTransform = glm::translate( glm::mat4(1.), glm::vec3(0., 0., 0.) );
	vm.getVulkanServer().getCamera().setTransform(camTransform);

	// Create cubes
	cubeMaker(cubeMesh);
	cubeMesh.setTransform( glm::translate(glm::mat4(1.), glm::vec3(4., 4., -10.)) );

	mesh2.vertices.push_back(Vertex({{0.f, 0.f, 0.2f}, {1.0f, 0.0f, 0.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{1.f, 0.f, 0.2f}, {1.0f, 0.0f, 0.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{0.f, 1.f, 0.2f}, {1.0f, 0.0f, 0.0f, 1.f}}));
	mesh2.triangles.push_back(Triangle({0,1,2}));

	mesh3.vertices.push_back(Vertex({{0.f, 0.f, 0.4f}, {1.0f, 0.0f, 0.0f, 1.f}}));
	mesh3.vertices.push_back(Vertex({{1.f, 0.f, 0.f}, {0.0f, 1.0f, 0.0f, 1.f}}));
	mesh3.vertices.push_back(Vertex({{0.f, 1.f, 0.f}, {0.0f, 0.0f, 1.0f, 1.f}}));
	mesh3.triangles.push_back(Triangle({0,1,2}));

	vm.addMesh(&cubeMesh);
	vm.addMesh(&mesh2);
	vm.addMesh(&mesh3);
}

void tick(float deltaTime){
	//cout << "FPS: " << to_string((int)(1/deltaTime)) << endl;

	//cubeMesh.setTransform(cubeMesh.getTransform() * glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	//mesh2.setTransform(mesh2.getTransform() * glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
	//mesh3.setTransform(mesh3.getTransform() * glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(280.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
}

int main() {

	if(vm.init()){
		ticker.init();
		ready();
		while(vm.can_step()){
			ticker.step();
			tick(ticker.getDeltaTime());
			vm.step();
		}
	}

	vm.terminate();
	return 0;
}
