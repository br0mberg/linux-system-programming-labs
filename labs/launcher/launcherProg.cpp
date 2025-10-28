#include <iostream>
#include <cstring>
#include <cerrno>
#include <cstdio>   // perror чтоб не ругался
#include <unistd.h>   // fork, execvp, dup2, _exit
#include <sys/wait.h>   // waitpid
#include <fcntl.h>    // open

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "как запускать: " << argv[0] << " <программа> [аргументы...]\n";
        return 2;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork не взлетел: " << std::strerror(errno) << "\n";
        return 1;
    }

    if (pid == 0) {
        // в потомке открываем файлы и направляем потоки
        int out_fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            std::cerr << "не открыл out.txt: " << std::strerror(errno) << "\n";
            _exit(127);
        }
        int err_fd = open("err.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (err_fd < 0) {
            std::cerr << "не открыл err.txt: " << std::strerror(errno) << "\n";
            _exit(127);
        }

        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            std::cerr << "не смог подменить stdout: " << std::strerror(errno) << "\n";
            _exit(127);
        }
        if (dup2(err_fd, STDERR_FILENO) == -1) {
            std::cerr << "не смог подменить stderr: " << std::strerror(errno) << "\n";
            _exit(127);
        }
        close(out_fd);
        close(err_fd);

        // запускаем то, что нам передали (argv[1] и дальше — аргументы)
        execvp(argv[1], &argv[1]);

        // если execvp не стартанул
        std::cerr << "запуск накрылся (" << argv[1] << "): " << std::strerror(errno) << "\n";
        _exit(127);
    }

    // родитель ждёт и отдаёт тот же код возврата
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        std::cerr << "ошибка ожидания waitpid дочернего процесса: " << std::strerror(errno) << "\n";
        return 1;
    }
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

/*
протестил
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -g launcherProg.cpp -o launcher
./launcher /bin/echo hello world
rm -f out.txt err.txt
*/