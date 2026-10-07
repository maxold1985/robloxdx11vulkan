local EngineObject = require(script.Parent.Object)

local Scene = {}
Scene.__index = Scene

function Scene.new()
	local self = setmetatable({}, Scene)

	self._objects = {}
	self._order = {}

	return self
end

function Scene:CreateObject(name: string?)
	local object = EngineObject.new(name)
	self:AddObject(object)
	return object
end

function Scene:AddObject(object)
	assert(object ~= nil and object.Id ~= nil, "Invalid engine object")

	if self._objects[object.Id] ~= nil then
		error(("Object '%s' is already in this scene"):format(object.Name))
	end

	self._objects[object.Id] = object
	table.insert(self._order, object)

	return object
end

function Scene:GetObject(id: string)
	return self._objects[id]
end

function Scene:FindByName(name: string)
	for _, object in self._order do
		if not object.Destroyed and object.Name == name then
			return object
		end
	end

	return nil
end

function Scene:GetObjects()
	return table.clone(self._order)
end

function Scene:RemoveObject(objectOrId)
	local id = if type(objectOrId) == "string" then objectOrId else objectOrId.Id
	local object = self._objects[id]

	if object == nil then
		return nil
	end

	self._objects[id] = nil

	local index = table.find(self._order, object)
	if index ~= nil then
		table.remove(self._order, index)
	end

	return object
end

function Scene:Update(deltaTime: number)
	for _, object in self._order do
		if not object.Destroyed then
			object:Update(deltaTime)
		end
	end
end

function Scene:Destroy()
	for index = #self._order, 1, -1 do
		local object = self._order[index]
		object:Destroy()
		self._objects[object.Id] = nil
		self._order[index] = nil
	end
end

return Scene
