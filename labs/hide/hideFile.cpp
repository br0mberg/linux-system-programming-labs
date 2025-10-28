#include <iostream>
#include <string>
#include <sys/stat.h>   // stat, mkdir, chmod
#include <unistd.h>     // access
#include <cerrno>
#include <cstdio>       // std::rename
#include <cstring>      // strerror

static const char* DARK = ".dark";
static const mode_t DARK_MODE_FINAL  = 0300; // w+x для владельца (без r)
static const mode_t DARK_MODE_CREATE = 0700; // на время операций

// есть путь и это не каталог
static bool exists_not_dir(const std::string& p, struct stat& st) {
    if (lstat(p.c_str(), &st) == -1) return false;
    return !S_ISDIR(st.st_mode);
}

static bool create_dark_dir(const std::string& dark) {
    if (mkdir(dark.c_str(), DARK_MODE_CREATE) == -1 && errno != EEXIST) {
        std::cerr << "не смог создать " << dark << ": " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

static void set_dark_final_mode(const std::string& dark) {
    if (chmod(dark.c_str(), DARK_MODE_FINAL) == -1) {
        std::cerr << "не смог выставить права 0300 на " << dark
                  << ": " << std::strerror(errno) << "\n";
    }
}

static int do_hide(const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos) {
        std::cerr << "нужно имя файла из текущей папки (без '/').\n";
        return 2;
    }

    std::string src = "./" + name;
    struct stat st{};
    if (!exists_not_dir(src, st)) {
        std::cerr << "нет такого файла (или это каталог): " << name << "\n";
        return 1;
    }

    std::string dark = std::string("./") + DARK;
    if (!create_dark_dir(dark)) return 1;

    std::string dst = dark + "/" + name;
    if (access(dst.c_str(), F_OK) == 0) {
        std::cerr << "в " << DARK << " уже есть: " << name << "\n";
        return 1;
    }

    if (std::rename(src.c_str(), dst.c_str()) == -1) {
        std::cerr << "не смог спрятать (" << std::strerror(errno) << ")\n";
        return 1;
    }

    set_dark_final_mode(dark);
    std::cout << "спрятал: " << DARK << "/" << name << "\n";
    return 0;
}

static int do_unhide(const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos) {
        std::cerr << "нужно имя файла из текущей папки (без '/').\n";
        return 2;
    }

    std::string dark = std::string("./") + DARK;
    std::string src  = dark + "/" + name;

    struct stat st{};
    if (!exists_not_dir(src, st)) {
        std::cerr << "в " << DARK << " такого нет: " << name << "\n";
        return 1;
    }

    std::string dst = "./" + name;
    if (access(dst.c_str(), F_OK) == 0) {
        std::cerr << "в текущей папке уже есть: " << name << "\n";
        return 1;
    }

    if (std::rename(src.c_str(), dst.c_str()) == -1) {
        std::cerr << "не смог вернуть (" << std::strerror(errno) << ")\n";
        return 1;
    }

    set_dark_final_mode(dark);
    std::cout << "вернул: " << name << "\n";
    return 0;
}

#ifndef HIDEFILE_NO_MAIN
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr
            << "использование:\n"
            << "  " << argv[0] << " --hide   <имя>\n"
            << "  " << argv[0] << " --unhide <имя>\n";
        return 1;
    }

    std::string cmd  = argv[1];
    std::string name = argv[2];

    if (cmd == "--hide" || cmd == "-h")   return do_hide(name);
    if (cmd == "--unhide" || cmd == "-u") return do_unhide(name);

    std::cerr << "неизвестный флаг: " << cmd << "\n";
    return 1;
}
#endif
/*
потестил
echo "секрет" > note.md
ls
./hideFile --hide note.md
cat ./.dark/note.md
ls ./.dark
./hideFile --unhide note.md
ls

можно еще права посмотреть до/после
ls -ld .dark
ls -l .dark/note.md
*/