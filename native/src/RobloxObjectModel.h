#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct lua_State;

namespace RobloxObjectModel
{
	struct RenderPartSnapshot
	{
		std::uint64_t id = 0;
		std::string name;

		float positionX = 0.0f;
		float positionY = 0.0f;
		float positionZ = 0.0f;

		float sizeX = 1.0f;
		float sizeY = 1.0f;
		float sizeZ = 1.0f;

		float colorR = 0.5f;
		float colorG = 0.5f;
		float colorB = 0.5f;

		float transparency = 0.0f;

		bool anchored = false;
		bool canCollide = true;
	};

	struct RenderPartPropertyUpdate
	{
		std::uint64_t id = 0;

		float positionX = 0.0f;
		float positionY = 0.0f;
		float positionZ = 0.0f;

		float sizeX = 1.0f;
		float sizeY = 1.0f;
		float sizeZ = 1.0f;

		float colorR = 0.5f;
		float colorG = 0.5f;
		float colorB = 0.5f;

		float transparency = 0.0f;

		bool anchored = false;
		bool canCollide = true;
	};

	struct RenderCameraSnapshot
	{
		bool hasSubject = false;

		float targetX = 0.0f;
		float targetY = 0.0f;
		float targetZ = 0.0f;
	};

	void install(lua_State* L);
	void shutdown(lua_State* L);

	void step(
		lua_State* L,
		double deltaTime
	);

	void emitKey(
		lua_State* L,
		const std::string& keyCodeName,
		bool pressed
	);

	std::vector<RenderPartSnapshot>
	getRenderParts(lua_State* L);

	RenderCameraSnapshot
	getRenderCamera(lua_State* L);

	bool applyRenderPartProperties(
		lua_State* L,
		const RenderPartPropertyUpdate& update
	);

	bool isInstance(
		lua_State* L,
		int index
	);
}
