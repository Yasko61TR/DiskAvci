#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "scanner.hpp"

namespace fs = std::filesystem;
using diskavci::ScanOptions;
using diskavci::scan;

// Her test için geçici bir klasör açar, bitince siler.
class ScannerTest : public ::testing::Test {
protected:
    fs::path root;

    void SetUp() override {
        std::random_device rd;
        root = fs::temp_directory_path() / ("diskavci_test_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(root);
    }

    void TearDown() override {
        std::error_code ec;
        fs::permissions(root / "kapali", fs::perms::owner_all, fs::perm_options::add, ec);
        fs::remove_all(root, ec);
    }

    // `rel` yoluna `bytes` baytlık dosya yazar, gerekirse klasörleri açar.
    void write_file(const fs::path& rel, std::size_t bytes) {
        const fs::path full = root / rel;
        fs::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out << std::string(bytes, 'x');
    }
};

TEST_F(ScannerTest, EmptyDirectory) {
    const auto result = scan(root);
    EXPECT_EQ(result.file_count, 0u);
    EXPECT_EQ(result.dir_count, 1u);
    EXPECT_EQ(result.total_bytes, 0u);
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(ScannerTest, CountsFilesDirectoriesAndBytes) {
    write_file("a.txt", 100);
    write_file("alt/b.txt", 200);
    write_file("alt/derin/c.txt", 300);

    const auto result = scan(root);
    EXPECT_EQ(result.file_count, 3u);
    EXPECT_EQ(result.dir_count, 3u);  // kök, alt, alt/derin
    EXPECT_EQ(result.total_bytes, 600u);
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(ScannerTest, LargestFilesAreSortedAndLimited) {
    write_file("kucuk.bin", 10);
    write_file("orta.bin", 200);
    write_file("buyuk.bin", 300);

    ScanOptions options;
    options.top_n = 2;
    const auto result = scan(root, options);

    ASSERT_EQ(result.largest_files.size(), 2u);
    EXPECT_EQ(result.largest_files[0].size, 300u);
    EXPECT_EQ(result.largest_files[1].size, 200u);
}

TEST_F(ScannerTest, TopNZeroListsNothing) {
    write_file("a.bin", 10);

    ScanOptions options;
    options.top_n = 0;
    const auto result = scan(root, options);

    EXPECT_TRUE(result.largest_files.empty());
    EXPECT_EQ(result.file_count, 1u);
}

TEST_F(ScannerTest, DoesNotFollowDirectorySymlink) {
    write_file("gercek/a.txt", 100);

    std::error_code ec;
    fs::create_directory_symlink(root / "gercek", root / "kisayol", ec);
    if (ec) GTEST_SKIP() << "Sembolik link oluşturulamadı: " << ec.message();

    const auto result = scan(root);
    EXPECT_EQ(result.symlink_count, 1u);
    EXPECT_EQ(result.file_count, 1u);   // link üzerinden ikinci kez sayılmaz
    EXPECT_EQ(result.total_bytes, 100u);
}

TEST_F(ScannerTest, SymlinkLoopTerminates) {
    write_file("a/dosya.txt", 50);

    std::error_code ec;
    fs::create_directory_symlink(root, root / "a" / "dongu", ec);  // a/dongu -> kök
    if (ec) GTEST_SKIP() << "Sembolik link oluşturulamadı: " << ec.message();

    const auto result = scan(root);
    EXPECT_EQ(result.symlink_count, 1u);
    EXPECT_EQ(result.file_count, 1u);
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(ScannerTest, HandlesTurkishCharactersInNames) {
    write_file(fs::u8path(u8"çalışma_ğüşiö") / fs::u8path(u8"İstanbul_ışık.txt"), 42);

    const auto result = scan(root);
    EXPECT_EQ(result.file_count, 1u);
    EXPECT_EQ(result.total_bytes, 42u);
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(ScannerTest, NonexistentRootIsReportedAsError) {
    const auto result = scan(root / "yok");
    EXPECT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.file_count, 0u);
}

TEST_F(ScannerTest, FileAsRootIsReportedAsError) {
    write_file("tek.txt", 10);

    const auto result = scan(root / "tek.txt");
    EXPECT_EQ(result.errors.size(), 1u);
}

#ifndef _WIN32
TEST_F(ScannerTest, UnreadableDirectoryIsSkippedNotFatal) {
    if (geteuid() == 0) GTEST_SKIP() << "root izinleri atlar, bu test anlamsız";

    write_file("acik/a.txt", 100);
    write_file("kapali/gizli.txt", 999);
    fs::permissions(root / "kapali", fs::perms::none);

    const auto result = scan(root);
    EXPECT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.file_count, 1u);   // erişilebilen dosya yine sayıldı
    EXPECT_EQ(result.total_bytes, 100u);
}
#endif
