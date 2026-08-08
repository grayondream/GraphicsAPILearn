

#include "base/Log.hpp"
#include "app/AppFactory.hpp"
#include "app/IApplication.hpp"
#include "utils/EnumUtil.hpp"
#include "base/Constexpr.hpp"

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
 *  AdvancedShader                    Advanced Shader Test
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
 *  Defer                           render defer scene
 *  SSAO                            render SSAO scene
 *  PBR_Base                        render PBR base scene
 *  PBR_Texture                     render PBR texture scene
 *  PBR_IBL_Irradiance_Conversion   render PBR IBL Irradiance Conversion scene
 *  PBR_IBL_Irradiance              render PBR IBL Irradiance scene
 *  PBR_IBL_Specular                render PBR IBL Specular scene
 */

namespace EnumUtil = Utils::Enum;

int main(int argc, char **argv) {
    auto type = AppType::PBR_IBL_Specular;
    auto api = GraphicsType::GL;
    LOGI("Start Graphics Learn!!!");
    LOGI("Select {} Application, Render App With {} API", EnumUtil::EnumName(type), EnumUtil::EnumName(api));
    auto app = AppFactory::create(api, type);
    assert(app);

    GLFWWindowProperties props(
        "Pure Window (No Graphics API)",
        GetWindowWidth(), GetWindowHeight(),  // 宽高
        200, 200 );
    props.title = "Hello Graphics!";
    props.vsync = false;
    app->init(props);
    return app->run();
}
