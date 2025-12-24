#include <iostream>
#include <cstring>
#include <cerrno>

#include <unistd.h>  // fork, setsid, dup2, execvp, _exit
#include <fcntl.h>    // open
#include <signal.h>  // signal, SIGHUP
#include <sys/types.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "как запускать: " << argv[0] << " <программа> [аргументы...]\n";
        return 2;
    }
    std::cerr << "parent pid=" << getpid() << "\n";
    // создаём процесс-потомка
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "ошибка fork(): " << std::strerror(errno) << "\n";
        return 1;
    }

    if (pid > 0) {
        // родитель нам больше не нужен
        std::cout << "запустил, pid=" << pid << "\n";
        return 0;
    }

    /*тут мы в дочернем процессе*/
    // новый сессионный лидер, больше нет управляющего терминала
    if (setsid() == -1) { 
        // если не смогли
        std::cerr << "setsid не удалось: " << std::strerror(errno) << "\n";
        _exit(127);
    }

    // прочитал, что добавляют ещё fork() чтобы снова не получить TTY
    pid = fork();
    if (pid < 0) {
        _exit(127);
    }
    if (pid > 0) {
        _exit(0);             // ребёнок уходит и остаётся внук
    }

    // игнорируем SIGHUP
    signal(SIGHUP, SIG_IGN);  

    // открываем /dev/null один раз и направляем в него все стандартные потоки
    int nullfd = open("/dev/null", O_RDWR);
    if (nullfd == -1) {
        std::cerr << "не открыл /dev/null: " << std::strerror(errno) << "\n";
        _exit(127);
    }

    // stdin в /dev/null
    if (dup2(nullfd, STDIN_FILENO) == -1) {
        std::cerr << "dup2(stdin) не вышел: " << std::strerror(errno) << "\n";
        _exit(127);
    }
    // stdout в /dev/null
    if (dup2(nullfd, STDOUT_FILENO) == -1) {
        std::cerr << "dup2(stdout) не вышел: " << std::strerror(errno) << "\n";
        _exit(127);
    }
    // stderr в /dev/null
    if (dup2(nullfd, STDERR_FILENO) == -1) {
        _exit(127);
    }
    if (nullfd > STDERR_FILENO) close(nullfd); // лишний дескриптор можно закрыть

    // запускаем целевую программу
    execvp(argv[1], &argv[1]);

    _exit(127);
}

/*
протестил
g++ -std=c++17 -Wall -Wextra -pedantic -O2 nohup2.cpp -o nohup2
./nohup2 /bin/sh -c 'echo OUT; echo ERR 1>&2; sleep 1; echo DONE'
*/