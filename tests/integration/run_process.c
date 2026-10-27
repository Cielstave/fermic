#include <signal.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "interrupt")) {
        raise(SIGINT);
    }
    if (argc == 2 && !strcmp(argv[1], "terminate")) {
        raise(SIGTERM);
    }
    return 127;
}
