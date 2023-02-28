#pragma once

class ShaderProgram;
class ComputeShader {
private:
    ShaderProgram* program;
    unsigned int blockID;
    unsigned int bindpoint;

public:

    ComputeShader();
    
    void SetShader(ShaderProgram* _program) { program = _program; }
    unsigned int GetBindPoint() const& { return bindpoint; }
    unsigned int GetBlockID()   const& { return blockID; }
};