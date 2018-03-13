#include <iostream>

#include "VisualServer.h"

#include <iostream>

int main() {

	Mesh mesh;
	mesh.vertices.push_back(Vertex({{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}}));
	mesh.vertices.push_back(Vertex({{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.f}}));
	mesh.vertices.push_back(Vertex({{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.f}}));
	mesh.vertices.push_back(Vertex({{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.f}}));
	mesh.triangles.push_back(Triangle({0,1,2}));
	mesh.triangles.push_back(Triangle({0,2,3}));

	VisualServer vm;

	if(vm.init()){

		vm.add_mesh(&mesh);

		while(vm.can_step()){
			vm.step();
		}
	}

	vm.terminate();

	return 0;
}
