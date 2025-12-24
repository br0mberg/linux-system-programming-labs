// cat_backend.cpp
// Кото-сервис:
//  - GET  /cat -> случайный JPEG из каталога с задержкой 1..15 сек
//  - POST /cat -> сохраняет присланный бинарник (tar/jpg) с задержкой 1..15 сек

#include "libs/httplib.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static void sleep_1_15s(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(1, 2);
    std::this_thread::sleep_for(std::chrono::seconds(d(rng)));
}

static std::vector<unsigned char> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    std::streamsize n = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<unsigned char> buf((size_t)n);
    in.read(reinterpret_cast<char*>(buf.data()), n);
    return buf;
}

static void write_file(const fs::path& p, const std::string& bytes) {
    std::ofstream out(p, std::ios::binary);
    out.write(bytes.data(), (std::streamsize)bytes.size());
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: cat_backend <host> <port> <cats_dir> [uploads_dir]\n";
        std::cerr << "Example: cat_backend 0.0.0.0 8080 ./cats ./uploads\n";
        return 2;
    }

    const std::string host = argv[1];
    const int port = std::stoi(argv[2]);
    const fs::path cats_dir = argv[3];
    const fs::path uploads_dir = (argc >= 5) ? fs::path(argv[4]) : fs::path("./uploads");

    fs::create_directories(uploads_dir);

    std::vector<fs::path> cats;
    for (auto& e : fs::directory_iterator(cats_dir)) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        for (auto& ch : ext) ch = (char)tolower(ch);
        if (ext == ".jpg" || ext == ".jpeg") cats.push_back(e.path());
    }
    if (cats.empty()) {
        std::cerr << "No .jpg/.jpeg files in " << cats_dir << "\n";
        return 1;
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> pick(0, cats.size() - 1);

    httplib::Server srv;

    srv.Get("/cat", [&](const httplib::Request&, httplib::Response& res) {
        sleep_1_15s(rng);

        auto p = cats[pick(rng)];
        auto bytes = read_file(p);
        if (bytes.empty()) {
            res.status = 500;
            res.set_content("Failed to read cat\n", "text/plain");
            return;
        }
        res.set_content(reinterpret_cast<const char*>(bytes.data()), bytes.size(), "image/jpeg");
    });

    srv.Post("/cat", [&](const httplib::Request& req, httplib::Response& res) {
        sleep_1_15s(rng);

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

        std::string ext = ".bin";
        auto ct = req.get_header_value("Content-Type");
        if (ct.find("application/x-tar") != std::string::npos) ext = ".tar";
        else if (ct.find("image/jpeg") != std::string::npos) ext = ".jpg";

        fs::path out = uploads_dir / ("upload_" + std::to_string(ms) + ext);
        write_file(out, req.body);

        res.set_content("OK\n", "text/plain");
    });

    std::cout << "cat_backend listening on " << host << ":" << port << "\n";
    std::cout << "cats_dir=" << cats_dir << " uploads_dir=" << uploads_dir << "\n";
    srv.listen(host.c_str(), port);
    return 0;
}
