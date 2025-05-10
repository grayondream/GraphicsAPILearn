#pragma once
#include <string>

namespace FileUtils{
    std::string readFile2String(const std::string &file);    

    inline std::string join(const std::string &path1){
        return path1;
    }
    
    inline std::string join(const std::string &path1, const std::string &path2) {
        if (path1.empty()) {
            return path2;
        }
        if (path2.empty()) {
            return path1;
        }
    
        // 确保使用正斜杠
        std::string result = path1;
    
        // 去掉最后的分隔符
        while(result.back() == '/' || result.back() == '\\') {
            result.pop_back();
        }
    
        // 添加分隔符和第二个路径
        result += '/' + path2;

        return result;
    }
    
    template<typename... Args>
    std::string join(const std::string &path, Args... args) {
        return join(path, join(args...));
    }
}