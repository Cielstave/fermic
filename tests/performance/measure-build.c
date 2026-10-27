#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) {
        perror("clock_gettime");
        return -1;
    }
    return (double)now.tv_sec * 1e3 + (double)now.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fputs("usage: measure-build LABEL OBJECT COMMAND [ARGUMENTS...]\n", stderr);
        return 2;
    }
    double start = now_ms();
    if (start < 0) {
        return 1;
    }
    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (!child) {
        if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
            _exit(126);
        }
        execvp(argv[3], argv + 3);
        perror("execvp");
        _exit(127);
    }
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status)) {
        fputs("measured build failed\n", stderr);
        return 1;
    }
    double end = now_ms();
    struct stat object;
    if (end < 0) {
        return 1;
    }
    if (stat(argv[2], &object)) {
        perror("cannot inspect measured object");
        return 1;
    }
    printf("%s,%.3f,%lld\n", argv[1], end - start, (long long)object.st_size);
    return ferror(stdout) != 0;
}
