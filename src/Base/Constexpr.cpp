#include "Constexpr.hpp"
#include "ConstexprValue.hpp"

namespace Constexpr{
    int GetShadowMapWidth() {
        return kShadowMapWidth;
    }

    int GetShadowMapHeight() {
        return kShadowMapHeight;
    }

    int GetWindowWidth() {
        return kWindowWidth;
    }

    int GetWindowHeight() {
        return kWindowHeight;
    }

    bool GetEnableMsaa() {
        return kEnableMsaa;
    }
}