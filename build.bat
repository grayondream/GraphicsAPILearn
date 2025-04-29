cmake -S . -B build ^
-DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" ^
-DDX_SDK_ROOT="%DIRECTX_SDK_ROOT%" ^
-DWINDOWS_SDK_ROOT="%WINDOWS_SDK_ROOT%"