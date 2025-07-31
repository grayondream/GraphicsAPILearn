```bash
              ________                    .__    .__               
             /  _____/___________  ______ |  |__ |__| ____   ______
            /   \  __\_  __ \__  \ \____ \|  |  \|  |/ ___\ /  ___/
            \    \_\  \  | \// __ \|  |_> >   Y  \  \  \___ \___ \ 
             \______  /__|  (____  /   __/|___|  /__|\___  >____  >
                    \/           \/|__|        \/        \/     \/ 
            _____ __________.___  .____                               
           /  _  \\______   \   | |    |    ____ _____ _______  ____  
          /  /_\  \|     ___/   | |    |  _/ __ \\__  \\_  __ \/    \ 
         /    |    \    |   |   | |    |__\  ___/ / __ \|  | \/   |  \
         \____|__  /____|   |___| |_______ \___  >____  /__|  |___|  /
                 \/                       \/   \/     \/           \/  
```

# Graphics API Learn

&emsp;&emsp;The project is a graphics API learning initiative aimed at understanding the principles, implementations, and applications of different graphics APIs. The goal is to support various platforms and graphics APIs, with expected support for Windows, macOS, Linux, and graphics APIs such as OpenGL, DirectX, Metal, and Vulkan.

## Environment

&emsp;&emsp;The current project is expected to support the Windows platform, using vcpkg to manage third-party libraries. The target environments for different platforms are as follows:
- [x] Windows, Development Environment: Windows 10, Visual Studio 2019+
  - [ ] OpenGL, in development
  - [ ] DirectX
  - [ ] Vulkan
- [ ] macOS
- [ ] Linux

&emsp;&emsp;The project relies on several third-party libraries managed via vcpkg. The current dependencies are as follows:
- OpenGL
- assimp
- spdlog
- glm
- stb_image
- imgui

## Features
&emsp;&emsp;As the project is still under development, relevant support details can be found in [TODO.md](TODO.md).

## Build
&emsp;&emsp;The current project uses CMake for building; therefore, you need to ensure that CMake and the corresponding C++ development environment for your platform are installed locally.

### Windows
&emsp;&emsp;Windows uses Visual Studio 2019+ as the development environment, so you need to have Visual Studio 2019+ installed. Since DirectX is a dependency, you also need to install the DirectX SDK and set the `DIRECTX_SDK_ROOT` and `WINDOWS_SDK_ROOT`. Third-party dependencies can be installed via vcpkg, which requires setting `VCPKG_ROOT`. Execute the following script to install dependencies:
```bash
./Script/install_dependency.bat
```

&emsp;&emsp;After installation, execute the following script to generate the project:
```bash
./Script/generate_proj.bat
```

&emsp;&emsp;Once the dependencies and generated project files are ready, open Visual Studio 2019+, load the generated project file, and you can start compiling and debugging. Note that the current project is in development and does not support command line input parameters or real-time changes to rendering instances; you need to modify the AppType in main.cpp to select the rendering target features.