#pragma once
#include <string>
#include <GL/glew.h>
#include <iostream>

class Shader {
public:
    GLuint id = 0;

    Shader() = default;
    Shader(const char* vertexSrc, const char* fragmentSrc) { Compile(vertexSrc, fragmentSrc); }

    void Compile(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertexSrc, nullptr);
        glCompileShader(vs);
        Check(vs, "VERTEX");

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragmentSrc, nullptr);
        glCompileShader(fs);
        Check(fs, "FRAGMENT");

        id = glCreateProgram();
        glAttachShader(id, vs);
        glAttachShader(id, fs);
        glLinkProgram(id);
        Check(id, "PROGRAM");

        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    void Use() const { glUseProgram(id); }

private:
    void Check(GLuint obj, const std::string& type) {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(obj, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(obj, 1024, nullptr, infoLog);
                std::cerr << "[Shader] Compile error (" << type << "):\n" << infoLog << "\n";
            }
        }
        else {
            glGetProgramiv(obj, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(obj, 1024, nullptr, infoLog);
                std::cerr << "[Shader] Link error:\n" << infoLog << "\n";
            }
        }
    }
};
