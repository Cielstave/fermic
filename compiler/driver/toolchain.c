#define _POSIX_C_SOURCE 200809L
#include "fermi/driver.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

bool fe_run_tool(FeContext *context, char *const argv[]) {
    int pipefd[2];
    if (pipe(pipefd)) {
        return fe_error(context, (FeSpan){0, 1, 1}, "E_TOOLCHAIN",
                        "cannot open backend diagnostic pipe");
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return fe_error(context, (FeSpan){0, 1, 1}, "E_TOOLCHAIN", "cannot start native backend");
    }
    if (!pid) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDERR_FILENO) < 0 || dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[1]);
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    close(pipefd[1]);
    char details[320] = {0}, chunk[512];
    size_t used = 0;
    ssize_t count;
    bool readable = true;
    while ((count = read(pipefd[0], chunk, sizeof(chunk))) != 0) {
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            readable = false;
            break;
        }
        for (ssize_t i = 0; i < count && used + 1 < sizeof(details); ++i) {
            details[used++] = chunk[i] ? chunk[i] : '?';
        }
    }
    close(pipefd[0]);
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return fe_error(context, (FeSpan){0, 1, 1}, "E_TOOLCHAIN",
                            "cannot collect backend status");
        }
    }
    if (readable && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }
    fe_error(context, (FeSpan){0, 1, 1}, "E_TOOLCHAIN", "native backend failed: %.320s", details);
    snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
             "FERMI_CC must name a compatible C compiler executable; backend output is limited to "
             "319 bytes");
    return false;
}
bool fe_backend_target(const char *cc, char text[128]) {
    int pipefd[2];
    if (pipe(pipefd)) {
        return false;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }
    if (!pid) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[1]);
        int sink = open("/dev/null", O_WRONLY);
        if (sink < 0 || dup2(sink, STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(sink);
        execlp(cc, cc, "-dumpmachine", (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    size_t used = 0;
    bool valid = true;
    char buf[128];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) != 0) {
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            valid = false;
            break;
        }
        for (ssize_t i = 0; i < n; ++i) {
            char ch = buf[i];
            if (ch == '\n' || ch == '\r') {
                continue;
            }
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.') ||
                used >= 127) {
                valid = false;
            } else {
                text[used++] = ch;
            }
        }
    }
    close(pipefd[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            valid = false;
            break;
        }
    }
    text[used] = 0;
    return valid && used && WIFEXITED(status) && !WEXITSTATUS(status);
}
