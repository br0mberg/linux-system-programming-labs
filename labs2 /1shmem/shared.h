#pragma once

#include <cstddef>

constexpr const char* SHM_NAME = "/pc_shm";
constexpr const char* SEM_EMPTY = "/pc_sem_empty";
constexpr const char* SEM_FULL  = "/pc_sem_full";
constexpr const char* SEM_MUTEX = "/pc_sem_mutex";

constexpr size_t MAX_FILES = 8;
constexpr size_t MAX_FILENAME = 256;
constexpr size_t MAX_FILESIZE = 64 * 1024;

struct FileSlot {
    bool eof;
    size_t name_len;
    size_t data_len;
    char filename[MAX_FILENAME];
    char data[MAX_FILESIZE];
};

struct SharedBuffer {
    size_t write_idx;
    size_t read_idx;
    FileSlot slots[MAX_FILES];
};
