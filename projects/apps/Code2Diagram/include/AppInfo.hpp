/**
 * @file AppInfo.hpp
 * @brief Name und Version der Anwendung — aus Solution.json (SSOT)
 *
 * Das Root-CMakeLists liest beide Werte aus Solution.json und reicht sie als
 * C2D_APP_NAME / C2D_APP_VERSION an den Kern weiter; nur AppInfo.cpp sieht sie.
 */
#pragma once

#include <string_view>

namespace c2d
{

[[nodiscard]] std::string_view appName() noexcept;
[[nodiscard]] std::string_view appVersion() noexcept;

}  // namespace c2d
