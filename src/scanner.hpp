#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace diskavci {

struct FileEntry {
    std::filesystem::path path;
    std::uintmax_t size = 0;
};

struct ScanError {
    std::filesystem::path path;
    std::string message;
};

struct ScanOptions {
    // En büyük kaç dosya listelensin.
    std::size_t top_n = 10;
};

struct ScanResult {
    std::uintmax_t total_bytes = 0;
    std::uint64_t file_count = 0;
    std::uint64_t dir_count = 0;      // taranan klasörler (kök dahil)
    std::uint64_t symlink_count = 0;  // takip edilmeyen sembolik linkler
    std::vector<FileEntry> largest_files;  // büyükten küçüğe sıralı
    std::vector<ScanError> errors;         // atlanan yollar ve nedenleri
};

// `root` altını tarar. Sembolik linkleri takip etmez; okunamayan yolları
// atlayıp `errors` listesine yazar, taramayı durdurmaz.
ScanResult scan(const std::filesystem::path& root, const ScanOptions& options = {});

}  // namespace diskavci
