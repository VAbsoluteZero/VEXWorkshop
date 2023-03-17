#include "GfxUtils.h"

#include <SDL.h>
#include <SDL_filesystem.h>

#include <filesystem>
#include <fstream>

void vex::TextShaderLib::build(const char* rel_path)
{
    path_to_dir = rel_path;
    vex::InlineBufferAllocator<1024 * 20> stackbuf;
    auto al = stackbuf.makeAllocatorHandle();

    for (const auto& entry : std::filesystem::directory_iterator(path_to_dir))
    {
        const auto& path = entry.path();
        auto file_name = path.string();

        std::ifstream m_stream(file_name, std::ios::binary | std::ios::ate);

        size_t size = std::filesystem::file_size(file_name);

        char* buffer = (char*)al.alloc(size + 1, 1);
        m_stream.seekg(0);
        m_stream.read(buffer, size);
        if (size == 0)
            continue;

        auto ext = path.extension().string();
        auto type = [](std::string& ext) -> ShaderSourceType
        {
            if (ext == ".wgsl")
                return ShaderSourceType::WGSL;
            else if (ext == ".hlsl")
                return ShaderSourceType::HLSL;
            return ShaderSourceType::Invalid;
        }(ext);
        buffer[size] = '\0';
        this->shad_src.emplace(std::move(file_name), ShaderSource{buffer, type});

        stackbuf.reset();
    }

    SPDLOG_INFO("shaders found : {} :", this->shad_src.size());
    for (auto& [k, entry] : this->shad_src)
    {
        SPDLOG_INFO("-- {}", k);
    }
}

void vex::TextShaderLib::reload()
{ 
    vex::InlineBufferAllocator<1024 * 20> stackbuf;
    auto al = stackbuf.makeAllocatorHandle(); 
    for (auto& [file_name, entry] : this->shad_src)
    {   
        std::ifstream m_stream(file_name, std::ios::binary | std::ios::ate);

        size_t size = std::filesystem::file_size(file_name);

        char* buffer = (char*)al.alloc(size + 1, 1);
        m_stream.seekg(0);
        m_stream.read(buffer, size);
        if (size == 0)
            continue; 
        buffer[size] = '\0';

        entry.text = buffer;

        stackbuf.reset();
    }
}
