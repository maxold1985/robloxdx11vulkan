return {
	Name = "ServerBootstrap",
	Dependencies = {},

	Start = function(context)
		print("[RobloxDX11Vulkan] server runtime started")

		local object = context.Scene:CreateObject("ServerWorld")
		object.Side = "Server"
	end,
}
