#include <iostream>
#include <string>
#include <sys/types.h> // POSIX-типвы
#include <sys/stat.h> // lstat + макросы
#include <unistd.h> // базовые POSIX-вызовы
#include <dirent.h> // работа с директориями
#include <cerrno> // коды ошибок
#include <cstring>
#include <cstdio> // perror на errno

int main() {
    const char* dirpath = ".";
    DIR* dir = opendir(dirpath);
    if (!dir) {
        std::perror("opendir");
        return 1;
    }

    // счётчики
    unsigned long long n_regular = 0;
    unsigned long long n_directory = 0;
    unsigned long long n_link = 0;
    unsigned long long n_char = 0;
    unsigned long long n_block = 0;
    unsigned long long n_fifo = 0;
    unsigned long long n_sock = 0;
    unsigned long long n_unknown = 0; 

    while (true) {
        errno = 0;                 // чтобы отличить EOF от ошибки
        dirent* de = readdir(dir);
        if (!de) {
            if (errno) {
                std::perror("readdir");
                closedir(dir);
                return 1;
            }
            break; // конец каталога
        }

        const char* name = de->d_name;
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
            continue; // пропустить спец-записи
        }

        std::string path = std::string("./") + name;

        struct stat st {};
        if (lstat(path.c_str(), &st) == -1) {
            // возможны битые ссылки/гонки; не падаем, держимся и учитываем как unknown
            std::cerr << "lstat failed for " << path << ": "
                      << std::strerror(errno) << '\n';
            ++n_unknown;
            continue;
        }

        mode_t m = st.st_mode;
        if      (S_ISREG(m))  ++n_regular;
        else if (S_ISDIR(m))  ++n_directory;
        else if (S_ISLNK(m))  ++n_link;
        else if (S_ISCHR(m))  ++n_char;
        else if (S_ISBLK(m))  ++n_block;
        else if (S_ISFIFO(m)) ++n_fifo;
        else if (S_ISSOCK(m)) ++n_sock;
        else                  ++n_unknown;
    }

    if (closedir(dir) == -1) {
        std::perror("closedir");
        return 1;
    }

    // Стабильный формат вывода — все категории, даже если нули.
    std::cout
        << "regular:      " << n_regular     << '\n'
        << "directory:    " << n_directory     << '\n'
        << "symlink:      " << n_link     << '\n'
        << "char_device:  " << n_char     << '\n'
        << "block_device: " << n_block     << '\n'
        << "fifo:         " << n_fifo    << '\n'
        << "socket:       " << n_sock    << '\n'
        << "unknown:      " << n_unknown << '\n';

    return 0;
}