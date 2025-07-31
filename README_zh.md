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

&emsp;&emsp;项目是一个图形API学习项目，旨在学习不同图形API的原理、实现和应用。目标支持不同平台不同图形API，预期支持Windows、macOS、Linux等平台，支持不同的图形API，如OpenGL、DirectX、Metal、Vulkan等。

## 环境

&emsp;&emsp;当前项目预期支持Windows平台，使用vcpkg管理第三方库，不同平台目标环境如下：
- [x] Windows，开发环境：Windows10，Visual Studio 2019+
  - [ ] OpenGL，开发中
  - [ ] DirectX
  - [ ] Vulkan
- [ ] macOS
- [ ] Linux

&emsp;&emsp;项目依赖一部分三方库，通过vcpkg管理，当前项目依赖的三方库如下：
- OpenGL；
- assimp；
- spdlog；
- glm；
- stb_image；
- imgui

## Fetaures
&emsp;&emsp;由于仍然在开发中，相关支持内容见[TODO.md](TODO.md)
## 构建
&emsp;&emsp;当前项目使用cmake构建，因此首先需要确认你本地安装了cmake和相应平台的C++开发环境。
### Windows
&emsp;&emsp;Windows使用Visual Studio 2019+作为开发环境，需要安装好Visual Studio 2019+。由于依赖DirectX，因此需要安装好DirectX SDK，并设置`DIRECTX_SDK_ROOT`和`WINDOWS_SDK_ROOT`。第三方依赖可以通过vcpkg安装，vcpkg需要设置`VCPKG_ROOT`，执行下面的脚本安装：
```bash
./Script/install_dependency.bat
```

&emsp;&emsp;安装完成后，执行下面的脚本生成项目：
```bash
./Script/generate_proj.bat
```

&emsp;&emsp;当依赖和生成的项目文件都准备好后，打开Visual Studio 2019+，打开生成的项目文件，即可开始编译和调试。需要注意的是当前项目处于开发中不支持命令行输入参数和实时更改渲染实例，需要修改`main.cpp`中的`AppType`来选择渲染的目标特性。

## 改进
- [ ] 支持命令行输入参数，如指定渲染目标、渲染模式等；
- [ ] 支持实时更改渲染实例，如实时更改渲染目标、渲染模式等；
- [ ] 完全隔离不同API
- [ ] 项目结构不合理，需要对项目结构进行优化，如将不同的渲染目标、渲染模式等进行分类，分别放在不同的文件中；
- [ ] 部分App存在内存泄漏问题，需要进行修复；
- [ ] 部分代码不合理，需要进行调整或者删除

- 