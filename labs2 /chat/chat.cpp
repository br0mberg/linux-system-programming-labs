#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include <atomic>
#include <cerrno> 
#include <csignal>
#include <cstring> 
#include <iostream>
#include <string>
#include <thread>

// Флаг работы приложения: true — работаем, false — завершаемся
static std::atomic<bool> running{true};


// Дескриптор сокета, чтобы разбудить recvfrom() при Ctrl+C
static std::atomic<int> g_sock{-1};

// Обработчик Ctrl C: ставим флаг завершения и "будим" блокирующий recvfrom()
static void on_sigint(int) {
    running = false;
    int s = g_sock.load();
    if (s != -1) {
        shutdown(s, SHUT_RDWR); // разбудит recvfrom в другом потоке
    }
}

// Утилита для аварийного завершения с печатью errno
static void die(const std::string& msg) {
    std::cerr << "ERROR: " << msg
              << " (errno=" << errno << " " << std::strerror(errno) << ")\n";
    std::exit(1);
}

int main(int argc, char* argv[]) {
    // Аргументы: порт обязателен, никнейм опционален
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: chat <port> [nickname]\n";
        return 2;
    }

    // Парсим порт из argv[1]
    int port = 0;
    try {
        port = std::stoi(argv[1]);
    } catch (...) {
        std::cerr << "Неверный порт\n";
        return 2;
    }
    if (port <= 0 || port > 65535) {
        std::cerr << "Порт должен быть в диапазоне 1..65535\n";
        return 2;
    }

    // Никнейм по умолчанию
    std::string nick = (argc == 3) ? argv[2] : "anon";

    // Ставим обработчик Ctrl+C через sigaction без SA_RESTART,
    // чтобы блокирующие вызовы не перезапускались бесконечно.
    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // важно: НЕ SA_RESTART
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        die("sigaction");
    }

    // Создаём UDP сокет (AF_INET — IPv4, SOCK_DGRAM — UDP)
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) die("socket");
    g_sock = sock;

    // Разрешаем отправку широковещательных дейтаграмм (broadcast)
    int yes = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) == -1) {
        die("setsockopt SO_BROADCAST");
    }

    // Разрешаем переиспользование адреса/порта
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        die("setsockopt SO_REUSEADDR");
    }

    // Привязываем сокет к порту на всех интерфейсах, чтобы принимать входящие сообщения
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(static_cast<uint16_t>(port));
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == -1) {
        die("bind");
    }

    // Адрес назначения для отправки: широковещательный 255.255.255.255:<port>
    sockaddr_in bc_addr{};
    bc_addr.sin_family = AF_INET;
    bc_addr.sin_port = htons(static_cast<uint16_t>(port));
    bc_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // Поток-приёмник: слушает UDP и печатает входящие сообщения
    std::thread receiver([&] {
        char buf[2048];

        while (running) {
            sockaddr_in src{};
            socklen_t slen = sizeof(src);

            ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                 reinterpret_cast<sockaddr*>(&src), &slen);
            if (n < 0) {
                if (!running) break;
                if (errno == EINTR) continue; // прервано сигналом
                continue;
            }
            if (n == 0) {
                if (!running) break;
                continue;
            }

            buf[n] = '\0';

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
            int src_port = ntohs(src.sin_port);

            std::cout << "\n[" << ip << ":" << src_port << "] " << buf << "\n> " << std::flush;
        }
    });

    std::cout << "UDP broadcast chat на порту " << port << ", nick=" << nick << "\n";
    std::cout << "Введите сообщение и нажмите Enter. Ctrl+C для выхода.\n> " << std::flush;

    // Цикл отправки: читаем строки из stdin и шлём их broadcast'ом
    std::string line;
    while (running && std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> " << std::flush;
            continue;
        }

        std::string msg = nick + ": " + line;

        if (sendto(sock, msg.c_str(), msg.size(), 0,
                   reinterpret_cast<sockaddr*>(&bc_addr), sizeof(bc_addr)) == -1) {
            std::cerr << "sendto не удалось: " << std::strerror(errno) << "\n";
        }

        std::cout << "> " << std::flush;
    }

    // Завершение: останавливаем поток приёма и закрываем сокет
    running = false;
    shutdown(sock, SHUT_RDWR);
    close(sock);

    if (receiver.joinable()) receiver.join();

    std::cout << "\nПокеда.\n";
    return 0;
}

// g++ -std=c++17 chat.cpp -o chat -pthread
// ./chat 7777 andrey
// в другом терминател ./chat 7777 leonid_agutin
