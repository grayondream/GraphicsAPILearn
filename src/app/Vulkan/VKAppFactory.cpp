#include "VKAppFactory.hpp"
#include "app/GL/GLAppFactory.hpp"

std::shared_ptr<IApplication> VKAppFactory::create(const AppType type) {
    return GLAppFactory::create(type);
}
