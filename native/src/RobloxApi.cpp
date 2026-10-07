#include "RobloxApi.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C"
{
#include "lua.h"
#include "lualib.h"
}

namespace
{
	void pushTypedTable(lua_State* L, const char* typeName)
	{
		lua_newtable(L);

		lua_pushstring(L, typeName);
		lua_setfield(L, -2, "__type");
	}

	void setStringField(lua_State* L, const char* name, const char* value)
	{
		lua_pushstring(L, value);
		lua_setfield(L, -2, name);
	}

	double getNumberField(lua_State* L, int index, const char* field)
	{
		lua_getfield(L, index, field);
		const double value = luaL_checknumber(L, -1);
		lua_pop(L, 1);
		return value;
	}

	void pushVector3(lua_State* L, double x, double y, double z)
	{
		pushTypedTable(L, "Vector3");

		lua_pushnumber(L, x);
		lua_setfield(L, -2, "X");

		lua_pushnumber(L, y);
		lua_setfield(L, -2, "Y");

		lua_pushnumber(L, z);
		lua_setfield(L, -2, "Z");

		luaL_getmetatable(L, "Roblox.Vector3");
		lua_setmetatable(L, -2);
	}

	int vector3New(lua_State* L)
	{
		const double x = luaL_checknumber(L, 1);
		const double y = luaL_checknumber(L, 2);
		const double z = luaL_checknumber(L, 3);

		pushVector3(L, x, y, z);
		return 1;
	}

	int vector3Add(lua_State* L)
	{
		const double ax = getNumberField(L, 1, "X");
		const double ay = getNumberField(L, 1, "Y");
		const double az = getNumberField(L, 1, "Z");

		const double bx = getNumberField(L, 2, "X");
		const double by = getNumberField(L, 2, "Y");
		const double bz = getNumberField(L, 2, "Z");

		pushVector3(L, ax + bx, ay + by, az + bz);
		return 1;
	}

	int vector3Sub(lua_State* L)
	{
		const double ax = getNumberField(L, 1, "X");
		const double ay = getNumberField(L, 1, "Y");
		const double az = getNumberField(L, 1, "Z");

		const double bx = getNumberField(L, 2, "X");
		const double by = getNumberField(L, 2, "Y");
		const double bz = getNumberField(L, 2, "Z");

		pushVector3(L, ax - bx, ay - by, az - bz);
		return 1;
	}

	int vector3ToString(lua_State* L)
	{
		const double x = getNumberField(L, 1, "X");
		const double y = getNumberField(L, 1, "Y");
		const double z = getNumberField(L, 1, "Z");

		char buffer[160];
		std::snprintf(buffer, sizeof(buffer), "%g, %g, %g", x, y, z);

		lua_pushstring(L, buffer);
		return 1;
	}

	int instanceDestroy(lua_State* L)
	{
		luaL_checktype(L, 1, LUA_TTABLE);

		lua_pushboolean(L, 1);
		lua_setfield(L, 1, "_destroyed");

		lua_pushnil(L);
		lua_setfield(L, 1, "Parent");

		return 0;
	}

	int instanceNew(lua_State* L)
	{
		const char* className = luaL_checkstring(L, 1);

		pushTypedTable(L, "Instance");

		setStringField(L, "ClassName", className);
		setStringField(L, "Name", className);

		lua_pushcfunction(L, instanceDestroy, "Instance.Destroy");
		lua_setfield(L, -2, "Destroy");

		if (std::strcmp(className, "Part") == 0)
		{
			pushVector3(L, 0.0, 0.0, 0.0);
			lua_setfield(L, -2, "Position");

			pushVector3(L, 4.0, 1.0, 2.0);
			lua_setfield(L, -2, "Size");

			lua_pushboolean(L, 0);
			lua_setfield(L, -2, "Anchored");
		}

		if (!lua_isnoneornil(L, 2))
		{
			lua_pushvalue(L, 2);
			lua_setfield(L, -2, "Parent");
		}

		return 1;
	}

