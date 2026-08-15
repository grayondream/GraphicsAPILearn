#pragma once
#include "IApplication.hpp"
#include "GLFWWindow.hpp"
#include "IImGuiWindow.hpp"
#include "app/sample/Sample.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

// AppHost：驱动 Sample 的宿主（GL 后端运行器）。Task 1 迁移样例后，
// 旧 GLAppFactory 仍返回 IApplication，故以 AppHost 包装 Sample 接入既有 main 运行链。
// Task 3 将把 AppHost 重构为正式总控宿主并删除本临时实现。
class AppHost : public IApplication {
public:
    explicit AppHost(std::shared_ptr<Sample> sample);
    ~AppHost();

    bool init(const GLFWWindowProperties& properties) override;
    int run() override;
    void exit() override;
    unsigned int getSampleCount() const override;

private:
    void initInput();

    std::shared_ptr<Sample> _sample{};
    std::unique_ptr<GLFWWindow> _window{};
    std::unique_ptr<IImGuiWindow> _imguiWindow{};
    std::shared_ptr<rhi::IRenderer> _renderer{};
};
