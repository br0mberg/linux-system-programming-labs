// Сервис коллажей:
//  - GET /make
//  1. параллельно скачивает котиков GET /cat
//  2. оставляет 12 уникальных (по хэшу контента)
//  3. кладёт их в tar
//  4.. отправляет tar POST /cat

#include "libs/httplib.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

// хэш FNV-1a 64-bit 
static uint64_t fnv1a64(const unsigned char* data, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)data[i];
        h *= 1099511628211ull;
    }
    return h;
}

// tar
#pragma pack(push, 1)
struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};
#pragma pack(pop)

static void tar_write_octal(char* dst, size_t len, uint64_t val) {
    std::snprintf(dst, len, "%0*lo", (int)len - 1, (unsigned long)val);
}

static uint32_t tar_checksum(const TarHeader& h) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&h);
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(TarHeader); i++) sum += p[i];
    return sum;
}

static void tar_append_file(std::string& out, const std::string& filename, const std::string& data) {
    TarHeader h{};
    std::memset(&h, 0, sizeof(h));

    std::snprintf(h.name, sizeof(h.name), "%s", filename.c_str());
    std::snprintf(h.mode, sizeof(h.mode), "%07o", 0644);
    std::snprintf(h.uid, sizeof(h.uid), "%07o", 0);
    std::snprintf(h.gid, sizeof(h.gid), "%07o", 0);
    tar_write_octal(h.size, sizeof(h.size), (uint64_t)data.size());
    tar_write_octal(h.mtime, sizeof(h.mtime), 0);

    std::memset(h.chksum, ' ', sizeof(h.chksum));
    h.typeflag = '0';
    std::snprintf(h.magic, sizeof(h.magic), "ustar");
    std::snprintf(h.version, sizeof(h.version), "00");

    uint32_t sum = tar_checksum(h);
    std::snprintf(h.chksum, sizeof(h.chksum), "%06o", sum);
    h.chksum[6] = '\0';
    h.chksum[7] = ' ';

    out.append(reinterpret_cast<const char*>(&h), sizeof(h));
    out.append(data);

    size_t pad = (512 - (data.size() % 512)) % 512;
    out.append(pad, '\0');
}

static void tar_finish(std::string& tar) {
    tar.append(512, '\0');
    tar.append(512, '\0');
}

// HTTP
static std::string http_get_cat(const std::string& host, int port, int timeout_sec) {
    httplib::Client cli(host, port);
    cli.set_connection_timeout(timeout_sec, 0);
    cli.set_read_timeout(timeout_sec, 0);

    auto res = cli.Get("/cat");
    if (!res || res->status != 200) return {};
    return res->body;
}

static bool http_post_tar(const std::string& host, int port, const std::string& tar, int timeout_sec) {
    httplib::Client cli(host, port);
    cli.set_connection_timeout(timeout_sec, 0);
    cli.set_read_timeout(timeout_sec, 0);

    auto res = cli.Post("/cat", tar, "application/x-tar");
    return (res && res->status >= 200 && res->status < 300);
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: kotichi_simple <listen_host> <listen_port> <backend_host> <backend_port>\n";
        std::cerr << "Example: kotichi_simple 0.0.0.0 9090 127.0.0.1 8080\n";
        return 2;
    }

    const std::string listen_host = argv[1];
    const int listen_port = std::stoi(argv[2]);
    const std::string backend_host = argv[3];
    const int backend_port = std::stoi(argv[4]);

    const int need_unique = 12;
    const int parallelism = 12;   // параллельно дергаем /cat
    const int timeout_sec = 30;   // учитываем задержки 1..15 сек
    const int max_rounds = 12;    // чтобы не зависать на дублях

    httplib::Server srv;

    srv.Get("/make", [&](const httplib::Request&, httplib::Response& res) {
        std::unordered_set<uint64_t> seen;
        seen.reserve(128);

        std::vector<std::string> cats;
        cats.reserve(need_unique);

        for (int round = 0; round < max_rounds && (int)cats.size() < need_unique; round++) {
            std::vector<std::future<std::string>> futs;
            futs.reserve(parallelism);

            for (int i = 0; i < parallelism; i++) {
                futs.emplace_back(std::async(std::launch::async, [&] {
                    return http_get_cat(backend_host, backend_port, timeout_sec);
                }));
            }

            for (auto& f : futs) {
                std::string bytes = f.get();
                if (bytes.empty()) continue;

                uint64_t h = fnv1a64(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
                if (seen.insert(h).second) {
                    cats.push_back(std::move(bytes));
                    if ((int)cats.size() >= need_unique) break;
                }
            }
        }

        if ((int)cats.size() < need_unique) {
            res.status = 503;
            res.set_content("Not enough unique cats\n", "text/plain");
            return;
        }

        std::string tar;
        tar.reserve((size_t)need_unique * 200 * 1024);

        for (int i = 0; i < need_unique; i++) {
            char name[64];
            std::snprintf(name, sizeof(name), "cat_%02d.jpg", i + 1);
            tar_append_file(tar, name, cats[i]);
        }
        tar_finish(tar);

        if (!http_post_tar(backend_host, backend_port, tar, timeout_sec)) {
            res.status = 502;
            res.set_content("Failed to POST tar to backend\n", "text/plain");
            return;
        }

        res.set_content("OK\n", "text/plain");
    });

    std::cout << "kotichi_simple listening on " << listen_host << ":" << listen_port << "\n";
    std::cout << "backend = " << backend_host << ":" << backend_port << "\n";
    std::cout << "GET /make -> 12 unique cats -> tar -> POST /cat\n";
    srv.listen(listen_host.c_str(), listen_port);
    return 0;
}

// g++ -std=c++17 cat_backend.cpp -o cat_backend -pthread
// g++ -std=c++17 kotichi_collage.cpp -o kotichi_collage -pthread
// ./cat_backend 0.0.0.0 8080 ./cats ./uploads
// ./kotichi_collage 0.0.0.0 9090 127.0.0.1 8080
// curl -v "http://127.0.0.1:9090/make"
