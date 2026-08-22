#include "SampleFactory.hpp"
#include "app/sample/Sample.hpp"
#include "app/sample/BaseSample.hpp"
#include "app/Samples/Base/TriangleApp.hpp"
#include "app/Samples/Base/RectApp.hpp"
#include "app/Samples/Base/SimpleTextureApp.hpp"
#include "app/Samples/Base/CubeApp.hpp"
#include "app/Samples/Base/CameraApp.hpp"
#include "app/Samples/Light/SimpleLightAmbination.hpp"
#include "app/Samples/Light/SimpleLightDiffuse.hpp"
#include "app/Samples/Light/SimpleLightSpecular.hpp"
#include "app/Samples/Light/SimpleLightMaterial.hpp"
#include "app/Samples/Light/SimpleLightMap.hpp"
#include "app/Samples/Light/LightSource/LightSourceDirection.hpp"
#include "app/Samples/Light/LightSource/LightSourcePoint.hpp"
#include "app/Samples/Light/LightSource/LightSourceSpot.hpp"
#include "app/Samples/Light/LightSource/LightSourceMult.hpp"
#include "app/Samples/Model/LoadModelApp.hpp"
#include "app/Samples/Advanced/DepthTestApp.hpp"
#include "app/Samples/Advanced/TemplateTestApp.hpp"
#include "app/Samples/Advanced/BlendApp.hpp"
#include "app/Samples/Advanced/CullFaceApp.hpp"
#include "app/Samples/Advanced/FrameBufferApp.hpp"
#include "app/Samples/Advanced/SkyboxApp.hpp"
#include "app/Samples/Advanced/AdvancedGLSLApp.hpp"
#include "app/Samples/Advanced/UniformBufferApp.hpp"
#include "app/Samples/Advanced/Geomtery/SimpleGemoteryApp.hpp"
#include "app/Samples/Advanced/Geomtery/ExplodeApp.hpp"
#include "app/Samples/Advanced/Geomtery/NormalLine.hpp"
#include "app/Samples/Advanced/Instance/MultieInstanceApp.hpp"
#include "app/Samples/Advanced/Instance/SaturnApp.hpp"
#include "app/Samples/Advanced/MsaaApp.hpp"
#include "app/Samples/Light/Advanced/BlinnPhongApp.hpp"
#include "app/Samples/Light/Advanced/GammaApp.hpp"
#include "app/Samples/Light/Advanced/ShadowMapApp.hpp"
#include "app/Samples/Light/Advanced/ShadowApp.hpp"
#include "app/Samples/Light/Advanced/PointLightShadowApp.hpp"
#include "app/Samples/Light/Advanced/NormalMapApp.hpp"
#include "app/Samples/Light/Advanced/ParallaxMapApp.hpp"
#include "app/Samples/Light/Advanced/HdrApp.hpp"
#include "app/Samples/Light/Advanced/BloomApp.hpp"
#include "app/Samples/Light/Advanced/DeferApp.hpp"
#include "app/Samples/Light/Advanced/SSAOApp.hpp"
#include "app/Samples/Advanced/PBR/PBRBaseApp.hpp"
#include "app/Samples/Advanced/PBR/PBRTextureApp.hpp"
#include "app/Samples/Advanced/PBR/IBLIrradianceConversionApp.hpp"
#include "app/Samples/Advanced/PBR/IBLIrradianceApp.hpp"
#include "app/Samples/Advanced/PBR/IBLSpecularApp.hpp"
#include <memory>

std::shared_ptr<Sample> SampleFactory::create(const AppType type) {
    switch (type) {
        case AppType::Base:              return std::make_shared<BaseSample>();
        case AppType::Triangle:          return std::make_shared<TriangleApp>();
        case AppType::Rect:              return std::make_shared<RectApp>();
        case AppType::SimpleTexture:     return std::make_shared<SimpleTextureApp>();
        case AppType::Cube:              return std::make_shared<CubeApp>();
        case AppType::Camera:            return std::make_shared<CameraApp>();
        case AppType::SimpleLight_Ambination:       return std::make_shared<SimpleLightAmbination>();
        case AppType::SimpleLight_Diffuse:          return std::make_shared<SimpleLightDiffuse>();
        case AppType::SimpleLight_Specular:         return std::make_shared<SimpleLightSpecular>();
        case AppType::SimpleLight_Material:         return std::make_shared<SimpleLightMaterial>();
        case AppType::SimpleLight_Map:              return std::make_shared<SimpleLightMap>();
        case AppType::SimpleLight_Source_Direction: return std::make_shared<LightSourceDirection>();
        case AppType::SimpleLight_Source_Point:     return std::make_shared<LightSourcePoint>();
        case AppType::SimpleLight_Source_Spot:      return std::make_shared<LightSourceSpot>();
        case AppType::SimpleLight_Source_Mult:      return std::make_shared<LightSourceMult>();
        case AppType::LoadModel:         return std::make_shared<LoadModelApp>();
        case AppType::DepthTest:         return std::make_shared<DepthTestApp>();
        case AppType::TemplateTest:      return std::make_shared<TemplateTestApp>();
        case AppType::Blend:             return std::make_shared<BlendApp>();
        case AppType::CullFace:          return std::make_shared<CullFaceApp>();
        case AppType::FrameBuffer:       return std::make_shared<FrameBufferApp>();
        case AppType::SkyBox:            return std::make_shared<SkyboxApp>();
        case AppType::AdvancedShader:    return std::make_shared<AdvancedGLSLApp>();
        case AppType::UniformBuffer:     return std::make_shared<UniformBufferApp>();
        case AppType::SimpleGeometry:    return std::make_shared<SimpleGemoteryApp>();
        case AppType::Explode:           return std::make_shared<ExplodeApp>();
        case AppType::NormalLine:        return std::make_shared<NormalLine>();
        case AppType::MultiInstance:     return std::make_shared<MultieInstanceApp>();
        case AppType::MultiInstance_Saturn: return std::make_shared<SaturnApp>();
        case AppType::Msaa:              return std::make_shared<MsaaApp>();
        case AppType::BlinnPhong:        return std::make_shared<BlinnPhongApp>();
        case AppType::Gamma:             return std::make_shared<GammaApp>();
        case AppType::Shadow_Map:        return std::make_shared<ShadowMapApp>();
        case AppType::Shadow:            return std::make_shared<ShadowApp>();
        case AppType::Shadow_PointLight: return std::make_shared<PointLightShadowApp>();
        case AppType::NormalMap:         return std::make_shared<NormalMapApp>();
        case AppType::ParallaxMap:       return std::make_shared<ParallaxMapApp>();
        case AppType::Hdr:               return std::make_shared<HdrApp>();
        case AppType::Bloom:             return std::make_shared<BloomApp>();
        case AppType::Defer:             return std::make_shared<DeferApp>();
        case AppType::SSAO:              return std::make_shared<SSAOApp>();
        case AppType::PBR_Base:          return std::make_shared<PBRBaseApp>();
        case AppType::PBR_Texture:       return std::make_shared<PBRTextureApp>();
        case AppType::PBR_IBL_Irradiance_Conversion: return std::make_shared<IBLIrradianceConversionApp>();
        case AppType::PBR_IBL_Irradiance:             return std::make_shared<IBLIrradianceApp>();
        case AppType::PBR_IBL_Specular:               return std::make_shared<IBLSpecularApp>();
        default: return nullptr;
    }
}
