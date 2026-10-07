#include "RobloxObjectModel.h"
#include "RobloxTypes.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C"
{
#include "lua.h"
#include "lualib.h"
}

namespace
{
	constexpr const char* INSTANCE_METATABLE = "Roblox.Instance";
	constexpr const char* SIGNAL_METATABLE = "Roblox.RBXScriptSignal";
	constexpr const char* CONNECTION_METATABLE = "Roblox.RBXScriptConnection";

	struct ConnectionState;

	struct InstanceObject
	{
		std::string className = "Instance";
		std::string name = "Instance";

		InstanceObject* parent = nullptr;
		std::vector<InstanceObject*> children;

		bool destroyed = false;
		bool archivable = true;

		bool anchored = false;
		bool canCollide = true;
		double transparency = 0.0;

		RobloxTypes::Vector3Value position;
		RobloxTypes::Vector3Value size{4.0, 1.0, 2.0};

		int luaRef = LUA_NOREF;

		std::vector<std::unique_ptr<ConnectionState>> ownedConnections;
		std::unordered_map<std::string, std::vector<ConnectionState*>> signals;
	};

	struct ConnectionState
	{
		int callbackRef = LUA_NOREF;
		bool connected = true;
		bool once = false;
	};

	struct RuntimeContext
	{
		std::vector<std::unique_ptr<InstanceObject>> objects;
		std::unordered_map<std::string, InstanceObject*> services;

		InstanceObject* dataModel = nullptr;
		InstanceObject* workspace = nullptr;
	};

	struct InstanceUserdata
	{
		InstanceObject* object = nullptr;
	};

	struct SignalUserdata
	{
		InstanceObject* object = nullptr;
		std::string eventName;
	};

	struct ConnectionUserdata
	{
		ConnectionState* connection = nullptr;
	};

	std::unordered_map<lua_State*, std::unique_ptr<RuntimeContext>> contexts;

	RuntimeContext& context(lua_State* L)
	{
		lua_State* mainThread = lua_mainthread(L);
		auto iterator = contexts.find(mainThread);

		if (iterator == contexts.end())
		{
			luaL_error(L, "Roblox object model is not installed");
		}

		return *iterator->second;
	}

	InstanceObject* createObject(
		RuntimeContext& runtime,
		std::string className,
		std::string name = {}
	)
	{
		auto object = std::make_unique<InstanceObject>();
		object->className = std::move(className);
		object->name = name.empty() ? object->className : std::move(name);

		InstanceObject* raw = object.get();
		runtime.objects.push_back(std::move(object));

		return raw;
	}

	void pushInstance(lua_State* L, InstanceObject* object)
	{
		if (!object)
		{
			lua_pushnil(L);
			return;
		}

		if (object->luaRef != LUA_NOREF)
		{
			lua_getref(L, object->luaRef);
			return;
		}

		auto* userdata = static_cast<InstanceUserdata*>(
			lua_newuserdata(L, sizeof(InstanceUserdata))
		);

		userdata->object = object;

		luaL_getmetatable(L, INSTANCE_METATABLE);
		lua_setmetatable(L, -2);

		object->luaRef = lua_ref(L, -1);
	}

	InstanceObject* checkInstance(lua_State* L, int index)
	{
		auto* userdata = static_cast<InstanceUserdata*>(
			luaL_checkudata(L, index, INSTANCE_METATABLE)
		);

		if (!userdata->object)
		{
			luaL_error(L, "invalid Instance");
		}

		return userdata->object;
	}

	void disconnect(lua_State* L, ConnectionState* connection)
	{
		if (!connection || !connection->connected)
			return;

		connection->connected = false;

		if (connection->callbackRef != LUA_NOREF)
		{
			connection->callbackRef =
				lua_unref(L, connection->callbackRef);
		}
	}

