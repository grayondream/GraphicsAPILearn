#pragma once
#include <unordered_map>
#include <memory>
#include <string>

class IApplication;
class AppRegister{
private:
    AppRegister();
public:
    static AppRegister* instance();
    void run();

    std::shared_ptr<IApplication> get(const std::string &name);
    void push(const std::string &name, const std::shared_ptr<IApplication> &app);

public:
    std::unordered_map<std::string, std::shared_ptr<IApplication>> _apps{};
};