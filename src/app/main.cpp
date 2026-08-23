

#include <cxxopts.hpp>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

#include "base/Log.hpp"
#include "app/AppFactory.hpp"
#include "app/IApplication.hpp"
#include "utils/EnumUtil.hpp"
#include "base/Constexpr.hpp"

using namespace Constexpr;

namespace EnumUtil = Utils::Enum;

static std::vector<std::pair<std::string, GraphicsType>> AvailableBackends() {
    std::vector<std::pair<std::string, GraphicsType>> v;
#if ENABLE_OPENGL
    v.emplace_back("gl", GraphicsType::GL);
#endif
#if ENABLE_VULKAN
    v.emplace_back("vulkan", GraphicsType::Vulkan);
#endif
#if ENABLE_DX12
    v.emplace_back("dx12", GraphicsType::DX12);
#endif
    return v;
}

static std::string_view StripEnumPrefix(const std::string_view name) {
    const auto pos = name.rfind("::");
    return pos == std::string_view::npos ? name : name.substr(pos + 2);
}

static std::optional<AppType> FindAppType(const std::string& name) {
    for (int i = 0; i < static_cast<int>(AppType::Count); ++i) {
        const auto t = static_cast<AppType>(i);
        if (StripEnumPrefix(EnumUtil::EnumName(t)) == name) {
            return t;
        }
    }
    return std::nullopt;
}

static std::string ListAppNames() {
    std::string s;
    for (int i = 0; i < static_cast<int>(AppType::Count); ++i) {
        if (i > 0) s += " ";
        s += StripEnumPrefix(EnumUtil::EnumName(static_cast<AppType>(i)));
    }
    return s;
}

static std::string ListBackendNames(const std::vector<std::pair<std::string, GraphicsType>>& backends) {
    std::string s;
    for (size_t i = 0; i < backends.size(); ++i) {
        if (i > 0) s += " ";
        s += backends[i].first;
    }
    return s;
}

int main(int argc, char **argv) {
    auto backends = AvailableBackends();

    cxxopts::Options options("renderLearn", "Graphics Learn example runner");
    options.add_options()
        ("b,backend", "graphics backend (default: gl)", cxxopts::value<std::string>()->default_value("gl"))
        ("a,app", "example app name (default: Triangle)", cxxopts::value<std::string>()->default_value("Triangle"))
        ("h,help", "print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        std::cout << "Available backends:\n  " << ListBackendNames(backends) << std::endl;
        std::cout << "Available apps:\n  " << ListAppNames() << std::endl;
        return 0;
    }

    const std::string backendName = result["backend"].as<std::string>();
    const std::string appName = result["app"].as<std::string>();

    std::optional<GraphicsType> gtype;
    for (const auto& [name, type] : backends) {
        if (name == backendName) {
            gtype = type;
            break;
        }
    }
    if (!gtype) {
        LOGE("Unknown backend '{}'. Available backends: {}", backendName, ListBackendNames(backends));
        return 1;
    }

    auto type = FindAppType(appName);
    if (!type) {
        LOGE("Unknown app '{}'. Available apps:\n{}", appName, ListAppNames());
        return 1;
    }

    LOGI("Start Graphics Learn!!!");
    LOGI("Select {} Application, Render App With {} API", StripEnumPrefix(EnumUtil::EnumName(*type)), StripEnumPrefix(EnumUtil::EnumName(*gtype)));
    auto app = AppFactory::create(*gtype, *type);
    assert(app);
    if (!app) {
        LOGE("Failed to create app '{}' with backend '{}'", appName, backendName);
        return 1;
    }

    GLFWWindowProperties props(
        "Pure Window (No Graphics API)",
        GetWindowWidth(), GetWindowHeight(),  // 宽高
        200, 200 );
    props.title = "Hello Graphics!";
    props.vsync = false;
    props.vulkan = (*gtype == GraphicsType::Vulkan);
    if (!app->init(props)) {
        LOGE("App init failed for app '{}' with backend '{}'", appName, backendName);
        return 1;
    }
    return app->run();
}
