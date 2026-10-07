local Players = game:GetService("Players")
local Workspace = game:GetService("Workspace")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local player = Players.LocalPlayer
local cube = Workspace:FindFirstChild("Cube_" .. player.UserId)
local remote = ReplicatedStorage:FindFirstChild("CubeControl")

assert(cube ~= nil, "server did not create the player cube")
assert(remote ~= nil, "server did not create CubeControl RemoteEvent")
assert(cube:GetAttribute("OwnerUserId") == player.UserId)
assert(cube.Color == Color3.fromRGB(0, 170, 255))
assert(cube.Material == Enum.Material.SmoothPlastic)
assert(Workspace.CurrentCamera.CameraSubject == cube)

local velocity = cube.AssemblyLinearVelocity
assert(math.abs(velocity.Z + 24) < 0.001, "W input did not reach server movement code")

print("[cube smoke] cube:", cube.Name)
print("[cube smoke] velocity:", velocity)
print("[cube smoke] camera subject:", Workspace.CurrentCamera.CameraSubject)
