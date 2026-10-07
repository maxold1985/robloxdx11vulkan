#include "LuauRuntime.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
	bool startsWith(
		const std::string& value,
		const std::string& prefix
	)
	{
		return
			value.size() >= prefix.size() &&
			value.compare(
				0,
				prefix.size(),
				prefix
			) == 0;
	}

	int parsePositiveInt(
		const std::string& value,
		const char* optionName
	)
	{
		const int result =
			std::atoi(
				value.c_str()
			);

		if (result < 0)
		{
			throw std::runtime_error(
				std::string(optionName) +
				" must be >= 0"
			);
		}

		return result;
	}
}

int main(int argc, char** argv)
{
	try
	{
		LuauRuntime runtime;

		if (argc <= 1)
		{
			const std::string path =
				"native/scripts/test.luau";

			std::cout
				<< "[robloxdx11vulkan] running "
				<< path
				<< "\n";

			if (!runtime.executeFile(path))
				return 1;

			runtime.step(1.0 / 60.0);

			std::cout
				<< "[robloxdx11vulkan] finished\n";

			return 0;
		}

		for (
			int index = 1;
			index < argc;
			++index
		)
		{
			const std::string argument =
				argv[index];

			if (
				startsWith(
					argument,
					"--press="
				)
			)
			{
				const std::string key =
					argument.substr(8);

				std::cout
					<< "[input] press "
					<< key
					<< "\n";

				runtime.emitKey(
					key,
					true
				);

				continue;
			}

			if (
				startsWith(
					argument,
					"--release="
				)
			)
			{
				const std::string key =
					argument.substr(10);

				std::cout
					<< "[input] release "
					<< key
					<< "\n";

				runtime.emitKey(
					key,
					false
				);

				continue;
			}

			if (
				startsWith(
					argument,
					"--step="
				)
			)
			{
				const int frames =
					parsePositiveInt(
						argument.substr(7),
						"--step"
					);

				for (
					int frame = 0;
					frame < frames;
					++frame
				)
				{
					runtime.step(
						1.0 / 60.0
					);
				}

				continue;
			}

			std::cout
				<< "[robloxdx11vulkan] running "
				<< argument
				<< "\n";

			if (
				!runtime.executeFile(
					argument
				)
			)
			{
				return 1;
			}
		}

		std::cout
			<< "[robloxdx11vulkan] finished\n";

		return 0;
	}
	catch (
		const std::exception& exception
	)
	{
		std::cerr
			<< "[fatal] "
			<< exception.what()
			<< "\n";

		return 1;
	}
}