	void purgeDisconnected(
		InstanceObject* object,
		const std::string& eventName
	)
	{
		auto iterator = object->signals.find(eventName);

		if (iterator == object->signals.end())
			return;

		auto& connections = iterator->second;

		connections.erase(
			std::remove_if(
				connections.begin(),
				connections.end(),
				[](ConnectionState* connection)
				{
					return
						connection == nullptr ||
						!connection->connected;
				}
			),
			connections.end()
		);
	}

	template <typename PushArguments>
	void fireEvent(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		int argumentCount,
		PushArguments pushArguments
	)
	{
		auto iterator = object->signals.find(eventName);

		if (iterator == object->signals.end())
			return;

		const std::vector<ConnectionState*> snapshot = iterator->second;

		for (ConnectionState* connection : snapshot)
		{
			if (!connection || !connection->connected)
				continue;

			lua_getref(L, connection->callbackRef);
			pushArguments();

			const int status =
				lua_pcall(L, argumentCount, 0, 0);

			if (status != LUA_OK)
			{
				const char* message = lua_tostring(L, -1);

				std::fprintf(
					stderr,
					"[RBXScriptSignal:%s] %s\n",
					eventName.c_str(),
					message ? message : "callback error"
				);

				lua_pop(L, 1);
			}

			if (connection->once)
			{
				disconnect(L, connection);
			}
		}

		purgeDisconnected(object, eventName);
	}

