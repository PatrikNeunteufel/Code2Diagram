/**
 * @file AppInfo.cpp
 * @brief Name und Version der Anwendung
 */
#include "pch.h"
#include "AppInfo.hpp"

#if !defined(C2D_APP_NAME) || !defined(C2D_APP_VERSION)
#  error "C2D_APP_NAME / C2D_APP_VERSION fehlen - sie kommen aus dem Root-CMakeLists (Solution.json)."
#endif

namespace c2d
{

std::string_view appName() noexcept
{
    return C2D_APP_NAME;
}

std::string_view appVersion() noexcept
{
    return C2D_APP_VERSION;
}

}  // namespace c2d
