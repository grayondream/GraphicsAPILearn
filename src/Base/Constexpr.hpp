#pragma once

namespace Constexpr{
    //这些接口本身不是特别合适，但是为了方便修改，避免修改后大量文件需要重新编译而浪费时间
    int GetShadowMapWidth();

    int GetShadowMapHeight();

    int GetWindowWidth();

    int GetWindowHeight();

    bool GetEnableMsaa();
};
