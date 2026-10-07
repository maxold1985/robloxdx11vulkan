#include "LuauRuntime.h"

#ifdef _WIN32
#include "Dx11Renderer.h"
#endif

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

	struct Options
	{
		bool render = false;
		int width = 1280;
		int height = 720;

		std::vector<std::string> scripts;
		std::vector<std::string> commands;
	};

	Options parseOptions(
		int argc,
		char** argv
	)
	{
		Options options;

		for (
			int index = 1;
			index < argc;
			++index
		)
		{
			const std::string argument =
				argv[index];

			if (argument == "--render")
			{
				options.render = true;
				continue;
			}

			if (
				startsWith(
					argument,
					"--width="
				)
			)
			{
				options.width =
					parsePositiveInt(
						argument.substr(8),
						"--width"
					);

				continue;
			}

			if (
				startsWith(
					argument,
					"--height="
				)
			)
			{
				options.height =
					parsePositiveInt(
						argument.substr(9),
						"--height"
					);

				continue;
			}

			if (
				startsWith(
					argument,
					"--press="
				) ||
				startsWith(
					argument,
					"--release="
				) ||
				startsWith(
					argument,
					"--step="
				)
			)
			{
				options.commands.push_back(
					argument
				);

				continue;
			}

			options.scripts.push_back(
				argument
			);
		}

		return options;
	}

	bool loadScripts(
		LuauRuntime& runtime,
		const std::vector<std::string>& scripts
	)
	{
		for (
			const std::string& script :
			scripts
		)
		{
			std::cout
				<< "[robloxdx11vulkan] running "
				<< script
				<< "\n";

			if (
				!runtime.executeFile(
					script
				)
			)
			{
				return false;
			}
		}

		return true;
	}

	bool runCommands(
		LuauRuntime& runtime,
		const std::vector<std::string>& commands
	)
	{
		for (
			const std::string& command :
			commands
		)
		{
			if (
				startsWith(
					command,
					"--press="
				)
			)
			{
				runtime.emitKey(
					command.substr(8),
					true
				);

				continue;
			}

			if (
				startsWith(
					command,
					"--release="
				)
			)
			{
				runtime.emitKey(
					command.substr(10),
					false
				);

				continue;
			}

			if (
				startsWith(
					command,
					"--step="
				)
			)
			{
				const int frames =
					parsePositiveInt(
						command.substr(7),
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
		}

		return true;
	}

#ifdef _WIN32
	int runRenderer(
		LuauRuntime& runtime,
		int width,
		int height
	)
	{
		Dx11Renderer renderer;

		if (
			!renderer.initialize(
				L"RobloxDX11Vulkan - Luau Runtime",
				width,
				height
			)
		)
		{
			std::cerr
				<< "[DX11] failed to initialize renderer\n";

			return 1;
		}

		renderer.setInputCallback(
			[
				&runtime
			](
				const std::string& keyCode,
				bool pressed
			)
			{
				runtime.emitKey(
					keyCode,
					pressed
				);
			}
		);

		renderer.setPropertyCallback(
			[
				&runtime
			](
				const RobloxObjectModel::
					RenderPartPropertyUpdate&
						update
			)
			{
				return
					runtime
						.applyRenderPartProperties(
							update
						);
			}
		);

		using Clock =
			std::chrono::steady_clock;

		auto previous =
			Clock::now();

		while (
			renderer.pumpEvents()
		)
		{
			const auto now =
				Clock::now();

			double deltaTime =
				std::chrono::duration<
					double
				>(
					now - previous
				).count();

			previous = now;

			if (deltaTime < 0.0)
				deltaTime = 0.0;

			if (deltaTime > 0.1)
				deltaTime = 0.1;

			runtime.step(
				deltaTime
			);

			renderer.render(
				runtime.getRenderParts(),
				runtime.getRenderCamera()
			);
		}

		return 0;
	}
#endif
}

int main(
	int argc,
	char** argv
)
{
	try
	{
		LuauRuntime runtime;

		Options options =
			parseOptions(
				argc,
				argv
			);

		if (
			options.scripts.empty()
		)
		{
			if (options.render)
			{
				options.scripts = {
					"native/scripts/cube/CubeServer.server.lua",
					"native/scripts/cube/CubeClient.client.lua"
				};
			}
			else
			{
				options.scripts = {
					"native/scripts/test.luau"
				};
			}
		}

		if (
			!loadScripts(
				runtime,
				options.scripts
			)
		)
		{
			return 1;
		}

		if (
			!runCommands(
				runtime,
				options.commands
			)
		)
		{
			return 1;
		}

		if (options.render)
		{
#ifdef _WIN32
			return runRenderer(
				runtime,
				options.width,
				options.height
			);
#else
			std::cerr
				<< "--render currently requires Windows DirectX 11\n";

			return 1;
#endif
		}

		runtime.step(
			1.0 / 60.0
		);

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
