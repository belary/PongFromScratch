#include "logger.h"

void vk_compile_shader(char* shaderPath, char* spirvPath)
{
    CAKEZ_ASSERT(shaderPath, "No Shader Path supplied");
    CAKEZ_ASSERT(spirvPath, "No Spriv Path supplied");

    char command[320] = {};
    sprintf(command, "lib\\glslc.exe %s -o %s", shaderPath, spirvPath);
    int result = system(command);
    CAKEZ_ASSERT(!result, "Failed to compile shader: %s", shaderPath);
}