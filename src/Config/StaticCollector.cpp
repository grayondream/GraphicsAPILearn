#include "StaticCollector.hpp"

constexpr const char * kResourceRoot = "res";

namespace StaticCollector {
    std::filesystem::path getResPath(){
        return std::filesystem::current_path() / kResourceRoot;
    }
}