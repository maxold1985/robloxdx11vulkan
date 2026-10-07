local Workspace = game:GetService("Workspace")

local PhysicsSystem = {}
PhysicsSystem.__index = PhysicsSystem

PhysicsSystem.Name = "Physics"
PhysicsSystem.Priority = 20

function PhysicsSystem.new()
	local self = setmetatable({}, PhysicsSystem)

	self.Name = PhysicsSystem.Name
	self.Priority = PhysicsSystem.Priority

	return self
end

function PhysicsSystem:Raycast(origin: Vector3, direction: Vector3, params: RaycastParams?)
	return Workspace:Raycast(origin, direction, params)
end

function PhysicsSystem:CreateRaycastParams(
	filterType: Enum.RaycastFilterType?,
	filterInstances: { Instance }?,
	ignoreWater: boolean?
): RaycastParams
	local params = RaycastParams.new()

	params.FilterType = filterType or Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = filterInstances or {}
	params.IgnoreWater = if ignoreWater == nil then true else ignoreWater

	return params
end

function PhysicsSystem:ApplyImpulse(part: BasePart, impulse: Vector3)
	assert(part:IsA("BasePart"), "ApplyImpulse requires a BasePart")
	part:ApplyImpulse(impulse)
end

function PhysicsSystem:ApplyImpulseAtPosition(part: BasePart, impulse: Vector3, position: Vector3)
	assert(part:IsA("BasePart"), "ApplyImpulseAtPosition requires a BasePart")
	part:ApplyImpulseAtPosition(impulse, position)
end

function PhysicsSystem:SetLinearVelocity(part: BasePart, velocity: Vector3)
	assert(part:IsA("BasePart"), "SetLinearVelocity requires a BasePart")
	part.AssemblyLinearVelocity = velocity
end

function PhysicsSystem:SetAngularVelocity(part: BasePart, velocity: Vector3)
	assert(part:IsA("BasePart"), "SetAngularVelocity requires a BasePart")
	part.AssemblyAngularVelocity = velocity
end

return PhysicsSystem
