#include "StaticCollector.hpp"

#ifndef RESOURCE_DIR
constexpr const char * kResourceRoot = "res";
#else
constexpr const char* kResourceRoot = RESOURCE_DIR;
#endif//RESOURCE_DIR

namespace StaticCollector {
    std::filesystem::path getResPath(){
        return std::filesystem::current_path() / kResourceRoot;
    }
}