	int gameGetService(lua_State* L)
	{
		const char* serviceName = luaL_checkstring(L, 2);

		if (std::strcmp(serviceName, "Workspace") == 0)
		{
			lua_getglobal(L, "workspace");
			return 1;
		}

		lua_getglobal(L, serviceName);

		if (!lua_isnil(L, -1))
			return 1;

		lua_pop(L, 1);

		pushTypedTable(L, "Instance");
		setStringField(L, "Name", serviceName);
		setStringField(L, "ClassName", serviceName);

		lua_pushvalue(L, -1);
		lua_setglobal(L, serviceName);

		return 1;
	}

	int robloxTypeof(lua_State* L)
	{
		if (lua_istable(L, 1))
		{
			lua_getfield(L, 1, "__type");

			if (lua_isstring(L, -1))
				return 1;

			lua_pop(L, 1);
		}

		lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
		return 1;
	}

	int robloxWarn(lua_State* L)
	{
		const int count = lua_gettop(L);

		std::fputs("[warn] ", stderr);

		for (int index = 1; index <= count; ++index)
		{
			size_t length = 0;
			const char* value = luaL_tolstring(L, index, &length);

			if (index > 1)
				std::fputc('\t', stderr);

			if (value)
				std::fwrite(value, 1, length, stderr);

			lua_pop(L, 1);
		}

		std::fputc('\n', stderr);
		return 0;
	}

	void createSimpleService(lua_State* L, const char* name)
	{
		pushTypedTable(L, "Instance");
		setStringField(L, "Name", name);
		setStringField(L, "ClassName", name);
		lua_setglobal(L, name);
	}

	void installVector3(lua_State* L)
	{
		luaL_newmetatable(L, "Roblox.Vector3");

		lua_pushcfunction(L, vector3Add, "Vector3.__add");
		lua_setfield(L, -2, "__add");

		lua_pushcfunction(L, vector3Sub, "Vector3.__sub");
		lua_setfield(L, -2, "__sub");

		lua_pushcfunction(L, vector3ToString, "Vector3.__tostring");
		lua_setfield(L, -2, "__tostring");

		lua_pop(L, 1);

		lua_newtable(L);
		lua_pushcfunction(L, vector3New, "Vector3.new");
		lua_setfield(L, -2, "new");
		lua_setglobal(L, "Vector3");
	}

	void installInstance(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, instanceNew, "Instance.new");
		lua_setfield(L, -2, "new");
		lua_setglobal(L, "Instance");
	}

	void installWorkspace(lua_State* L)
	{
		pushTypedTable(L, "Instance");
		setStringField(L, "Name", "Workspace");
		setStringField(L, "ClassName", "Workspace");
		lua_setglobal(L, "workspace");
	}

	void installDataModel(lua_State* L)
	{
		pushTypedTable(L, "Instance");
		setStringField(L, "Name", "game");
		setStringField(L, "ClassName", "DataModel");

		lua_pushcfunction(L, gameGetService, "DataModel.GetService");
		lua_setfield(L, -2, "GetService");

		lua_getglobal(L, "workspace");
		lua_setfield(L, -2, "Workspace");

		lua_setglobal(L, "game");
	}
}

void RobloxApi::install(lua_State* L)
{
	installVector3(L);
	installInstance(L);
	installWorkspace(L);

	createSimpleService(L, "RunService");
	createSimpleService(L, "Players");
	createSimpleService(L, "ReplicatedStorage");
	createSimpleService(L, "ServerScriptService");
	createSimpleService(L, "UserInputService");

	installDataModel(L);

	lua_pushcfunction(L, robloxTypeof, "typeof");
	lua_setglobal(L, "typeof");

	lua_pushcfunction(L, robloxWarn, "warn");
	lua_setglobal(L, "warn");
}
