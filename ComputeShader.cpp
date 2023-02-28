#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
using namespace gl;

#include "ComputeShader.h"

ComputeShader::ComputeShader() : bindpoint(0) {
    glGenBuffers(1, &blockID);
}