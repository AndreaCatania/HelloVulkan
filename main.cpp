#include <iostream>

#include "VisualServer.h"

#include <iostream>

int main() {

	const std::vector<Vertex> vertices = {
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.f}}
	};

	VisualServer vm;

	if(vm.init()){

		vm.add_vertices(vertices);

		while(vm.can_step()){
			vm.step();
		}
	}

	vm.terminate();

	return 0;
}
