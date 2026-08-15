#pragma once
#include "app/AppType.hpp"
#include <memory>
class Sample;
class SampleFactory {
public:
    static std::shared_ptr<Sample> create(const AppType type);
};
