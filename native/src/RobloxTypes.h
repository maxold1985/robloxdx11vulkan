#pragma once

struct lua_State;

namespace RobloxTypes
{
	struct Vector3Value
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
	};

	void install(lua_State* L);

	void pushVector3(lua_State* L, const Vector3Value& value);
	Vector3Value checkVector3(lua_State* L, int index);
	bool isVector3(lua_State* L, int index);
}
