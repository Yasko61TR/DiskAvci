#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "scanner.hpp"

namespace {

std::string format_bytes(std::uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
    return buffer;
}

void print_usage() {
    std::cerr << "Kullanım: diskavci scan <klasör>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "scan") {
        print_usage();
        return 1;
    }

    const std::filesystem::path root(argv[2]);
    const diskavci::ScanResult result = diskavci::scan(root);

    std::cout << "Taranan: " << root.string() << "\n";
    std::cout << "  Dosya: " << result.file_count << "\n";
    std::cout << "  Klasör: " << result.dir_count << "\n";
    std::cout << "  Toplam boyut: " << format_bytes(result.total_bytes) << "\n";
    std::cout << "  Takip edilmeyen link: " << result.symlink_count << "\n";

    if (!result.largest_files.empty()) {
        std::cout << "\nEn büyük dosyalar:\n";
        for (const auto& file : result.largest_files) {
            std::cout << "  " << format_bytes(file.size) << "\t" << file.path.string() << "\n";
        }
    }

    if (!result.errors.empty()) {
        std::cout << "\nAtlanan yol: " << result.errors.size() << "\n";
        for (const auto& error : result.errors) {
            std::cout << "  " << error.path.string() << ": " << error.message << "\n";
        }
        return 2;
    }
    return 0;
}
