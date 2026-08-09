#pragma once
#include "app/GL/GLApp.hpp"
// 已迁移 App 的未来基类别名（全量迁移末尾会把 GLApp 整体重命名为 App）。
// 当前阶段仅作占位，保证 include 链平滑。
class App : public GLApp {
public:
    using GLApp::GLApp;
};