	void fireEvent0(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			0,
			[]() {}
		);
	}

	void fireEventInstance(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		InstanceObject* argument
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			1,
			[L, argument]()
			{
				pushInstance(L, argument);
			}
		);
	}

	void fireEventInstancePair(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		InstanceObject* first,
		InstanceObject* second
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			2,
			[L, first, second]()
			{
				pushInstance(L, first);
				pushInstance(L, second);
			}
		);
	}

	void fireEventString(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		const std::string& value
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			1,
			[L, &value]()
			{
				lua_pushlstring(
					L,
					value.data(),
					value.size()
				);
			}
		);
	}

	void firePropertyChanged(
		lua_State* L,
		InstanceObject* object,
		const std::string& property
	)
	{
		fireEventString(L, object, "Changed", property);

		fireEvent0(
			L,
			object,
			"PropertyChanged:" + property
		);
	}

	void fireAncestryChangedRecursive(
		lua_State* L,
		InstanceObject* object
	)
	{
		fireEventInstancePair(
			L,
			object,
			"AncestryChanged",
			object,
			object->parent
		);

		for (InstanceObject* child : object->children)
		{
			fireAncestryChangedRecursive(L, child);
		}
	}

	bool isAncestorOf(
		const InstanceObject* ancestor,
		const InstanceObject* object
	)
	{
		for (
			const InstanceObject* current = object->parent;
			current != nullptr;
			current = current->parent
		)
		{
			if (current == ancestor)
				return true;
		}

		return false;
	}

	void removeChild(
		InstanceObject* parent,
		InstanceObject* child
	)
	{
		if (!parent)
			return;

		auto& children = parent->children;

		children.erase(
			std::remove(
				children.begin(),
				children.end(),
				child
			),
			children.end()
		);
	}

	void setParent(
		lua_State* L,
		InstanceObject* object,
		InstanceObject* newParent
	)
	{
		if (object->destroyed)
		{
			luaL_error(
				L,
				"The Parent property of %s is locked",
				object->name.c_str()
			);
		}

		if (newParent && newParent->destroyed)
		{
			luaL_error(
				L,
				"Cannot parent %s to a destroyed Instance",
				object->name.c_str()
			);
		}

		if (newParent == object)
		{
			luaL_error(L, "Attempt to set Instance as its own Parent");
		}

		if (newParent && isAncestorOf(object, newParent))
		{
			luaL_error(
				L,
				"Attempt to set Parent would create a cyclic Instance tree"
			);
		}

		if (object->parent == newParent)
			return;

		InstanceObject* oldParent = object->parent;

		if (oldParent)
		{
			removeChild(oldParent, object);
		}

		object->parent = newParent;

		if (newParent)
		{
			newParent->children.push_back(object);
		}

		if (oldParent)
		{
			fireEventInstance(
				L,
				oldParent,
				"ChildRemoved",
				object
			);
		}

		if (newParent)
		{
			fireEventInstance(
				L,
				newParent,
				"ChildAdded",
				object
			);
		}

		firePropertyChanged(L, object, "Parent");
		fireAncestryChangedRecursive(L, object);
	}

	void destroyObject(lua_State* L, InstanceObject* object)
	{
		if (!object || object->destroyed)
			return;

		fireEvent0(L, object, "Destroying");

		const std::vector<InstanceObject*> children =
			object->children;

		for (InstanceObject* child : children)
		{
			destroyObject(L, child);
		}

		setParent(L, object, nullptr);

		for (auto& connection : object->ownedConnections)
		{
			disconnect(L, connection.get());
		}

		object->signals.clear();
		object->destroyed = true;
	}

	bool isBasePartClass(const std::string& className)
	{
		return
			className == "Part" ||
			className == "MeshPart" ||
			className == "WedgePart" ||
			className == "CornerWedgePart" ||
			className == "TrussPart" ||
			className == "UnionOperation";
	}

	bool classIsA(
		const std::string& className,
		const std::string& requested
	)
	{
		if (className == requested)
			return true;

		if (requested == "Instance")
			return true;

		if (
			isBasePartClass(className) &&
			(
				requested == "BasePart" ||
				requested == "PVInstance"
			)
		)
		{
			return true;
		}

		if (
			(
				className == "Model" ||
				className == "Workspace"
			) &&
			requested == "PVInstance"
		)
		{
			return true;
		}

		if (
			className == "DataModel" &&
			requested == "ServiceProvider"
		)
		{
			return true;
		}

		return false;
	}

	InstanceObject* findFirstChild(
		InstanceObject* object,
		const std::string& name,
		bool recursive
	)
	{
		for (InstanceObject* child : object->children)
		{
			if (!child->destroyed && child->name == name)
				return child;
		}

		if (recursive)
		{
			for (InstanceObject* child : object->children)
			{
				if (InstanceObject* found =
					findFirstChild(child, name, true))
				{
					return found;
				}
			}
		}

		return nullptr;
	}

	void collectDescendants(
		InstanceObject* object,
		std::vector<InstanceObject*>& result
	)
	{
		for (InstanceObject* child : object->children)
		{
			if (child->destroyed)
				continue;

			result.push_back(child);
			collectDescendants(child, result);
		}
	}

	std::string fullName(InstanceObject* object)
	{
		std::vector<std::string> names;

		for (
			InstanceObject* current = object;
			current != nullptr;
			current = current->parent
		)
		{
			names.push_back(current->name);
		}

		std::string result;

		for (auto iterator = names.rbegin(); iterator != names.rend(); ++iterator)
		{
			if (!result.empty())
				result += ".";

			result += *iterator;
		}

		return result;
	}

	void pushSignal(
		lua_State* L,
		InstanceObject* object,
		std::string eventName
	)
	{
		auto destructor =
			[](lua_State*, void* raw)
			{
				static_cast<SignalUserdata*>(raw)->~SignalUserdata();
			};

		void* raw = lua_newuserdatadtor(
			L,
			sizeof(SignalUserdata),
			destructor
		);

		new (raw) SignalUserdata{
			object,
			std::move(eventName)
		};

		luaL_getmetatable(L, SIGNAL_METATABLE);
		lua_setmetatable(L, -2);
	}

	int instanceDestroy(lua_State* L)
	{
		destroyObject(L, checkInstance(L, 1));
		return 0;
	}

	int instanceGetChildren(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);

		lua_createtable(
			L,
			static_cast<int>(object->children.size()),
			0
		);

		int index = 1;

		for (InstanceObject* child : object->children)
		{
			if (child->destroyed)
				continue;

			pushInstance(L, child);
			lua_rawseti(L, -2, index++);
		}

		return 1;
	}

	int instanceGetDescendants(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		std::vector<InstanceObject*> descendants;

		collectDescendants(object, descendants);

		lua_createtable(
			L,
			static_cast<int>(descendants.size()),
			0
		);

		for (
			size_t index = 0;
			index < descendants.size();
			++index
		)
		{
			pushInstance(L, descendants[index]);
			lua_rawseti(
				L,
				-2,
				static_cast<int>(index + 1)
			);
		}

		return 1;
	}

	int instanceFindFirstChild(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string name = luaL_checkstring(L, 2);
		const bool recursive =
			lua_isnoneornil(L, 3)
				? false
				: lua_toboolean(L, 3) != 0;

		pushInstance(
			L,
			findFirstChild(object, name, recursive)
		);

		return 1;
	}

	int instanceFindFirstChildOfClass(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string className = luaL_checkstring(L, 2);

		for (InstanceObject* child : object->children)
		{
			if (
				!child->destroyed &&
				child->className == className
			)
			{
				pushInstance(L, child);
				return 1;
			}
		}

		lua_pushnil(L);
		return 1;
	}

	int instanceFindFirstChildWhichIsA(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string className = luaL_checkstring(L, 2);
		const bool recursive =
			lua_isnoneornil(L, 3)
				? false
				: lua_toboolean(L, 3) != 0;

		std::vector<InstanceObject*> candidates;

		if (recursive)
		{
			collectDescendants(object, candidates);
		}
		else
		{
			candidates = object->children;
		}

		for (InstanceObject* child : candidates)
		{
			if (
				!child->destroyed &&
				classIsA(child->className, className)
			)
			{
				pushInstance(L, child);
				return 1;
			}
		}

		lua_pushnil(L);
		return 1;
	}

	int instanceIsA(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string className = luaL_checkstring(L, 2);

		lua_pushboolean(
			L,
			classIsA(object->className, className)
		);

		return 1;
	}

	int instanceIsDescendantOf(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		InstanceObject* ancestor = checkInstance(L, 2);

		lua_pushboolean(
			L,
			isAncestorOf(ancestor, object)
		);

		return 1;
	}

	int instanceIsAncestorOf(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		InstanceObject* descendant = checkInstance(L, 2);

		lua_pushboolean(
			L,
			isAncestorOf(object, descendant)
		);

		return 1;
	}

	int instanceGetFullName(lua_State* L)
	{
		const std::string value =
			fullName(checkInstance(L, 1));

		lua_pushlstring(
			L,
			value.data(),
			value.size()
		);

		return 1;
	}

	int instanceClearAllChildren(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::vector<InstanceObject*> children =
			object->children;

		for (InstanceObject* child : children)
		{
			destroyObject(L, child);
		}

		return 0;
	}

	int instanceGetPropertyChangedSignal(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string property = luaL_checkstring(L, 2);

		pushSignal(
			L,
			object,
			"PropertyChanged:" + property
		);

		return 1;
	}

	int dataModelGetService(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);

		if (object->className != "DataModel")
		{
			luaL_error(
				L,
				"GetService is only valid on DataModel"
			);
		}

		const std::string serviceName =
			luaL_checkstring(L, 2);

		RuntimeContext& runtime = context(L);
		auto iterator = runtime.services.find(serviceName);

		if (iterator == runtime.services.end())
		{
			luaL_error(
				L,
				"%s is not an available service in this runtime",
				serviceName.c_str()
			);
		}

		pushInstance(L, iterator->second);
		return 1;
	}

	int instanceIndex(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string key = luaL_checkstring(L, 2);

		if (key == "Name")
		{
			lua_pushlstring(
				L,
				object->name.data(),
				object->name.size()
			);
			return 1;
		}

		if (key == "ClassName")
		{
			lua_pushlstring(
				L,
				object->className.data(),
				object->className.size()
			);
			return 1;
		}

		if (key == "Parent")
		{
			pushInstance(L, object->parent);
			return 1;
		}

		if (key == "Archivable")
		{
			lua_pushboolean(L, object->archivable);
			return 1;
		}

		if (isBasePartClass(object->className))
		{
			if (key == "Anchored")
			{
				lua_pushboolean(L, object->anchored);
				return 1;
			}

			if (key == "CanCollide")
			{
				lua_pushboolean(L, object->canCollide);
				return 1;
			}

			if (key == "Transparency")
			{
				lua_pushnumber(L, object->transparency);
				return 1;
			}

			if (key == "Position")
			{
				RobloxTypes::pushVector3(
					L,
					object->position
				);
				return 1;
			}

			if (key == "Size")
			{
				RobloxTypes::pushVector3(
					L,
					object->size
				);
				return 1;
			}
		}

		if (key == "Destroy")
		{
			lua_pushcfunction(
				L,
				instanceDestroy,
				"Instance.Destroy"
			);
			return 1;
		}

		if (key == "GetChildren")
		{
			lua_pushcfunction(
				L,
				instanceGetChildren,
				"Instance.GetChildren"
			);
			return 1;
		}

		if (key == "GetDescendants")
		{
			lua_pushcfunction(
				L,
				instanceGetDescendants,
				"Instance.GetDescendants"
			);
			return 1;
		}

		if (key == "FindFirstChild")
		{
			lua_pushcfunction(
				L,
				instanceFindFirstChild,
				"Instance.FindFirstChild"
			);
			return 1;
		}

		if (key == "FindFirstChildOfClass")
		{
			lua_pushcfunction(
				L,
				instanceFindFirstChildOfClass,
				"Instance.FindFirstChildOfClass"
			);
			return 1;
		}

		if (key == "FindFirstChildWhichIsA")
		{
			lua_pushcfunction(
				L,
				instanceFindFirstChildWhichIsA,
				"Instance.FindFirstChildWhichIsA"
			);
			return 1;
		}

		if (key == "IsA")
		{
			lua_pushcfunction(
				L,
				instanceIsA,
				"Instance.IsA"
			);
			return 1;
		}

		if (key == "IsDescendantOf")
		{
			lua_pushcfunction(
				L,
				instanceIsDescendantOf,
				"Instance.IsDescendantOf"
			);
			return 1;
		}

		if (key == "IsAncestorOf")
		{
			lua_pushcfunction(
				L,
				instanceIsAncestorOf,
				"Instance.IsAncestorOf"
			);
			return 1;
		}

		if (key == "GetFullName")
		{
			lua_pushcfunction(
				L,
				instanceGetFullName,
				"Instance.GetFullName"
			);
			return 1;
		}

		if (key == "ClearAllChildren")
		{
			lua_pushcfunction(
				L,
				instanceClearAllChildren,
				"Instance.ClearAllChildren"
			);
			return 1;
		}

		if (key == "GetPropertyChangedSignal")
		{
			lua_pushcfunction(
				L,
				instanceGetPropertyChangedSignal,
				"Instance.GetPropertyChangedSignal"
			);
			return 1;
		}

		if (
			object->className == "DataModel" &&
			key == "GetService"
		)
		{
			lua_pushcfunction(
				L,
				dataModelGetService,
				"DataModel.GetService"
			);
			return 1;
		}

		if (
			key == "ChildAdded" ||
			key == "ChildRemoved" ||
			key == "AncestryChanged" ||
			key == "Destroying" ||
			key == "Changed"
		)
		{
			pushSignal(L, object, key);
			return 1;
		}

		if (InstanceObject* child =
			findFirstChild(object, key, false))
		{
			pushInstance(L, child);
			return 1;
		}

		luaL_error(
			L,
			"%s is not a valid member of %s \"%s\"",
			key.c_str(),
			object->className.c_str(),
			object->name.c_str()
		);

		return 0;
	}

	int instanceNewIndex(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);
		const std::string key = luaL_checkstring(L, 2);

		if (object->destroyed)
		{
			luaL_error(
				L,
				"Cannot modify destroyed Instance %s",
				object->name.c_str()
			);
		}

		if (key == "Name")
		{
			const std::string value = luaL_checkstring(L, 3);

			if (object->name != value)
			{
				object->name = value;
				firePropertyChanged(L, object, "Name");
			}

			return 0;
		}

		if (key == "Parent")
		{
			InstanceObject* parent = nullptr;

			if (!lua_isnil(L, 3))
			{
				parent = checkInstance(L, 3);
			}

			setParent(L, object, parent);
			return 0;
		}

		if (key == "Archivable")
		{
			const bool value = lua_toboolean(L, 3) != 0;

			if (object->archivable != value)
			{
				object->archivable = value;
				firePropertyChanged(L, object, "Archivable");
			}

			return 0;
		}

		if (key == "ClassName")
		{
			luaL_error(L, "ClassName is a read-only property");
		}

		if (isBasePartClass(object->className))
		{
			if (key == "Anchored")
			{
				const bool value = lua_toboolean(L, 3) != 0;

				if (object->anchored != value)
				{
					object->anchored = value;
					firePropertyChanged(L, object, "Anchored");
				}

				return 0;
			}

			if (key == "CanCollide")
			{
				const bool value = lua_toboolean(L, 3) != 0;

				if (object->canCollide != value)
				{
					object->canCollide = value;
					firePropertyChanged(L, object, "CanCollide");
				}

				return 0;
			}

			if (key == "Transparency")
			{
				const double value = luaL_checknumber(L, 3);

				if (value < 0.0 || value > 1.0)
				{
					luaL_error(
						L,
						"Transparency must be between 0 and 1"
					);
				}

				if (object->transparency != value)
				{
					object->transparency = value;
					firePropertyChanged(
						L,
						object,
						"Transparency"
					);
				}

				return 0;
			}

			if (key == "Position")
			{
				const auto value =
					RobloxTypes::checkVector3(L, 3);

				object->position = value;
				firePropertyChanged(L, object, "Position");
				return 0;
			}

			if (key == "Size")
			{
				const auto value =
					RobloxTypes::checkVector3(L, 3);

				if (
					value.x <= 0.0 ||
					value.y <= 0.0 ||
					value.z <= 0.0
				)
				{
					luaL_error(
						L,
						"Part Size components must be greater than zero"
					);
				}

				object->size = value;
				firePropertyChanged(L, object, "Size");
				return 0;
			}
		}

		luaL_error(
			L,
			"%s is not a writable property of %s",
			key.c_str(),
			object->className.c_str()
		);

		return 0;
	}

	int instanceToString(lua_State* L)
	{
		InstanceObject* object = checkInstance(L, 1);

		lua_pushlstring(
			L,
			object->name.data(),
			object->name.size()
		);

		return 1;
	}

	int instanceNew(lua_State* L)
	{
		const std::string className =
			luaL_checkstring(L, 1);

		RuntimeContext& runtime = context(L);
		InstanceObject* object =
			createObject(runtime, className);

		pushInstance(L, object);

		if (!lua_isnoneornil(L, 2))
		{
			InstanceObject* parent = checkInstance(L, 2);
			setParent(L, object, parent);
		}

		return 1;
	}

	SignalUserdata* checkSignal(lua_State* L, int index)
	{
		return static_cast<SignalUserdata*>(
			luaL_checkudata(
				L,
				index,
				SIGNAL_METATABLE
			)
		);
	}

	ConnectionUserdata* checkConnection(
		lua_State* L,
		int index
	)
	{
		return static_cast<ConnectionUserdata*>(
			luaL_checkudata(
				L,
				index,
				CONNECTION_METATABLE
			)
		);
	}

	void pushConnection(
		lua_State* L,
		ConnectionState* connection
	)
	{
		auto* userdata =
			static_cast<ConnectionUserdata*>(
				lua_newuserdata(
					L,
					sizeof(ConnectionUserdata)
				)
			);

		userdata->connection = connection;

		luaL_getmetatable(L, CONNECTION_METATABLE);
		lua_setmetatable(L, -2);
	}

	int signalConnectInternal(lua_State* L, bool once)
	{
		SignalUserdata* signal = checkSignal(L, 1);
		luaL_checktype(L, 2, LUA_TFUNCTION);

		if (!signal->object || signal->object->destroyed)
		{
			luaL_error(L, "Cannot connect to destroyed Instance signal");
		}

		auto connection = std::make_unique<ConnectionState>();
		connection->callbackRef = lua_ref(L, 2);
		connection->connected = true;
		connection->once = once;

		ConnectionState* raw = connection.get();

		signal->object->ownedConnections.push_back(
			std::move(connection)
		);

		signal->object->signals[signal->eventName].push_back(
			raw
		);

		pushConnection(L, raw);
		return 1;
	}

	int signalConnect(lua_State* L)
	{
		return signalConnectInternal(L, false);
	}

	int signalOnce(lua_State* L)
	{
		return signalConnectInternal(L, true);
	}

	int signalIndex(lua_State* L)
	{
		checkSignal(L, 1);
		const std::string key = luaL_checkstring(L, 2);

		if (key == "Connect")
		{
			lua_pushcfunction(
				L,
				signalConnect,
				"RBXScriptSignal.Connect"
			);
			return 1;
		}

		if (key == "Once")
		{
			lua_pushcfunction(
				L,
				signalOnce,
				"RBXScriptSignal.Once"
			);
			return 1;
		}

		if (key == "Wait")
		{
			luaL_error(
				L,
				"RBXScriptSignal:Wait requires the scheduler, which is not implemented yet"
			);
		}

		luaL_error(
			L,
			"%s is not a valid member of RBXScriptSignal",
			key.c_str()
		);

		return 0;
	}

	int signalToString(lua_State* L)
	{
		SignalUserdata* signal = checkSignal(L, 1);

		lua_pushfstring(
			L,
			"Signal %s",
			signal->eventName.c_str()
		);

		return 1;
	}

	int connectionDisconnect(lua_State* L)
	{
		ConnectionUserdata* userdata =
			checkConnection(L, 1);

		disconnect(L, userdata->connection);
		return 0;
	}

	int connectionIndex(lua_State* L)
	{
		ConnectionUserdata* userdata =
			checkConnection(L, 1);

		const std::string key = luaL_checkstring(L, 2);

		if (key == "Connected")
		{
			lua_pushboolean(
				L,
				userdata->connection &&
				userdata->connection->connected
			);

			return 1;
		}

		if (key == "Disconnect")
		{
			lua_pushcfunction(
				L,
				connectionDisconnect,
				"RBXScriptConnection.Disconnect"
			);

			return 1;
		}

		luaL_error(
			L,
			"%s is not a valid member of RBXScriptConnection",
			key.c_str()
		);

		return 0;
	}

	int connectionToString(lua_State* L)
	{
		checkConnection(L, 1);
		lua_pushstring(L, "Connection");
		return 1;
	}

	void installMetatables(lua_State* L)
	{
		luaL_newmetatable(L, INSTANCE_METATABLE);

		lua_pushstring(L, "Instance");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(
			L,
			instanceIndex,
			"Instance.__index"
		);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(
			L,
			instanceNewIndex,
			"Instance.__newindex"
		);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(
			L,
			instanceToString,
			"Instance.__tostring"
		);
		lua_setfield(L, -2, "__tostring");

		lua_pop(L, 1);

		luaL_newmetatable(L, SIGNAL_METATABLE);

		lua_pushstring(L, "RBXScriptSignal");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(
			L,
			signalIndex,
			"RBXScriptSignal.__index"
		);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(
			L,
			signalToString,
			"RBXScriptSignal.__tostring"
		);
		lua_setfield(L, -2, "__tostring");

		lua_pop(L, 1);

		luaL_newmetatable(L, CONNECTION_METATABLE);

		lua_pushstring(L, "RBXScriptConnection");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(
			L,
			connectionIndex,
			"RBXScriptConnection.__index"
		);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(
			L,
			connectionToString,
			"RBXScriptConnection.__tostring"
		);
		lua_setfield(L, -2, "__tostring");

		lua_pop(L, 1);
	}

	void addService(
		RuntimeContext& runtime,
		const std::string& className,
		const std::string& name
	)
	{
		InstanceObject* service =
			createObject(runtime, className, name);

		service->parent = runtime.dataModel;
		runtime.dataModel->children.push_back(service);
		runtime.services[name] = service;

		if (name == "Workspace")
			runtime.workspace = service;
	}
}

