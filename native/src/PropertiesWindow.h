#pragma once

#ifdef _WIN32

#include "RobloxObjectModel.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <functional>
#include <memory>
#include <vector>

class PropertiesWindow
{
public:
	using ApplyCallback =
		std::function<
			bool(
				const RobloxObjectModel::
					RenderPartPropertyUpdate&
						update
			)
		>;

	PropertiesWindow();
	~PropertiesWindow();

	PropertiesWindow(
		const PropertiesWindow&
	) = delete;

	PropertiesWindow& operator=(
		const PropertiesWindow&
	) = delete;

	bool initialize(
		HWND owner
	);

	void setApplyCallback(
		ApplyCallback callback
	);

	void setParts(
		const std::vector<
			RobloxObjectModel::RenderPartSnapshot
		>& parts
	);

	void show();
	void hide();

	bool isVisible() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

#endif
