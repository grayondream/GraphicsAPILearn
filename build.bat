:: 获取当前脚本的路径
set "scriptPath=%~dp0"

:: 将路径中的最后一个反斜杠去掉
set "rootDir=%scriptPath:~0,-1%"

echo 当前脚本的根目录是: %rootDir%

cmake -S %rootDir% -B build ^
-DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" ^
-DDX_SDK_ROOT="%DIRECTX_SDK_ROOT%" ^
-DWINDOWS_SDK_ROOT="%WINDOWS_SDK_ROOT%"