#include "AppFactory.hpp"
#include "app/AppHost.hpp"
#include "IApplication.hpp"
#include <memory>

std::shared_ptr<IApplication> AppFactory::create(const GraphicsType gtype, const AppType type) {
    auto host = std::make_shared<AppHost>();
    host->setBackend(gtype);
    host->setSample(type);
    return host;
}
