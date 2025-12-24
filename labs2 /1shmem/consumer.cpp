#include "shared.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: consumer <output_dir>\n";
        return 1;
    }

    const char* output_dir = argv[1];

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);

    auto* buffer = static_cast<SharedBuffer*>(
        mmap(nullptr, sizeof(SharedBuffer),
             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));

    sem_t* sem_empty = sem_open(SEM_EMPTY, 0);
    sem_t* sem_full  = sem_open(SEM_FULL, 0);
    sem_t* sem_mutex = sem_open(SEM_MUTEX, 0);

    while (true) {
        sem_wait(sem_full);
        sem_wait(sem_mutex);

        FileSlot slot = buffer->slots[buffer->read_idx];
        buffer->read_idx = (buffer->read_idx + 1) % MAX_FILES;

        sem_post(sem_mutex);
        sem_post(sem_empty);

        if (slot.eof)
            break;

        std::string path = std::string(output_dir) + "/" + slot.filename;
        std::ofstream file(path, std::ios::binary);
        file.write(slot.data, slot.data_len);

        std::cout << "Written: " << slot.filename << "\n";
    }

    // очистка
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    sem_unlink(SEM_MUTEX);

    std::cout << "Consumer finished\n";
}
