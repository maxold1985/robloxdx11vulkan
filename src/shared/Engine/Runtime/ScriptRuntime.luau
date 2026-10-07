local ModuleRegistry = require(script.Parent.ModuleRegistry)

local ScriptRuntime = {}
ScriptRuntime.__index = ScriptRuntime

local function validateDependencies(descriptor)
	for index, dependencyName in descriptor.Dependencies do
		if type(dependencyName) ~= "string" or dependencyName == "" then
			error(("Invalid dependency #%d in '%s'"):format(index, descriptor.Name))
		end
	end
end

function ScriptRuntime.new()
	local self = setmetatable({}, ScriptRuntime)

	self.Registry = ModuleRegistry.new()
	self._resolved = {}
	self._started = false

	return self
end

function ScriptRuntime:Register(descriptor)
	validateDependencies(descriptor)
	self.Registry:Register(descriptor)
	table.clear(self._resolved)
	return descriptor
end

function ScriptRuntime:LoadModule(moduleScript: ModuleScript)
	assert(moduleScript:IsA("ModuleScript"), "LoadModule expects a ModuleScript")

	local ok, result = pcall(require, moduleScript)
	if not ok then
		error(("Failed to require %s: %s"):format(moduleScript:GetFullName(), tostring(result)))
	end

	if type(result) ~= "table" then
		error(("Runtime ModuleScript %s must return a descriptor table"):format(moduleScript:GetFullName()))
	end

	return self:Register(result)
end

function ScriptRuntime:LoadFolder(root: Instance)
	local loaded = {}

	for _, descendant in root:GetDescendants() do
		if descendant:IsA("ModuleScript") then
			table.insert(loaded, self:LoadModule(descendant))
		end
	end

	return loaded
end

function ScriptRuntime:_resolve()
	if #self._resolved > 0 then
		return self._resolved
	end

	local visiting = {}
	local visited = {}
	local result = {}

	local function visit(name: string)
		if visited[name] then
			return
		end

		if visiting[name] then
			error(("Circular runtime dependency detected at '%s'"):format(name))
		end

		local descriptor = self.Registry:Get(name)
		if descriptor == nil then
			error(("Runtime dependency '%s' was not registered"):format(name))
		end

		visiting[name] = true

		for _, dependencyName in descriptor.Dependencies do
			visit(dependencyName)
		end

		visiting[name] = nil
		visited[name] = true
		table.insert(result, descriptor)
	end

	for _, name in self.Registry:GetNames() do
		visit(name)
	end

	self._resolved = result
	return result
end

function ScriptRuntime:Init(context)
	for _, descriptor in self:_resolve() do
		if type(descriptor.Init) == "function" then
			descriptor.Init(context)
		end
	end
end

function ScriptRuntime:Start(context)
	if self._started then
		return
	end

	self._started = true
	self:Init(context)

	for _, descriptor in self:_resolve() do
		if type(descriptor.Start) == "function" then
			descriptor.Start(context)
		end
	end
end

function ScriptRuntime:Update(deltaTime: number, context)
	if not self._started then
		return
	end

	for _, descriptor in self:_resolve() do
		if type(descriptor.Update) == "function" then
			descriptor.Update(deltaTime, context)
		end
	end
end

function ScriptRuntime:Stop(context)
	if not self._started then
		return
	end

	local resolved = self:_resolve()

	for index = #resolved, 1, -1 do
		local descriptor = resolved[index]
		if type(descriptor.Stop) == "function" then
			descriptor.Stop(context)
		end
	end

	self._started = false
end

return ScriptRuntime
