#include <iostream>

#include "VisualServer.h"

#include <iostream>

int main() {

	VisualServer vm;

	if(vm.init()){
		while(vm.can_step()){
			vm.step();
		}
	}

	vm.terminate();

	return 0;
}
