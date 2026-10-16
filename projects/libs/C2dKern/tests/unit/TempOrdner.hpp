/**
 * @file TempOrdner.hpp
 * @brief Ein Ordner unter dem Temp-Ordner fuer Tests mit Projekten, der mit dem Test wieder verschwindet
 */
#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

class TempOrdner
{
public:
    TempOrdner()
    {
        static std::atomic<int> zaehler{0};
        const auto jetzt = std::chrono::steady_clock::now().time_since_epoch().count();
        m_weg = std::filesystem::temp_directory_path() /
                ("c2d_test_" + std::to_string(jetzt) + "_" + std::to_string(++zaehler));
        std::filesystem::create_directories(m_weg);
    }
    ~TempOrdner()
    {
        std::error_code egal;
        std::filesystem::remove_all(m_weg, egal);
    }
    TempOrdner(const TempOrdner&) = delete;
    TempOrdner& operator=(const TempOrdner&) = delete;

    void lege(const std::string& relativ, std::string_view inhalt) const
    {
        const std::filesystem::path weg = m_weg / relativ;
        std::filesystem::create_directories(weg.parent_path());
        std::ofstream(weg, std::ios::binary) << inhalt;
    }
    [[nodiscard]] const std::filesystem::path& weg() const { return m_weg; }

private:
    std::filesystem::path m_weg;
};
