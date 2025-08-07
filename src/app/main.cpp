

#include "Base/Log.hpp"
#include "App/AppFactory.hpp"
#include "App/IApplication.hpp"
#include "Utils/EnumUtil.hpp"
#include "Base/Constexpr.hpp"

using namespace Constexpr;
/*
 * Application List:
 * Base: Draw a empty window
 *  Base:                           clear window's color into one color by OpenGL
 *  Triangle                        draw a colored triangle by OpenGL
 *  Rect                            draw a colored rect by OpenGL
 *  SimpleTexture                   read a image into texture and draw it on a rect by OpenGL
 *  Cube                            draw a cube by OpenGL
 *  Camera                          create a virtual camera
 *  SimpleLight_Ambination          Global Illumination
 *  SimpleLight_Diffuse             diffuse light
 *  SimpleLight_Specular            Specular light
 *  SimpleLight_Material            Material
 *  SimpleLight_Map                 Light Map
 *  SimpleLight_Source_Direction    Direction Light Source
 *  SimpleLight_Source_Point        Point Light Source
 *  SimpleLight_Source_Spot         Spot Light Source
 *  SimpleLight_Source_Mult         Multiple Light Source
 *  LoadModel                       Load Model
 *  DepthTest                       Depth Test
 *  TemplateTest                    Template Test
 *  Blend                           Blend Test
 *  CullFace                        Cull Face Test
 *  FrameBuffer                     Frame Buffer Test
 *  SkyBox                          Render a skybox around the camera
 *  AdvancedGLSL                    Advanced GLSL Test
 *  UniformBuffer                   Uniform Buffer Test
 *  SimpleGeometry                  Simple Geometry Test
 *  Explode                         Explode Geometry Test
 *  NormalLine                      Draw Normal Line
 *  MultiInstance                   draw multiple instance
 *  MultiInstance_Saturn            draw multiple instance with saturn model
 *  Msaa                            Multi Sample Anti Aliasing
 *  BlinnPhong                      Blinn-Phong Lighting Model
 *  Gamma                           Gamma Correction
 *  Shadow_Map                      Shadow Mapping
 *  Shadow                          Render Shadow 
 *  Shadow_PointLight               Render Point light shadow
 *  NormalMap                       render object with normal map
 *  ParallaxMap                     render object with parallax map
 *  Hdr                             render hdr scene
 *  Bloom                           rende bloom light
 */

// namespace EnumUtil = Utils::Enum;

// int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {
//     ConsoleDebugger consoleDebugger{};
    
//     auto type = AppType::Bloom;
//     auto api = GraphicsType::GL;
//     LOGI("Start Graphics Learn!!!");
//     LOGI("Select {} Application, Render App With {} API", EnumUtil::EnumName(type), EnumUtil::EnumName(api));
//     auto app = AppFactory::create(api, type);
//     assert(app);
//     app->init(hInstance, { {GetWindowWidth(), GetWindowHeight(), "Hello Grapgic!"}, GetEnableMsaa()});
//     return app->run(nShowCmd);
// }


#include "GLFWWindow.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char **argv) {
    // 配置窗口属性
    GLFWWindowProperties props(
        "Pure Window (No Graphics API)",
        800, 600,  // 宽高
        200, 200,  // 位置
        true,      // 可 resize
        false      // 窗口模式
    );

    // 创建窗口实例
    GLFWWindow window(props);

    // 初始化窗口（仅创建原生窗口，无任何图形API）
    if (!window.initialize()) {
        std::cerr << "Failed to initialize window" << std::endl;
        return -1;
    }

    // 主循环（仅处理窗口事件，无渲染逻辑）
    std::cout << "Window running - press ESC to close, F1 to toggle fullscreen" << std::endl;
    while (!window.shouldClose()) {
        // 轮询事件（必须调用，否则窗口无响应）
        window.pollEvents();

        // 简单延迟，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 销毁窗口
    window.shutdown();
    return 0;
}