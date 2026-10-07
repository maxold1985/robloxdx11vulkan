return {
	Name = "ClientBootstrap",
	Dependencies = {},

	Start = function(context)
		print("[RobloxDX11Vulkan] client runtime started")

		local object = context.Scene:CreateObject("ClientWorld")
		object.Side = "Client"
	end,
}
