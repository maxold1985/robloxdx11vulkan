local Workspace = game:GetService("Workspace")
local RunService = game:GetService("RunService")

assert(Workspace == workspace)
assert(Workspace.ClassName == "Workspace")
assert(RunService.ClassName == "RunService")

local container = Instance.new("Folder")
container.Name = "Container"
container.Parent = Workspace

local childAddedCount = 0

local childAdded = container.ChildAdded:Connect(function(child)
	childAddedCount += 1
	print("ChildAdded:", child.Name, typeof(child))
end)

assert(typeof(container.ChildAdded) == "RBXScriptSignal")
assert(typeof(childAdded) == "RBXScriptConnection")
assert(childAdded.Connected)

local part = Instance.new("Part")
part.Name = "EmulatedPart"
part.Anchored = true
part.Size = Vector3.new(4, 2, 6)
part.Position = Vector3.new(1, 2, 3) + Vector3.new(4, 5, 6)
part.Parent = container

assert(childAddedCount == 1)
assert(container.EmulatedPart == part)
assert(container:FindFirstChild("EmulatedPart") == part)
assert(Workspace:FindFirstChild("EmulatedPart", true) == part)
assert(part:IsA("Part"))
assert(part:IsA("BasePart"))
assert(part:IsDescendantOf(Workspace))
assert(Workspace:IsAncestorOf(part))
assert(#container:GetChildren() == 1)
assert(#Workspace:GetDescendants() >= 2)

print("FullName:", part:GetFullName())
print("Position:", part.Position)
print("Magnitude:", part.Position.Magnitude)
print("Unit:", part.Position.Unit)
print("typeof(part):", typeof(part))

local nameChanged = 0

part:GetPropertyChangedSignal("Name"):Connect(function()
	nameChanged += 1
end)

part.Name = "RenamedPart"

assert(nameChanged == 1)
assert(container.RenamedPart == part)

local destroyed = false

part.Destroying:Once(function()
	destroyed = true
end)

part:Destroy()

assert(destroyed)
assert(container:FindFirstChild("RenamedPart") == nil)

childAdded:Disconnect()
assert(not childAdded.Connected)

warn("Roblox native object model test passed")
