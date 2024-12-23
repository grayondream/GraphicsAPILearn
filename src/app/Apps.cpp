#include "Apps.hpp"
#include "Application.hpp"
#include "BoxApplication.hpp"
#include "TriangleApplication.hpp"
#include <unordered_map>
#include <memory>

std::unordered_map<std::string, std::shared_ptr<Application>> gApps{};

std::shared_ptr<Application> GetApp(const std::string &name){
    const auto it = gApps.find(name);
    if(it != gApps.end()){
        return it->second;
    }

    return {};
}

void RegisterApp(const std::string &name, const std::shared_ptr<Application> app){
    gApps[name] = app;
}

void RegisterAllApp(){
    RegisterApp("BaseApp", std::make_shared<Application>());
    RegisterApp("TriangleApp", std::make_shared<TriangleApplication>());
    //RegisterApp("BoxApp", std::make_shared<BoxApplication>());
}