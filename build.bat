@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set includs=/Isrc /I%VULKAN_SDK%/Include
set links=/link /LIBPATH:%VULKAN_SDK%/Lib vulkan-1.lib
set defines=/D DEBUG

echo "Building..."

cl /EHsc %includs% %defines% src/main.cpp %links% 
