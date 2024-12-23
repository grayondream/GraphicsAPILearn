:: 获取当前脚本的路径
set "scriptPath=%~dp0"

:: 将路径中的最后一个反斜杠去掉
set "rootDir=%scriptPath:~0,-1%"

echo 当前脚本的根目录是: %rootDir%

cmake -S %rootDir% -B build ^
-DCMAKE_TOOLCHAIN_FILE="B:/Application/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
-DDX_SDK_ROOT="C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)" ^
-DWINDOWS_SDK_ROOT="C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0" 