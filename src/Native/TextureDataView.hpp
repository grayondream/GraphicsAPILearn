#pragma once
#include <Base/DataView.hpp>
#include "Math/Vector.hpp"
#include <cstdint>
#include "Geometry/Image.hpp"
#include <Geometry/Format.hpp>
#include <vector>

class Texture2DDataView{
public:
    Texture2DDataView(const DataView& data, const PixelFormat format, const ImageSize& size) {
        _size = size;
        _data = data;
        _format = format;
    }

    Texture2DDataView(uint8_t* const data, const int len, const PixelFormat format, const ImageSize& size) {
        _data = { data, len };
        _size = size;
        _format = format;
    }

    ImageSize size() const{
        return _size;
    }

    uint8_t* data() const {
        return _data.data();
    }

    std::size_t length() const {
        return _data.size();
    }

    PixelFormat format() const {
        return _format;
    }
private:
    DataView _data{};
    ImageSize _size{0, 0, 0};
    PixelFormat _format{};
};

using Texture3DDataView = std::vector<Texture2DDataView>;