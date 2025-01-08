#pragma once
#include <Native/TextureBase.hpp>
#include <Native/TextureDataView.hpp>

class GLTexture2D : public Texture2DBase {
public:
    using Texture2DType = unsigned int;
public:
	virtual ~GLTexture2D();

	virtual bool init(const Texture2DDataView &data) override;

	virtual void bind(const unsigned int unit) override;

	virtual void release() override;

	virtual void* handle() override;

	virtual bool valid() override;
private:
    Texture2DType _textureId{ 0 };
};