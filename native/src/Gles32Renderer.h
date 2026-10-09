#pragma once

#if defined(ROBLOX_HAS_GLES32)

#include "RobloxObjectModel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Gles32Renderer
{
public:
	using InputCallback =
		std::function<
			void(
				const std::string& keyCode,
				bool pressed
			)
		>;

	Gles32Renderer();
	~Gles32Renderer();

	Gles32Renderer(
		const Gles32Renderer&
	) = delete;

	Gles32Renderer& operator=(
		const Gles32Renderer&
	) = delete;

	bool initialize(
		const char* title,
		int width,
		int height
	);

	void setInputCallback(
		InputCallback callback
	);

	bool pumpEvents();

	void render(
		const std::vector<
			RobloxObjectModel::RenderPartSnapshot
		>& parts,
		const RobloxObjectModel::RenderCameraSnapshot&
			camera
	);

	bool isRunning() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

#endif
