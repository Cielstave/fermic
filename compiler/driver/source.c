#define _POSIX_C_SOURCE 200809L
#include "fermi/driver.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *fe_open_source(FeContext *context, const char *path) {
    int descriptor;
    do {
        descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        fe_error(context, (FeSpan){0, 1, 1}, "E_IO", "cannot open source: %s", strerror(errno));
        return NULL;
    }
    struct stat info;
    if (fstat(descriptor, &info)) {
        int saved = errno;
        close(descriptor);
        fe_error(context, (FeSpan){0, 1, 1}, "E_IO", "cannot inspect source: %s", strerror(saved));
        return NULL;
    }
    if (!S_ISREG(info.st_mode)) {
        close(descriptor);
        fe_error(context, (FeSpan){0, 1, 1}, "E_IO", "source must be a regular file");
        return NULL;
    }
    FILE *input = fdopen(descriptor, "rb");
    if (!input) {
        int saved = errno;
        close(descriptor);
        fe_error(context, (FeSpan){0, 1, 1}, "E_IO", "cannot read source: %s", strerror(saved));
    }
    return input;
}
