#pragma once
#include <native/TextureBase.hpp>
#include <native/TextureDataView.hpp>

class GLTexture3D : public Texture3DBase {
public:
    using Texture3DType = unsigned int;
public:
	virtual ~GLTexture3D();

	virtual bool init(const Texture3DDataView &data) override;

	virtual void bind(const unsigned int unit) override;

	virtual void release() override;

	virtual void* handle() override;

	virtual bool valid() override;
	
private:
	Texture3DType _textureId{ 0 };
};