#pragma once

template<class Func>
class ScopeGuard{
public:
    ScopeGuard(Func &&func)
        : _func(func){
    }

    ~ScopeGuard(){
        execute();
    }

    void discard(){
        _discard = true;
    }

    ScopeGuard& update(Func &&func){
        _func = func;
        _discard = false;
        return *this;
    }
private:
    void execute(){
        _func();
    }
private:
    bool _discard{};
    Func _func{};
};