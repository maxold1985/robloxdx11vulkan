local ModuleRegistry = {}
ModuleRegistry.__index = ModuleRegistry

function ModuleRegistry.new()
	local self = setmetatable({}, ModuleRegistry)

	self._modules = {}
	self._order = {}

	return self
end

function ModuleRegistry:Register(descriptor)
	assert(type(descriptor) == "table", "Runtime module must return a table")
	assert(type(descriptor.Name) == "string" and descriptor.Name ~= "", "Runtime module requires Name")

	if self._modules[descriptor.Name] ~= nil then
		error(("Runtime module '%s' is already registered"):format(descriptor.Name))
	end

	descriptor.Dependencies = descriptor.Dependencies or {}
	self._modules[descriptor.Name] = descriptor
	table.insert(self._order, descriptor.Name)

	return descriptor
end

function ModuleRegistry:Get(name: string)
	return self._modules[name]
end

function ModuleRegistry:GetNames()
	return table.clone(self._order)
end

function ModuleRegistry:Clear()
	table.clear(self._modules)
	table.clear(self._order)
end

return ModuleRegistry
