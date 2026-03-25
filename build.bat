@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set includs=/Isrc /I%VULKAN_SDK%/Include
set links=/link /LIBPATH:%VULKAN_SDK%/Lib vulkan-1.lib user32.lib
set defines=/D DEBUG

echo "Building..."

cl /EHsc /Z7 /Fe"main" %includs% %defines% src/platform/win32_platform.cpp %links% 
