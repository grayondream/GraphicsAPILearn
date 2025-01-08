#pragma once
#include <cstdint>
#include <stddef.h>

class DataView {
public:
	DataView() = default;
	DataView(uint8_t* const data, const int size) {
		_size = size;
		_data = data;
	}

	uint8_t* data() const {
		return _data;
	}

	int size() const {
		return _size;
	}
private:
	uint8_t* _data{};
	int _size{};
};