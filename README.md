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


## Renderer DirectX 11 mínimo

No Windows, o runtime possui agora um renderer DX11 que transforma os `BasePart` presentes no `Workspace` em cubos renderizados.

Fluxo:

```text
CubeServer.server.lua
CubeClient.client.lua
        |
        v
Luau VM
        |
        v
Workspace / Part / Camera
        |
        v
Render snapshots
        |
        v
Dx11Renderer
        |
        v
Janela Win32
```

O renderer usa atualmente:

- `BasePart.Position`
- `BasePart.Size`
- `BasePart.Color`
- `BasePart.Transparency`
- `Workspace.CurrentCamera.CameraSubject`
- input real de W/A/S/D/Space via `UserInputService`
- depth buffer
- iluminação direcional simples
- grid visual de debug, que não participa da física nem do `Workspace`

Para executar os dois scripts do cubo:

```bat
run_cube_dx11.bat
```

Ou diretamente:

```bat
build\Release\roblox_runtime.exe --render native\scripts\cube\CubeServer.server.lua native\scripts\cube\CubeClient.client.lua
```

Se `--render` for usado sem scripts, esses dois arquivos são carregados automaticamente.

O renderer atual é propositalmente mínimo: `Part` é desenhado como box sem rotação. `CFrame`, meshes, texturas, iluminação Roblox completa, materiais e Vulkan serão camadas posteriores.


## Backend OpenGL ES 3.2

Em sistemas Unix com X11, EGL e headers GLES3 disponíveis, o runtime também compila o backend `Gles32Renderer`.

Fluxo:

```text
Luau / Workspace
        |
        v
RenderPartSnapshot
        |
        v
Gles32Renderer
        |
        +-- X11 window
        +-- EGL
        +-- OpenGL ES 3.2
        +-- GLSL ES 320
```

O backend GLES 3.2 possui atualmente:

- janela X11
- contexto EGL OpenGL ES
- validação de que o contexto expõe OpenGL ES 3.2 ou superior
- cubos para `BasePart`
- `Position`, `Size`, `Color` e `Transparency`
- depth test
- alpha blending
- iluminação direcional simples
- grid de debug
- câmera seguindo `CameraSubject`
- input W/A/S/D/Space
- resize da viewport

### Termux / Acode + Termux:X11

Instale as dependências no Termux:

```sh
pkg update
pkg install x11-repo
pkg install git cmake ninja clang libx11 mesa
```

Com o Termux:X11 iniciado, configure o display conforme sua instalação. O valor comum é:

```sh
export DISPLAY=:0
```

Build:

```sh
chmod +x build_termux_gles.sh
./build_termux_gles.sh
```

Executar o exemplo:

```sh
chmod +x run_cube_gles.sh
./run_cube_gles.sh
```

Ou diretamente:

```sh
./build_gles32/roblox_runtime --render-gles
```

No Windows, `--render` continua usando DirectX 11. Em Unix, quando o backend GLES foi encontrado pelo CMake, `--render` usa GLES 3.2 automaticamente.

O backend atual usa X11 para criar a janela. Ele é adequado para Termux:X11/Linux. Um backend Android nativo baseado em `ANativeWindow` para APK será uma camada separada.