void RobloxObjectModel::install(lua_State* L)
{
	lua_State* mainThread = lua_mainthread(L);

	if (contexts.find(mainThread) != contexts.end())
		return;

	installMetatables(L);

	auto runtime = std::make_unique<RuntimeContext>();

	runtime->dataModel =
		createObject(*runtime, "DataModel", "game");

	addService(*runtime, "Workspace", "Workspace");
	addService(*runtime, "RunService", "RunService");
	addService(*runtime, "Players", "Players");
	addService(
		*runtime,
		"ReplicatedStorage",
		"ReplicatedStorage"
	);
	addService(
		*runtime,
		"ServerScriptService",
		"ServerScriptService"
	);
	addService(
		*runtime,
		"UserInputService",
		"UserInputService"
	);
	addService(*runtime, "Lighting", "Lighting");
	addService(*runtime, "SoundService", "SoundService");
	addService(*runtime, "TweenService", "TweenService");
	addService(*runtime, "HttpService", "HttpService");
	addService(
		*runtime,
		"CollectionService",
		"CollectionService"
	);
	addService(
		*runtime,
		"PhysicsService",
		"PhysicsService"
	);
	addService(*runtime, "Debris", "Debris");

	RuntimeContext* rawRuntime = runtime.get();
	contexts.emplace(mainThread, std::move(runtime));

	lua_newtable(L);
	lua_pushcfunction(L, instanceNew, "Instance.new");
	lua_setfield(L, -2, "new");
	lua_setglobal(L, "Instance");

	pushInstance(L, rawRuntime->dataModel);
	lua_setglobal(L, "game");

	pushInstance(L, rawRuntime->workspace);
	lua_setglobal(L, "workspace");
}

void RobloxObjectModel::shutdown(lua_State* L)
{
	lua_State* mainThread = lua_mainthread(L);
	auto iterator = contexts.find(mainThread);

	if (iterator == contexts.end())
		return;

	RuntimeContext& runtime = *iterator->second;

	for (auto& object : runtime.objects)
	{
		for (auto& connection : object->ownedConnections)
		{
			disconnect(L, connection.get());
		}
	}

	for (auto& object : runtime.objects)
	{
		if (object->luaRef != LUA_NOREF)
		{
			object->luaRef =
				lua_unref(L, object->luaRef);
		}
	}

	contexts.erase(iterator);
}

bool RobloxObjectModel::isInstance(
	lua_State* L,
	int index
)
{
	if (!lua_isuserdata(L, index))
		return false;

	if (!lua_getmetatable(L, index))
		return false;

	luaL_getmetatable(L, INSTANCE_METATABLE);
	const bool matches =
		lua_rawequal(L, -1, -2) != 0;

	lua_pop(L, 2);
	return matches;
}
