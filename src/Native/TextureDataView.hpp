#pragma once
#include <Base/DataView.hpp>
#include "Math/Vector.hpp"
#include <cstdint>
#include "Geometry/Image.hpp"

class Texture2DDataView{
public:
    Texture2DDataView(const DataView& data, const ImageSize& size) {
        _size = size;
        _data = data;
    }

    Texture2DDataView(uint8_t* const data, const int len, const ImageSize& size) {
        _data = { data, len };
        _size = size;
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

private:
    DataView _data{};
    ImageSize _size{0, 0, 0};
};