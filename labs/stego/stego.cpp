#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cerrno>
#include <cstdint>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const char   MAGIC[]    = "STEGJPG1";   // сигнатура флаг
static const size_t MAGIC_LEN  = sizeof(MAGIC) - 1;


static off_t filesize(int fd) {
    struct stat st{};
    if (fstat(fd, &st) == -1) return -1;
    return st.st_size;
}

static bool has_trailer(int fd, uint32_t &msg_len, off_t &payload_start) {
    off_t sz = filesize(fd);
    if (sz < (off_t)(MAGIC_LEN + 4)) return false;

    char tail_magic[MAGIC_LEN];
    if (pread(fd, tail_magic, MAGIC_LEN, sz - MAGIC_LEN) != (ssize_t)MAGIC_LEN) return false;
    if (memcmp(tail_magic, MAGIC, MAGIC_LEN) != 0) return false;

    unsigned char lenbuf[4];
    if (pread(fd, lenbuf, 4, sz - MAGIC_LEN - 4) != 4) return false;
    msg_len = (uint32_t)lenbuf[0]
            | ((uint32_t)lenbuf[1] << 8)
            | ((uint32_t)lenbuf[2] << 16)
            | ((uint32_t)lenbuf[3] << 24);

    if ((off_t)msg_len > sz) return false;
    payload_start = sz - MAGIC_LEN - 4 - (off_t)msg_len;
    if (payload_start < 2) return false;
    return true;
}

static bool likely_jpeg(int fd) {
    unsigned char soi[2] = {0,0};
    if (pread(fd, soi, 2, 0) != 2) return false;
    return soi[0] == 0xFF && soi[1] == 0xD8;
}

// добавить скрытое сообщение
static int add_message(const std::string& path, const std::string& msg) {
    int fd = open(path.c_str(), O_RDWR);
    if (fd == -1) {
        std::cerr << "не открыл " << path << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    uint32_t old_len = 0; off_t start = 0;
    if (has_trailer(fd, old_len, start)) {
        std::cerr << "в файле уже есть скрытое послание (" << old_len << " байт)\n";
        close(fd);
        return 1;
    }

    if (!likely_jpeg(fd)) {
        std::cerr << "предупр: не похоже на JPEG (SOI != FFD8), но всё равно пишу хвост\n";
    }

    off_t end = lseek(fd, 0, SEEK_END);
    if (end == (off_t)-1) {
        std::cerr << "lseek не удался: " << std::strerror(errno) << "\n";
        close(fd);
        return 1;
    }

    const std::string& payload = msg;
    if (!payload.empty()) {
        ssize_t w = write(fd, payload.data(), payload.size());
        if (w != (ssize_t)payload.size()) {
            std::cerr << "не записал payload: " << std::strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }

    uint32_t L = (uint32_t)payload.size();
    unsigned char lenbuf[4] = {
        (unsigned char)( L        & 0xFF),
        (unsigned char)((L >> 8 ) & 0xFF),
        (unsigned char)((L >> 16) & 0xFF),
        (unsigned char)((L >> 24) & 0xFF)
    };
    if (write(fd, lenbuf, 4) != 4) {
        std::cerr << "не записал длину: " << std::strerror(errno) << "\n";
        close(fd);
        return 1;
    }
    if (write(fd, MAGIC, MAGIC_LEN) != (ssize_t)MAGIC_LEN) {
        std::cerr << "не записал MAGIC: " << std::strerror(errno) << "\n";
        close(fd);
        return 1;
    }

    close(fd);
    std::cout << "добавил " << L << " байт скрытого текста в " << path << "\n";
    return 0;
}

// прочитать скрытое сообщение
static int read_message(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "не открыл " << path << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    uint32_t L = 0; off_t start = 0;
    if (!has_trailer(fd, L, start)) {
        std::cerr << "скрытого послания не найдено\n";
        close(fd);
        return 1;
    }

    std::vector<char> buf(L);
    if (L > 0) {
        ssize_t r = pread(fd, buf.data(), L, start);
        if (r != (ssize_t)L) {
            std::cerr << "не прочитал payload: " << std::strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }
    close(fd);

    std::cout.write(buf.data(), buf.size());
    std::cout.flush();
    return 0;
}

// удалить скрытое сообщение (восстановить оригинальный JPEG)
static int clear_message(const std::string& path) {
    int fd = open(path.c_str(), O_RDWR);
    if (fd == -1) {
        std::cerr << "не открыл " << path << ": " << std::strerror(errno) << "\n";
        return 1;
    }

    uint32_t L = 0; off_t start = 0;
    if (!has_trailer(fd, L, start)) {
        std::cerr << "ничего удалять: флаг не найден\n";
        close(fd);
        return 1;
    }

    off_t sz = filesize(fd);
    off_t cut = sz - (off_t)MAGIC_LEN - 4 - (off_t)L;
    if (ftruncate(fd, cut) == -1) {
        std::cerr << "не обрезал файл: " << std::strerror(errno) << "\n";
        close(fd);
        return 1;
    }

    close(fd);
    std::cout << "скрытое послание удалено, размер укорочен до " << cut << " байт\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr
            << "возможности запуска:\n"
            << "  "<< argv[0] << " --add \"сообщение\" <file.jpg>\n"
            << "  " << argv[0] << " --read <file.jpg>\n"
            << "  "<< argv[0] << " --clear <file.jpg>\n";
        return 2;
    }

    std::string cmd = argv[1];
    if (cmd == "--add") {
        if (argc != 4) {
            std::cerr << "нужно: --add \"сообщение\" <file.jpg>\n";
            return 2;
        }
        std::string msg  = argv[2];
        std::string path = argv[3];
        return add_message(path, msg);
    } else if (cmd == "--read") {
        if (argc != 3) {
            std::cerr << "нужно: --read <file.jpg>\n";
            return 2;
        }
        return read_message(argv[2]);
    } else if (cmd == "--clear") {
        if (argc != 3) {
            std::cerr << "нужно: --clear <file.jpg>\n";
            return 2;
        }
        return clear_message(argv[2]);
    } else {
        std::cerr << "неизвестная команда: " << cmd << "\n";
        return 2;
    }
}

/*
проверил
cp /usr/share/help/C/shotwell/figures/crop_thirds.jpg ./test.jpg
MSG="сообщение"
printf %s "$MSG" | wc -c
cp test.jpg test.orig.jpg

./stego --add "$MSG" test.jpg
S0=$(stat -c%s test.orig.jpg)
S1=$(stat -c%s test.jpg)
N=$(printf %s "$MSG" | wc -c)
echo "$((S1-S0)) должно быть=$((N+12))"
Δ=57 ожидаемо=57
./stego --read test.jpg; echo
./stego --clear test.jpg
sha256sum test.orig.jpg test.jpg
*/