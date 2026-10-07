#pragma once

#include <string>

struct lua_State;

namespace RobloxTypes
{
	struct Vector3Value
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
	};

	struct Color3Value
	{
		double r = 0.0;
		double g = 0.0;
		double b = 0.0;
	};

	void install(lua_State* L);

	void pushVector3(lua_State* L, const Vector3Value& value);
	Vector3Value checkVector3(lua_State* L, int index);
	bool isVector3(lua_State* L, int index);

	void pushColor3(lua_State* L, const Color3Value& value);
	Color3Value checkColor3(lua_State* L, int index);
	bool isColor3(lua_State* L, int index);

	void pushEnumItem(
		lua_State* L,
		const char* enumType,
		const char* itemName
	);

	std::string checkEnumItem(
		lua_State* L,
		int index,
		const char* expectedEnumType
	);

	bool isEnumItem(lua_State* L, int index);
	bool hasType(lua_State* L, int index, const char* typeName);
}
