#include <iostream>

#include "VisualServer.h"

#include <iostream>

int main() {

	Mesh mesh;
	mesh.triangles.push_back(Triangle({Vertex({{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}}),
									   Vertex({{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.f}}),
									   Vertex({{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.f}})}
									  )
							 );

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
