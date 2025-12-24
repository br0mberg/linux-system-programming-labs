#define HIDEFILE_NO_MAIN
#include "../hide/hideFile.cpp"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cerrno>
#include <cstdint>

#include <sys/stat.h>   // lstat, fstat
#include <fcntl.h>      // open
#include <unistd.h>     // pread, pwrite, read, write, close, access, unlink

static const size_t STASH_BYTES = 4096;

static std::string meta_path_for(const std::string& name) {
    return std::string("./") + DARK + "/.stash_" + name; // ./.dark/.stash_<имя>
}

static int distort(const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos) {
        std::cerr << "нужно имя файла из текущей папки (без '/')\n";
        return 2;
    }

    struct stat st{};
    if (!exists_not_dir(name, st)) {
        std::cerr << "нет такого файла или это каталог: " << name << "\n";
        return 1;
    }

    if (!create_dark_dir(std::string("./") + DARK)) return 1;
    set_dark_final_mode(std::string("./") + DARK);

    std::string meta = meta_path_for(name);
    if (access(meta.c_str(), F_OK) == 0) {
        std::cerr << "похоже, уже искажали - есть " << meta << "\n";
        return 1;
    }

    int fd = open(name.c_str(), O_RDWR);
    if (fd == -1) {
        std::cerr << "не открыл " << name << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    if (fstat(fd, &st) == -1) {
        std::cerr << "stat не вышел: " << std::strerror(errno) << "\n";
        close(fd);
        return 1;
    }

    size_t n = 0;
    if (st.st_size > 0) n = (size_t)std::min<off_t>((off_t)STASH_BYTES, st.st_size);

    // читаем первые n байт
    std::vector<unsigned char> head(n);
    if (n > 0) {
        ssize_t r = pread(fd, head.data(), n, 0);
        if (r != (ssize_t)n) {
            std::cerr << "не прочитал шапку: " << std::strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }

    // пишем head как есть в .dark/.stash_<имя>
    int mfd = open(meta.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (mfd == -1) {
        std::cerr << "не создал " << meta << ": " << std::strerror(errno) << "\n";
        close(fd);
        return 1;
    }
    ssize_t w = (head.empty() ? 0 : write(mfd, head.data(), head.size()));
    int e = errno;
    if (close(mfd) == -1) {}
    if (!head.empty() && w != (ssize_t)head.size()) {
        errno = (w < 0) ? e : EIO;
        std::cerr << "не записал метаданные: " << std::strerror(errno) << "\n";
        unlink(meta.c_str());
        close(fd);
        return 1;
    }

    // затираем начало файла нулями
    if (n > 0) {
        std::vector<unsigned char> zeros(n, 0);
        ssize_t wz = pwrite(fd, zeros.data(), n, 0);
        if (wz != (ssize_t)n) {
            std::cerr << "не затёр шапку: " << std::strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }

    close(fd);
    std::cout << "исказил: " << name << " (убил " << n << " байт)\n";
    return 0;
}

static int restore(const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos) {
        std::cerr << "нужно имя файла из текущей папки (без '/')\n";
        return 2;
    }

    std::string meta = meta_path_for(name);
    int mfd = open(meta.c_str(), O_RDONLY);
    if (mfd == -1) {
        std::cerr << "нет метаданных: " << meta << "\n";
        return 1;
    }

    std::vector<unsigned char> head;
    unsigned char buf[4096];
    for (;;) {
        ssize_t r = read(mfd, buf, sizeof(buf));
        if (r == 0) break;
        if (r < 0) {
            std::cerr << "не прочитал метаданные: " << std::strerror(errno) << "\n";
            close(mfd);
            return 1;
        }
        head.insert(head.end(), buf, buf + r);
    }
    close(mfd);

    int fd = open(name.c_str(), O_RDWR);
    if (fd == -1) {
        std::cerr << "не открыл " << name << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    if (!head.empty()) {
        ssize_t w = pwrite(fd, head.data(), head.size(), 0);
        if (w != (ssize_t)head.size()) {
            std::cerr << "не записал шапку: " << std::strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }
    close(fd);

    if (unlink(meta.c_str()) == -1) {
        std::cerr << "предупр: не удалил " << meta << ": " << std::strerror(errno) << "\n";
    }

    std::cout << "вернул: " << name << " (" << head.size() << " байт)\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr
            << "как запускать:\n"
            << "  "<< argv[0] << " --distort <имя>\n"
            << "  "<< argv[0] <<" --restore <имя>\n";
        return 2;
    }
    std::string cmd  = argv[1];
    std::string name = argv[2];

    if (cmd == "--distort" || cmd == "-d") return distort(name);
    if (cmd == "--restore" || cmd == "-r") return restore(name);

    std::cerr << "неизвестная команда: " << cmd << "\n";
    return 2;
}

/*
потестил
g++ -std=c++17 -Wall -Wextra -pedantic -O2 stash.cpp -o stash

cp /bin/ls ./ls.bin
file ls.bin 

./hideStash --distort ls.bin
file ls.bin

head -c 32 ls.bin | hexdump -C     # посмотреть шапку
ls -l .dark/.stash_ls.bin        # метаданные

./hideStash --restore ls.bin
file ls.bin                        # снова ELF

sha256sum /bin/ls ls.bin 

и вот так:
printf 'hi' > small.txt
./hideStash --distort small.txt
./hideStash --restore small.txt
*/