#pragma once

#include <iostream>
#include <format>
#include <string>

namespace base{
    namespace log{
        template<typename ...Args>
        void Logg(std::format_string<Args...> fmt, Args&& ...args) {
            printf("%s\n", std::format(fmt, std::forward<Args>(args)...).c_str());
        }

    }
}


using base::log::Logg;

#define LOGI Logg
#define LOGW Logg
#define LOGE Logg
