# RobloxDX11Vulkan

Runtime/engine experimental para executar **Luau escrito no estilo Roblox fora do cliente Roblox**.

O objetivo do projeto é construir uma implementação clean-room da camada de scripting da Roblox Engine:

```text
script .luau
    |
    v
Luau Compiler
    |
    v
Luau VM
    |
    v
Roblox Compatibility API
    |
    +-- game / DataModel
    +-- workspace
    +-- Instance
    +-- Vector3 / CFrame / Color3 ...
    +-- Services
    +-- RBXScriptSignal
    +-- task scheduler
    +-- physics
    +-- renderer DX11/Vulkan
```

## Estado atual

A pasta `native/` é o runtime principal.

Já existe uma primeira camada executável com:

- VM oficial Luau embutida em C++
- compilação de fonte Luau para bytecode
- sandbox por script
- `game:GetService()`
- `workspace`
- `Instance.new()` com identidade nativa estável
- árvore real `Parent/Children`
- acesso a filhos por nome: `workspace.Model.Part`
- `GetChildren()`, `GetDescendants()`, `FindFirstChild()`
- `FindFirstChildOfClass()` e `FindFirstChildWhichIsA()`
- `IsA()`, `IsAncestorOf()`, `IsDescendantOf()`
- `GetFullName()`, `ClearAllChildren()`, `Destroy()`
- `ChildAdded`, `ChildRemoved`, `AncestryChanged`, `Destroying`, `Changed`
- `GetPropertyChangedSignal()`
- `RBXScriptSignal:Connect()` e `:Once()`
- `RBXScriptConnection.Connected` e `:Disconnect()`
- propriedades básicas de BasePart: `Anchored`, `CanCollide`, `Transparency`, `Position`, `Size`
- `Vector3.new()`, aritmética básica, `.Magnitude` e `.Unit`
- `typeof()`
- `warn()`
- serviços iniciais: Workspace, RunService, Players, ReplicatedStorage, ServerScriptService, UserInputService, Lighting, SoundService, TweenService, HttpService, CollectionService, PhysicsService e Debris

Exemplo:

```lua
local Workspace = game:GetService("Workspace")

local part = Instance.new("Part")
part.Name = "EmulatedPart"
part.Position = Vector3.new(1, 2, 3)
part.Parent = Workspace

print(part.Name, part.Position, typeof(part))
```

## Build nativo

Requisitos:

- CMake 3.20+
- compilador C++17
- Git para o FetchContent baixar Luau

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\roblox_runtime.exe native\scripts\test.luau
```

O CMake usa o repositório oficial `luau-lang/luau` e liga `Luau.Compiler` + `Luau.VM`.

## Próximos subsistemas

1. scheduler para `task.spawn`, `task.defer`, `task.delay`, `task.wait`
2. `RBXScriptSignal:Wait()`
3. RunService e loop de frames
4. ModuleScript + `require`
5. `script` e carregamento de hierarquia de arquivos
6. CFrame, Color3, UDim2, Enum
7. Workspace raycast/física
8. propriedades adicionais de Part/Model/Humanoid
9. renderer DirectX 11
10. backend Vulkan

## Importante

Isto não tenta reutilizar binários privados do Roblox. A VM Luau é pública; a camada Roblox precisa ser reimplementada pelo projeto. Luau sozinho não fornece `game`, `Instance`, `Workspace` ou serviços: essas APIs são responsabilidade do host que embute a VM.

A antiga pasta `src/` contém o protótipo inicial de uma framework que rodava dentro do Roblox. Ela não é o runtime principal desta nova arquitetura.


## Validação

O repositório contém `.github/workflows/native-runtime.yml` e um teste CTest chamado `roblox_runtime_smoke`. O smoke test executa `native/scripts/test.luau` e verifica identidade de Instance, Parent/Children, sinais e propriedades básicas.
