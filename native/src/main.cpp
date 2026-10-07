#include "LuauRuntime.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
	try
	{
		LuauRuntime runtime;

		const std::string scriptPath =
			argc >= 2
				? argv[1]
				: "native/scripts/test.luau";

		std::cout
			<< "[robloxdx11vulkan] running "
			<< scriptPath
			<< "\n";

		if (!runtime.executeFile(scriptPath))
			return 1;

		std::cout << "[robloxdx11vulkan] script finished\n";
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "[fatal] " << exception.what() << "\n";
		return 1;
	}
}
