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

Mesh mesh;
Mesh mesh2;
Mesh mesh3;

void tick(float deltaTime){
	cout << "FPS: " << to_string((int)(1/deltaTime)) << endl;
	mesh.setTransform(mesh.getTransform() * glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
	mesh2.setTransform(mesh2.getTransform() * glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
	mesh3.setTransform(mesh3.getTransform() * glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(280.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
}

int main() {
	mesh.vertices.push_back(Vertex({{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}}));
	mesh.vertices.push_back(Vertex({{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.f}}));
	mesh.vertices.push_back(Vertex({{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.f}}));
	mesh.vertices.push_back(Vertex({{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.f}}));
	mesh.triangles.push_back(Triangle({0,1,2}));
	mesh.triangles.push_back(Triangle({0,2,3}));

	mesh2.vertices.push_back(Vertex({{-0.7f, -0.7f}, {1.0f, 0.0f, 1.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{0.7f, -0.7f}, {1.0f, 1.0f, 0.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{0.7f, 0.7f}, {0.0f, 1.0f, 1.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{-0.7f, 0.7f}, {1.0f, 0.0f, 1.0f, 1.f}}));
	mesh2.triangles.push_back(Triangle({0,1,2}));
	mesh2.triangles.push_back(Triangle({0,2,3}));

	mesh3.vertices.push_back(Vertex({{-1.f, -1.f}, {1.0f, 0.0f, 1.0f, 1.f}}));
	mesh3.vertices.push_back(Vertex({{-0.9f, -1.f}, {1.0f, 1.0f, 0.0f, 1.f}}));
	mesh3.vertices.push_back(Vertex({{-1.f, -0.9f}, {0.0f, 1.0f, 1.0f, 1.f}}));
	mesh3.triangles.push_back(Triangle({0,1,2}));

	VisualServer vm;
	Ticker ticker;

	if(vm.init()){
		ticker.init();

		vm.addMesh(&mesh2);
		vm.addMesh(&mesh);
		vm.addMesh(&mesh3);

		while(vm.can_step()){
			ticker.step();
			tick(ticker.getDeltaTime());
			vm.step();
		}
	}

	vm.terminate();

	return 0;
}
