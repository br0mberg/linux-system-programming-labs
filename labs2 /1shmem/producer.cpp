#include "shared.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <semaphore.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: producer <input_dir>\n";
        return 1;
    }

    const char* input_dir = argv[1];

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedBuffer));

    auto* buffer = static_cast<SharedBuffer*>(
        mmap(nullptr, sizeof(SharedBuffer),
             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));

    buffer->write_idx = 0;
    buffer->read_idx = 0;

    sem_t* sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, MAX_FILES);
    sem_t* sem_full  = sem_open(SEM_FULL,  O_CREAT, 0666, 0);
    sem_t* sem_mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);

    DIR* dir = opendir(input_dir);
    dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_REG)
            continue;

        std::string path = std::string(input_dir) + "/" + entry->d_name;
        std::ifstream file(path, std::ios::binary);

        if (!file)
            continue;

        FileSlot slot{};
        slot.eof = false;
        slot.name_len = strlen(entry->d_name);
        strncpy(slot.filename, entry->d_name, MAX_FILENAME);

        file.read(slot.data, MAX_FILESIZE);
        slot.data_len = file.gcount();

        sem_wait(sem_empty);
        sem_wait(sem_mutex);

        buffer->slots[buffer->write_idx] = slot;
        buffer->write_idx = (buffer->write_idx + 1) % MAX_FILES;

        sem_post(sem_mutex);
        sem_post(sem_full);
    }

    // отправляем EOF
    FileSlot eof_slot{};
    eof_slot.eof = true;

    sem_wait(sem_empty);
    sem_wait(sem_mutex);

    buffer->slots[buffer->write_idx] = eof_slot;
    buffer->write_idx = (buffer->write_idx + 1) % MAX_FILES;

    sem_post(sem_mutex);
    sem_post(sem_full);

    closedir(dir);
    std::cout << "Producer finished\n";
}
