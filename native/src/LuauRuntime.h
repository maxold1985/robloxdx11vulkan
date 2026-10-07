#pragma once

#include "RobloxObjectModel.h"

#include <string>
#include <vector>

struct lua_State;

class LuauRuntime
{
public:
	LuauRuntime();
	~LuauRuntime();

	LuauRuntime(
		const LuauRuntime&
	) = delete;

	LuauRuntime& operator=(
		const LuauRuntime&
	) = delete;

	bool execute(
		const std::string& source,
		const std::string& chunkName
	);

	bool executeFile(
		const std::string& path
	);

	void step(double deltaTime);

	void emitKey(
		const std::string& keyCodeName,
		bool pressed
	);

	std::vector<
		RobloxObjectModel::RenderPartSnapshot
	> getRenderParts() const;

	RobloxObjectModel::RenderCameraSnapshot
	getRenderCamera() const;

	bool applyRenderPartProperties(
		const RobloxObjectModel::
			RenderPartPropertyUpdate& update
	);

private:
	lua_State* state_;
};
