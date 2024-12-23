#pragma once
#include <string>
#include <memory>
#include "Application.hpp"

std::shared_ptr<Application> GetApp(const std::string &name);

void RegisterAllApp();