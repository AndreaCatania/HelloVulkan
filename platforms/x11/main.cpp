
#include "main/main.h"

#include "core/error_macros.h"
#include "core/print_string.h"
#include "core/typedefs.h"
#include <iostream>

/// This is really handy to avoid to set environment variable from outside
void init_environment_variable() {
#ifdef DEBUG_ENABLED
	putenv(__STR(VULKAN_EXPLICIT_LAYERS));
#endif
}

void print_line_callback(void *p_user_data, const std::string &p_line, bool p_error) {
	if (p_error) {
		std::cerr << "[ERROR] " << p_line << std::endl;
	} else {
		std::cout << "[INFO] " << p_line << std::endl;
	}
}

void print_error_callback(
		void *p_user_data,
		const char *p_function,
		const char *p_file,
		int p_line,
		const char *p_error,
		const char *p_explain,
		ErrorHandlerType p_type) {

	std::string msg =
			std::string() +
			(p_type == ERR_HANDLER_ERROR ? "[ERROR] " : "[WARN]") +
			p_file +
			" Function: " + p_function +
			", line: " +
			itos(p_line) +
			"\n\t" + p_error +
			" " + p_explain;

	std::cerr << msg << std::endl;
}

int main() {

	init_environment_variable();

	PrintHandlerList *print_handler = new PrintHandlerList;
	print_handler->printfunc = print_line_callback;
	add_print_handler(print_handler);

	ErrorHandlerList *error_handler = new ErrorHandlerList;
	error_handler->errfunc = print_error_callback;
	add_error_handler(error_handler);

	Main main;
	main.start();

	remove_error_handler(error_handler);
	delete error_handler;
	error_handler = nullptr;

	remove_print_handler(print_handler);
	delete print_handler;
	print_handler = nullptr;
}
