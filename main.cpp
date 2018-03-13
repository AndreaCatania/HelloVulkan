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

	Mesh mesh2;
	mesh2.vertices.push_back(Vertex({{-0.7f, -0.7f}, {1.0f, 0.0f, 1.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{0.7f, -0.7f}, {1.0f, 1.0f, 0.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{0.7f, 0.7f}, {0.0f, 1.0f, 1.0f, 1.f}}));
	mesh2.vertices.push_back(Vertex({{-0.7f, 0.7f}, {1.0f, 0.0f, 1.0f, 1.f}}));
	mesh2.triangles.push_back(Triangle({0,1,2}));
	mesh2.triangles.push_back(Triangle({0,2,3}));

	VisualServer vm;

	if(vm.init()){

		vm.add_mesh(&mesh);
		vm.add_mesh(&mesh2);

		while(vm.can_step()){
			vm.step();
		}
	}

	vm.terminate();

	return 0;
}
