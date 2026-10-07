local Workspace = game:GetService("Workspace")
local RunService = game:GetService("RunService")

local part = Instance.new("Part")
part.Name = "EmulatedPart"
part.Anchored = true
part.Position = Vector3.new(1, 2, 3) + Vector3.new(4, 5, 6)
part.Parent = Workspace

print("Class:", part.ClassName)
print("Name:", part.Name)
print("Position:", part.Position)
print("typeof(part):", typeof(part))
print("Workspace:", Workspace.Name)
print("RunService:", RunService.Name)

warn("Roblox compatibility layer is active")
