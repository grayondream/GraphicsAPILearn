@echo off

set GIT_ROOT=

for /f "delims=" %%i in ('git rev-parse --show-toplevel') do (
    set GIT_ROOT=%%i
)

echo Git root directory: %GIT_ROOT%

cmake -S "%GIT_ROOT%" -B "%GIT_ROOT%/build" ^
-DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" ^
-DDX_SDK_ROOT="%DIRECTX_SDK_ROOT%" ^
-DWINDOWS_SDK_ROOT="%WINDOWS_SDK_ROOT%"