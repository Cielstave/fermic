#define _XOPEN_SOURCE 700
#include "fermi/driver.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int run_error(FeContext *context, const char *operation, int error) {
    fe_error(context, (FeSpan){0, 1, 1}, "E_RUN", "%s: %s", operation, strerror(error));
    return 1;
}

static int execute(FeContext *context, const char *path, const FeOptions *options) {
    char **arguments = calloc((size_t)options->argument_count + 2, sizeof(*arguments));
    if (!arguments) {
        return run_error(context, "cannot allocate program arguments", errno);
    }
    arguments[0] = (char *)path;
    for (int i = 0; i < options->argument_count; ++i) {
        arguments[i + 1] = options->arguments[i];
    }
    int channel[2];
    if (pipe(channel)) {
        int error = errno;
        free(arguments);
        return run_error(context, "cannot open execution status pipe", error);
    }
    if (fcntl(channel[0], F_SETFD, FD_CLOEXEC) < 0 || fcntl(channel[1], F_SETFD, FD_CLOEXEC) < 0) {
        int error = errno;
        close(channel[0]);
        close(channel[1]);
        free(arguments);
        return run_error(context, "cannot configure execution status pipe", error);
    }
    /* The foreground program receives terminal interrupts. Keep the driver alive
     * to collect its status and remove temporary output, as POSIX system() does. */
    struct sigaction ignored = {0}, previous_int, previous_quit;
    ignored.sa_handler = SIG_IGN;
    sigemptyset(&ignored.sa_mask);
    if (sigaction(SIGINT, &ignored, &previous_int) < 0) {
        int error = errno;
        close(channel[0]);
        close(channel[1]);
        free(arguments);
        return run_error(context, "cannot configure interrupt handling", error);
    }
    if (sigaction(SIGQUIT, &ignored, &previous_quit) < 0) {
        int error = errno;
        sigaction(SIGINT, &previous_int, NULL);
        close(channel[0]);
        close(channel[1]);
        free(arguments);
        return run_error(context, "cannot configure quit handling", error);
    }
    pid_t child = fork();
    int fork_error = errno;
    if (!child) {
        close(channel[0]);
        int error = 0;
        if (sigaction(SIGINT, &previous_int, NULL) < 0 ||
            sigaction(SIGQUIT, &previous_quit, NULL) < 0) {
            error = errno;
        } else {
            execv(path, arguments);
            error = errno;
        }
        const char *bytes = (const char *)&error;
        size_t sent = 0;
        while (sent < sizeof(error)) {
            ssize_t count = write(channel[1], bytes + sent, sizeof(error) - sent);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                break;
            }
            sent += (size_t)count;
        }
        _exit(127);
    }
    close(channel[1]);
    int exec_error = 0, read_error = 0, wait_error = 0, status = 0;
    size_t received = 0;
    if (child > 0) {
        while (received < sizeof(exec_error)) {
            ssize_t count =
                read(channel[0], (char *)&exec_error + received, sizeof(exec_error) - received);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0) {
                read_error = errno;
                break;
            }
            if (!count) {
                break;
            }
            received += (size_t)count;
        }
        while (waitpid(child, &status, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            wait_error = errno;
            break;
        }
    }
    close(channel[0]);
    sigaction(SIGQUIT, &previous_quit, NULL);
    sigaction(SIGINT, &previous_int, NULL);
    free(arguments);
    if (child < 0) {
        return run_error(context, "cannot start program", fork_error);
    }
    if (read_error) {
        return run_error(context, "cannot read execution status", read_error);
    }
    if (wait_error) {
        return run_error(context, "cannot collect program status", wait_error);
    }
    if (received) {
        return run_error(context, "cannot execute program",
                         received == sizeof(exec_error) && exec_error ? exec_error : EIO);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return run_error(context, "invalid program termination status", EIO);
}

int fe_run_executable(FeProgram *program, const FeOptions *options) {
    if (options->command != FE_COMMAND_TEST && !options->output && !options->no_cache) {
        char cached[4096];
        int prepared = fe_prepare_run_cache(program, options, cached);
        if (prepared >= 0) {
            return prepared ? execute(program->ctx, cached, options) : 1;
        }
    }
    char directory[4096] = {0}, temporary[4128];
    const char *output = options->output;
    if (!output) {
        const char *root = getenv("TMPDIR");
        if (!root || !*root) {
            root = P_tmpdir;
        }
        if (snprintf(directory, sizeof(directory), "%s/fermi-run-XXXXXX", root) >=
            (int)sizeof(directory)) {
            return run_error(program->ctx, "temporary output path is too long", ENAMETOOLONG);
        }
        if (!mkdtemp(directory)) {
            return run_error(program->ctx, "cannot create temporary output directory", errno);
        }
        snprintf(temporary, sizeof(temporary), "%s/program", directory);
        output = temporary;
    }
    int result =
        fe_build_native(program, output, options->optimization,
                        options->command == FE_COMMAND_TEST ? fe_emit_tests : fe_emit_executable);
    if (!result) {
        /* Resolve relative output names explicitly; execution never searches PATH. */
        char *path = realpath(output, NULL);
        if (!path) {
            result = run_error(program->ctx, "cannot resolve executable path", errno);
        } else {
            result = execute(program->ctx, path, options);
            free(path);
        }
    }
    if (*directory) {
        int error = 0;
        if (unlink(output) && errno != ENOENT) {
            error = errno;
        }
        if (rmdir(directory) && errno != ENOENT && !error) {
            error = errno;
        }
        if (error && !program->ctx->diagnostic.code) {
            run_error(program->ctx, "cannot remove temporary output", error);
            if (!result) {
                result = 1;
            }
        }
    }
    return result;
}
