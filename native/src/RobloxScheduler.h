#pragma once

struct lua_State;

namespace RobloxScheduler
{
	void install(lua_State* L);
	void shutdown(lua_State* L);

	void step(
		lua_State* L,
		double deltaTime
	);

	bool isManaged(
		lua_State* L,
		lua_State* thread
	);
}
