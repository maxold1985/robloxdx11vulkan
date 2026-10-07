#pragma once

#ifdef _WIN32

#include "RobloxObjectModel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Dx11Renderer
{
public:
	using InputCallback =
		std::function<
			void(
				const std::string& keyCode,
				bool pressed
			)
		>;

	using PropertyCallback =
		std::function<
			bool(
				const RobloxObjectModel::
					RenderPartPropertyUpdate&
						update
			)
		>;

	Dx11Renderer();
	~Dx11Renderer();

	Dx11Renderer(
		const Dx11Renderer&
	) = delete;

	Dx11Renderer& operator=(
		const Dx11Renderer&
	) = delete;

	bool initialize(
		const wchar_t* title,
		int width,
		int height
	);

	void setInputCallback(
		InputCallback callback
	);

	void setPropertyCallback(
		PropertyCallback callback
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
