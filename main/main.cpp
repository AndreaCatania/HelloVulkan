
#include "main.h"

#include "core/VisualServer.h"
#include "core/mesh.h"
#include "core/texture.h"
#include "libs/glm/gtc/random.hpp"
#include "modules/glfw/glfw_window_server.h"

#define CLOUDY_CUBES_TEST 1
#define TEXTURE_TEST 0
#define LOAD_TEST 0

void print(string c) {
	cout << c << endl;
}

void print(const char *c) {
	cout << c << endl;
}

class Ticker {

public:
	void init() {
		deltaTime = 0;
		previousTime = std::chrono::high_resolution_clock::now();
	}
	void step() {
		std::chrono::time_point<std::chrono::high_resolution_clock> currentTime = std::chrono::high_resolution_clock::now();
		deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
		previousTime = currentTime;
	}
	float getDeltaTime() { return deltaTime; }

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> previousTime;
	float deltaTime;
};

Ticker ticker;
VisualServer *vm;

void cubeMaker(Mesh *mesh) {

	mesh->vertices.push_back(Vertex({ { -1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f } }));
	mesh->vertices.push_back(Vertex({ { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f } }));
	mesh->vertices.push_back(Vertex({ { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }));
	mesh->vertices.push_back(Vertex({ { -1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } }));
	mesh->vertices.push_back(Vertex({ { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } }));
	mesh->vertices.push_back(Vertex({ { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } }));
	mesh->vertices.push_back(Vertex({ { 1.0f, 1.0f, -1.0f }, { 1.0f, 0.0f } }));
	mesh->vertices.push_back(Vertex({ { -1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f } }));
	mesh->triangles.push_back(Triangle({ 0, 1, 2 }));
	mesh->triangles.push_back(Triangle({ 2, 3, 0 }));
	mesh->triangles.push_back(Triangle({ 1, 5, 6 }));
	mesh->triangles.push_back(Triangle({ 6, 2, 1 }));
	mesh->triangles.push_back(Triangle({ 7, 6, 5 }));
	mesh->triangles.push_back(Triangle({ 5, 4, 7 }));
	mesh->triangles.push_back(Triangle({ 4, 0, 3 }));
	mesh->triangles.push_back(Triangle({ 3, 7, 4 }));
	mesh->triangles.push_back(Triangle({ 4, 5, 1 }));
	mesh->triangles.push_back(Triangle({ 1, 0, 4 }));
	mesh->triangles.push_back(Triangle({ 3, 2, 6 }));
	mesh->triangles.push_back(Triangle({ 6, 7, 3 }));
}

glm::mat4 cameraBoom;
#if CLOUDY_CUBES_TEST
float cameraBoomLenght = 20;
vector<Mesh *> meshes;
Texture *texture;
#endif

#if TEXTURE_TEST
float cameraBoomLenght = 5;
Mesh *triangleMesh;
Texture *texture;
#endif

#if LOAD_TEST
float cameraBoomLenght = 5;
Mesh *mesh;
Texture *texture;
Mesh *planeMesh;
Texture *planeTexture;
#endif

void ready() {

	// Update camera view
	vm->getVulkanServer()->getCamera().setNearFar(0.1, 100.);

	cameraBoom = glm::mat4(1.);

	Camera &cam = vm->getVulkanServer()->getCamera();
	glm::mat4 camTransform(glm::translate(glm::mat4(1.), glm::vec3(0., 0., cameraBoomLenght)));
	cam.setTransform(cameraBoom * camTransform);

#if CLOUDY_CUBES_TEST

	texture = new Texture(vm);
	texture->load("/home/andrea/Workspace/git/HelloVulkan/assets/TestText.jpg");

	meshes.resize(50);
	float ballRadius = 20.;

	// Create cubes
	for (int i = meshes.size() - 1; 0 <= i; --i) {
		meshes[i] = new Mesh;
		meshes[i]->setColorTexture(texture);
		cubeMaker(meshes[i]);
		meshes[i]->setTransform(glm::translate(glm::mat4(1.), glm::ballRand(ballRadius)));
		vm->addMesh(meshes[i]);
	}
#endif

#if TEXTURE_TEST

	texture = new Texture(&vm);
	texture->load("assets/TestText.jpg");

	triangleMesh = new Mesh;
	triangleMesh->vertices.push_back(Vertex({ { -1.0f, -1.0f, 1.0f }, { 0., 1. } }));
	triangleMesh->vertices.push_back(Vertex({ { 1.0f, -1.0f, 1.0f }, { 1., 0. } }));
	triangleMesh->vertices.push_back(Vertex({ { 1.0f, 1.0f, 1.0f }, { 1., 1. } }));
	triangleMesh->triangles.push_back(Triangle({ 0, 1, 2 }));
	triangleMesh->setColorTexture(texture);
	vm->addMesh(triangleMesh);

#endif

#if LOAD_TEST
	texture = new Texture(&vm);
	texture->load("assets/deagle/ESe_Material__106_color.png");

	mesh = new Mesh;
	mesh->loadObj("assets/deagle/ESe.obj");
	mesh->setColorTexture(texture);
	vm->addMesh(mesh);

	planeTexture = new Texture(&vm);
	planeTexture->load("assets/default.png");

	planeMesh = new Mesh;
	planeMesh->loadObj("assets/quad.obj");
	planeMesh->setColorTexture(planeTexture);
	planeMesh->setTransform(glm::rotate(glm::translate(glm::mat4(1.), glm::vec3(-2, 0, 0)), glm::radians(-90.f), glm::vec3(0, 0, 1)));
	vm->addMesh(planeMesh);

#endif
}

void exit() {

#if CLOUDY_CUBES_TEST
	for (int i = meshes.size() - 1; 0 <= i; --i) {
		vm->removeMesh(meshes[i]);
		delete meshes[i];
	}
#endif

#if TEXTURE_TEST
	vm->removeMesh(triangleMesh);
	delete triangleMesh;
	triangleMesh;

	delete texture;
	texture = nullptr;
#endif

#if LOAD_TEST
	vm->removeMesh(mesh);

	delete mesh;
	mesh = nullptr;

	delete texture;
	texture = nullptr;

	delete planeMesh;
	planeMesh = nullptr;

	delete planeTexture;
	planeTexture = nullptr;
#endif
}

void tick(float deltaTime) {
	//cout << "FPS: " << to_string((int)(1/deltaTime)) << endl;

#if CLOUDY_CUBES_TEST

	Camera &cam = vm->getVulkanServer()->getCamera();
	glm::mat4 camTransform(glm::translate(glm::mat4(1.), glm::vec3(0., 0., cameraBoomLenght)));
	cameraBoom = glm::rotate(cameraBoom, deltaTime * glm::radians(20.f), glm::vec3(0, 1, 0));
	cam.setTransform(cameraBoom * camTransform);

	for (int i = meshes.size() - 1; 0 <= i; --i) {
		meshes[i]->setTransform(glm::rotate(meshes[i]->getTransform(), deltaTime * glm::radians(90.0f), glm::vec3(1.0f, .0f, .0f)));
	}
#endif

#if LOAD_TEST
	//Camera &cam = vm->getVulkanServer()->getCamera();
	//glm::mat4 camTransform(glm::translate(glm::mat4(1.), glm::vec3(0., 2., cameraBoomLenght)));
	//cameraBoom = glm::rotate(cameraBoom, deltaTime * glm::radians(180.f), glm::vec3(0, 1, 0));
	//cam.lookAt((cameraBoom * camTransform)[3], glm::vec3(0, 0, 0));

#endif
}

void Main::start() {
	WindowServer *window_server = new GLFWWindowServer;

	if (window_server->init()) {

		vm = new VisualServer(window_server);
		if (vm->init()) {

			ticker.init();
			ready();
			while (vm->can_step()) {
				ticker.step();
				tick(ticker.getDeltaTime());
				vm->step();
			}

			exit();
			vm->terminate();
		}
		delete vm;
		vm = NULL;

		window_server->terminate();
	}

	delete window_server;
	window_server = NULL;
}
