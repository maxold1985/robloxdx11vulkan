local HttpService = game:GetService("HttpService")

local EngineObject = {}
EngineObject.__index = EngineObject

function EngineObject.new(name: string?)
	local self = setmetatable({}, EngineObject)

	self.Id = HttpService:GenerateGUID(false)
	self.Name = name or "Object"
	self.Components = {}
	self.Destroyed = false

	return self
end

function EngineObject:AddComponent(component)
	assert(not self.Destroyed, "Cannot add a component to a destroyed object")
	assert(type(component) == "table", "Component must be a table")
	assert(type(component.Name) == "string" and component.Name ~= "", "Component requires Name")

	if self.Components[component.Name] ~= nil then
		error(("Component '%s' already exists on '%s'"):format(component.Name, self.Name))
	end

	component.Owner = self
	self.Components[component.Name] = component

	if type(component.OnAttach) == "function" then
		component:OnAttach(self)
	end

	return component
end

function EngineObject:GetComponent(name: string)
	return self.Components[name]
end

function EngineObject:HasComponent(name: string): boolean
	return self.Components[name] ~= nil
end

function EngineObject:RemoveComponent(name: string)
	local component = self.Components[name]
	if component == nil then
		return nil
	end

	if type(component.OnDetach) == "function" then
		component:OnDetach()
	end

	component.Owner = nil
	self.Components[name] = nil

	return component
end

function EngineObject:Update(deltaTime: number)
	for _, component in self.Components do
		if type(component.Update) == "function" then
			component:Update(deltaTime)
		end
	end
end

function EngineObject:Destroy()
	if self.Destroyed then
		return
	end

	self.Destroyed = true

	for name, component in self.Components do
		if type(component.Destroy) == "function" then
			component:Destroy()
		elseif type(component.OnDetach) == "function" then
			component:OnDetach()
		end

		component.Owner = nil
		self.Components[name] = nil
	end
end

return EngineObject
