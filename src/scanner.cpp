#include "scanner.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace diskavci {
namespace {

// `top` büyükten küçüğe sıralı tutulur ve en fazla `n` eleman içerir.
void keep_largest(std::vector<FileEntry>& top, FileEntry entry, std::size_t n) {
    if (n == 0) return;
    auto bigger_first = [](const FileEntry& a, const FileEntry& b) { return a.size > b.size; };
    auto pos = std::upper_bound(top.begin(), top.end(), entry, bigger_first);
    if (top.size() < n || pos != top.end()) {
        top.insert(pos, std::move(entry));
        if (top.size() > n) top.pop_back();
    }
}

void add_error(ScanResult& result, const fs::path& path, const std::error_code& ec) {
    result.errors.push_back({path, ec.message()});
}

}  // namespace

ScanResult scan(const fs::path& root, const ScanOptions& options) {
    ScanResult result;
    std::error_code ec;

    const auto root_status = fs::symlink_status(root, ec);
    if (ec) {
        add_error(result, root, ec);
        return result;
    }
    if (!fs::is_directory(root_status)) {
        result.errors.push_back({root, "bir klasör değil"});
        return result;
    }

    // Özyineleme yerine elle yönetilen yığın: bir klasörde hata olursa
    // sadece o klasörü atlayıp devam etmek kolay olur.
    std::vector<fs::path> pending{root};
    while (!pending.empty()) {
        const fs::path dir = std::move(pending.back());
        pending.pop_back();
        ++result.dir_count;

        ec.clear();
        fs::directory_iterator it(dir, ec);
        if (ec) {
            add_error(result, dir, ec);
            continue;
        }

        const fs::directory_iterator end;
        while (it != end) {
            const fs::directory_entry& entry = *it;
            std::error_code entry_ec;
            const auto status = entry.symlink_status(entry_ec);

            if (entry_ec) {
                add_error(result, entry.path(), entry_ec);
            } else if (fs::is_symlink(status)) {
                ++result.symlink_count;  // takip etmiyoruz (döngü riski)
            } else if (fs::is_directory(status)) {
                pending.push_back(entry.path());
            } else if (fs::is_regular_file(status)) {
                const auto size = entry.file_size(entry_ec);
                if (entry_ec) {
                    add_error(result, entry.path(), entry_ec);
                } else {
                    ++result.file_count;
                    result.total_bytes += size;
                    keep_largest(result.largest_files, {entry.path(), size}, options.top_n);
                }
            }

            ec.clear();
            it.increment(ec);
            if (ec) {
                add_error(result, dir, ec);
                break;
            }
        }
    }

    return result;
}

}  // namespace diskavci
