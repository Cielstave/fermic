#define _POSIX_C_SOURCE 200809L
#include "fermi/file.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void test_real_file(const char *path) {
    FermiFile file = {0}, moved = {0};
    size_t length = strlen(path);
    assert(fermi_file_open_read(&file, (const unsigned char *)path, length) == FERMI_FILE_OK);
    assert(file.open && file.descriptor >= 0);
    assert(fcntl(file.descriptor, F_GETFD) & FD_CLOEXEC);
    assert(fermi_file_open_read(&file, (const unsigned char *)path, length) ==
           FERMI_FILE_INVALID_STATE);
    assert(fermi_file_move(&file, &file) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_move(&moved, &file) == FERMI_FILE_OK);
    assert(!file.open && file.descriptor == 0);
    errno = EDOM;
    fermi_file_drop(&file);
    assert(errno == EDOM);
    unsigned char output[8192];
    memset(output, 0xa5, sizeof(output));
    size_t count = 99;
    assert(fermi_file_read(&moved, NULL, 0, &count) == FERMI_FILE_OK && count == 0);
    assert(fermi_file_read(&moved, output, sizeof(output), &count) == FERMI_FILE_OK);
    assert(count == 4096);
    for (size_t index = 0; index < count; ++index) {
        assert(output[index] == (unsigned char)index);
    }
    assert(output[count] == 0xa5);
    assert(fermi_file_read(&moved, output, sizeof(output), &count) == FERMI_FILE_OK && count == 17);
    for (size_t index = 0; index < count; ++index) {
        assert(output[index] == (unsigned char)index);
    }
    assert(fermi_file_read(&moved, output, sizeof(output), &count) == FERMI_FILE_OK && count == 0);
    int descriptor = moved.descriptor;
    assert(fermi_file_close(&moved) == FERMI_FILE_OK);
    assert(!moved.open && fcntl(descriptor, F_GETFD) == -1 && errno == EBADF);
    assert(fermi_file_close(&moved) == FERMI_FILE_INVALID_STATE);
    count = 99;
    assert(fermi_file_read(&moved, output, 1, &count) == FERMI_FILE_INVALID_STATE && count == 99);
    errno = EDOM;
    fermi_file_drop(&moved);
    assert(errno == EDOM);
}

static void test_rejections(const char *directory) {
    FermiFile file = {0};
    const unsigned char nul[] = {'a', 0, 'b'};
    assert(fermi_file_open_read(&file, NULL, 0) == FERMI_FILE_INVALID_PATH);
    assert(fermi_file_open_read(&file, NULL, 1) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_open_read(NULL, nul, 1) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_open_read(&file, nul, sizeof(nul)) == FERMI_FILE_INVALID_PATH);
    assert(fermi_file_open_read(&file, nul, 4097) == FERMI_FILE_PATH_TOO_LONG);
    assert(fermi_file_open_read(&file, (const unsigned char *)directory, strlen(directory)) ==
           FERMI_FILE_NOT_REGULAR);
    assert(!file.open && file.descriptor == 0);
    size_t count = 77;
    assert(fermi_file_read(NULL, NULL, 0, &count) == FERMI_FILE_INVALID_ARGUMENT && count == 77);
    assert(fermi_file_close(NULL) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_move(NULL, &file) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_move(&file, NULL) == FERMI_FILE_INVALID_ARGUMENT);
}

static void test_output(const char *directory) {
    mode_t previous_mask = umask(0077);
    char path[256], link[256];
    snprintf(path, sizeof(path), "%s/output", directory);
    snprintf(link, sizeof(link), "%s/link", directory);
    FermiFile file = {0};
    assert(fermi_file_create_new(&file, (const unsigned char *)path, strlen(path)) ==
           FERMI_FILE_OK);
    assert(fcntl(file.descriptor, F_GETFD) & FD_CLOEXEC);
    struct stat info;
    assert(fstat(file.descriptor, &info) == 0 && (info.st_mode & 0777) == 0600);
    unsigned char input[8192];
    memset(input, 0x65, sizeof(input));
    size_t count = 99;
    assert(fermi_file_write(&file, input, sizeof(input), &count) == FERMI_FILE_OK && count == 4096);
    assert(fermi_file_sync(&file) == FERMI_FILE_OK);
    assert(fermi_file_close(&file) == FERMI_FILE_OK);
    assert(fermi_file_create_new(&file, (const unsigned char *)path, strlen(path)) ==
           FERMI_FILE_OPEN_FAILED);
    assert(!file.open);
    assert(symlink("output", link) == 0);
    assert(fermi_file_create_new(&file, (const unsigned char *)link, strlen(link)) ==
           FERMI_FILE_OPEN_FAILED);
    assert(!file.open && stat(path, &info) == 0 && info.st_size == 4096);
    assert(unlink(link) == 0);
    assert(symlink("absent-target", link) == 0);
    assert(fermi_file_create_new(&file, (const unsigned char *)link, strlen(link)) ==
           FERMI_FILE_OPEN_FAILED);
    assert(fermi_file_open_read(&file, (const unsigned char *)path, strlen(path)) == FERMI_FILE_OK);
    count = 99;
    assert(fermi_file_write(&file, input, 1, &count) == FERMI_FILE_WRITE_FAILED && count == 99);
    unsigned char output[4096];
    assert(fermi_file_read(&file, output, sizeof(output), &count) == FERMI_FILE_OK &&
           count == 4096);
    assert(memcmp(input, output, count) == 0);
    fermi_file_drop(&file);
    assert(unlink(link) == 0 && unlink(path) == 0);
    umask(previous_mask);
}

int main(void) {
    char directory[] = "build/tests/file-XXXXXX";
    assert(mkdtemp(directory));
    char path[256];
    assert(snprintf(path, sizeof(path), "%s/data", directory) > 0);
    int writer = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    assert(writer >= 0);
    unsigned char bytes[4113];
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        bytes[index] = (unsigned char)index;
    }
    assert(write(writer, bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));
    assert(close(writer) == 0);
    test_real_file(path);
    test_rejections(directory);
    test_output(directory);
    char fifo[256];
    snprintf(fifo, sizeof(fifo), "%s/fifo", directory);
    assert(mkfifo(fifo, 0600) == 0);
    FermiFile file = {0};
    assert(fermi_file_open_read(&file, (const unsigned char *)fifo, strlen(fifo)) ==
           FERMI_FILE_NOT_REGULAR);
    assert(!file.open);
    assert(unlink(fifo) == 0);
    assert(unlink(path) == 0);
    assert(fermi_file_open_read(&file, (const unsigned char *)path, strlen(path)) ==
           FERMI_FILE_OPEN_FAILED);
    assert(!file.open);
    assert(rmdir(directory) == 0);
    puts("PASS: regular-file lifecycle, bounded reads, move, close-on-exec and FIFO rejection");
    return 0;
}
