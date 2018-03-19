#include <iostream>

#include "VisualServer.h"

#include <iostream>

#include "libs/glm/gtc/random.hpp"

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

vector<Mesh*> meshes;
glm::mat4 cameraBoom;
float cameraBoomLenght = 20;

void ready(){

	// Update camera view
	vm.getVulkanServer().getCamera().setNearFar(0.1, 100.);

	cameraBoom = glm::mat4(1.);

	meshes.resize(50);

	float ballRadius = 20.;

	// Create cubes
	for(int i = meshes.size()-1; 0<=i; --i){
		meshes[i] = new Mesh;
		cubeMaker(*meshes[i]);
		meshes[i]->setTransform( glm::translate(glm::mat4(1.), glm::ballRand(ballRadius)) );
		vm.addMesh(meshes[i]);
	}
}

void exit(){
	for(int i = meshes.size()-1; 0<=i; --i){
		delete meshes[i];
	}
}

void tick(float deltaTime){
	cout << "FPS: " << to_string((int)(1/deltaTime)) << endl;

	Camera& cam = vm.getVulkanServer().getCamera();
	glm::mat4 camTransform( glm::translate( glm::mat4(1.), glm::vec3(0., 0., cameraBoomLenght) ) );
	cameraBoom = glm::rotate(cameraBoom, deltaTime * glm::radians(20.f), glm::vec3(0,1,0));
	cam.setTransform( cameraBoom * camTransform );

	for(int i = meshes.size()-1; 0<=i; --i){
		meshes[i]->setTransform(glm::rotate(meshes[i]->getTransform(), deltaTime * glm::radians(90.0f), glm::vec3(1.0f, .0f, .0f)));
	}
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
		exit();
	}

	vm.terminate();
	return 0;
}
