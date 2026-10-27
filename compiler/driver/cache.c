#define _XOPEN_SOURCE 700
#include "fermi/driver.h"
#include "fermi/hash.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(__APPLE__)
#define fe_mtime st_mtimespec
#define fe_ctime st_ctimespec
#else
#define fe_mtime st_mtim
#define fe_ctime st_ctim
#endif

/* Cache only the default native toolchain. Wrappers and environment-controlled
 * include/library search paths can have dependencies this driver cannot infer. */
static bool cacheable_environment(void) {
    static const char *const variables[] = {"CPATH",
                                            "C_INCLUDE_PATH",
                                            "CPLUS_INCLUDE_PATH",
                                            "OBJC_INCLUDE_PATH",
                                            "LIBRARY_PATH",
                                            "SDKROOT",
                                            "COMPILER_PATH",
                                            "GCC_EXEC_PREFIX",
                                            "CCC_OVERRIDE_OPTIONS",
                                            "CLANG_CONFIG_FILE_SYSTEM_DIR",
                                            "CLANG_CONFIG_FILE_USER_DIR",
                                            "SOURCE_DATE_EPOCH"};
    const char *cc = getenv("FERMI_CC");
    if (cc && *cc) {
        return false;
    }
    for (size_t i = 0; i < sizeof(variables) / sizeof(variables[0]); ++i) {
        if (getenv(variables[i])) {
            return false;
        }
    }
    return true;
}

static bool directory_owned(const char *path, mode_t forbidden) {
    struct stat info;
    return !lstat(path, &info) && S_ISDIR(info.st_mode) && info.st_uid == getuid() &&
           !(info.st_mode & forbidden);
}

static bool cache_root(char path[4096]) {
    if ((mkdir("build", 0755) && errno != EEXIST) || !directory_owned("build", 022)) {
        return false;
    }
    if ((mkdir("build/.fermi-run", 0700) && errno != EEXIST) ||
        !directory_owned("build/.fermi-run", 077)) {
        return false;
    }
    char *resolved = realpath("build/.fermi-run", NULL);
    if (!resolved) {
        return false;
    }
    bool ok = strlen(resolved) + 160 < 4096;
    if (ok) {
        strcpy(path, resolved);
    }
    free(resolved);
    return ok;
}

static bool compiler_path(char path[4096], struct stat *info) {
    const char *search = getenv("PATH");
    if (!search) {
        return false;
    }
    do {
        const char *end = strchr(search, ':');
        size_t length = end ? (size_t)(end - search) : strlen(search);
        char candidate[4096];
        if (length < sizeof(candidate) - 8) {
            snprintf(candidate, sizeof(candidate), "%.*s%sclang", (int)length,
                     length ? search : ".", length ? "/" : "./");
            /* An empty PATH component denotes the current directory. */
            if (!length) {
                strcpy(candidate, "./clang");
            }
            if (!access(candidate, X_OK)) {
                char *resolved = realpath(candidate, NULL);
                bool ok = resolved && strlen(resolved) < 4096;
                if (ok) {
                    strcpy(path, resolved);
                }
                free(resolved);
                if (!ok || stat(path, info) || !S_ISREG(info->st_mode)) {
                    return false;
                }
                FILE *file = fopen(path, "rb");
                unsigned char magic[4];
                ok = file && fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
                     !memcmp(magic, "\177ELF", sizeof(magic));
                if (file && fclose(file)) {
                    ok = false;
                }
                return ok;
            }
        }
        if (!end) {
            break;
        }
        search = end + 1;
    } while (true);
    return false;
}

/* Termux uses loader variables for its normal toolchain. Include both their
 * values and the identity of named files/directories rather than disabling reuse. */
static bool loader_identity(FILE *out, const char *name) {
    const char *value = getenv(name);
    fprintf(out, "%s:%zu:%s\n", name, value ? strlen(value) : 0, value ? value : "");
    if (!value) {
        return !ferror(out);
    }
    char *copy = strdup(value);
    if (!copy) {
        return false;
    }
    char *state = NULL;
    const char *separators = !strcmp(name, "LD_PRELOAD") ? ": " : ":";
    for (char *item = strtok_r(copy, separators, &state); item;
         item = strtok_r(NULL, separators, &state)) {
        struct stat info;
        if (!stat(item, &info)) {
            fprintf(out, "%ju:%ju:%ju:%jd:%ld:%jd:%ld\n", (uintmax_t)info.st_dev,
                    (uintmax_t)info.st_ino, (uintmax_t)info.st_size, (intmax_t)info.fe_mtime.tv_sec,
                    info.fe_mtime.tv_nsec, (intmax_t)info.fe_ctime.tv_sec, info.fe_ctime.tv_nsec);
        } else {
            fputs("absent\n", out);
        }
    }
    free(copy);
    return !ferror(out);
}

static bool cache_key(FILE *code, const FeOptions *options, char key[65]) {
    char compiler[4096], source[65];
    struct stat info;
    struct utsname host;
    if (!compiler_path(compiler, &info) || uname(&host) || fseek(code, 0, SEEK_SET) ||
        !fe_sha256_stream(code, source)) {
        return false;
    }
    FILE *identity = tmpfile();
    if (!identity) {
        return false;
    }
    fprintf(identity, "fermi-run-cache-1\n%s\n%s\n%s\n%zu:%s\n", source, options->optimization,
            host.machine, strlen(compiler), compiler);
    fprintf(identity, "%ju:%ju:%ju:%jd:%ld:%jd:%ld\n", (uintmax_t)info.st_dev,
            (uintmax_t)info.st_ino, (uintmax_t)info.st_size, (intmax_t)info.fe_mtime.tv_sec,
            info.fe_mtime.tv_nsec, (intmax_t)info.fe_ctime.tv_sec, info.fe_ctime.tv_nsec);
    for (unsigned i = 0; fe_executable_flags[i]; ++i) {
        fprintf(identity, "%s\n", fe_executable_flags[i]);
    }
    const char *path = getenv("PATH");
    fprintf(identity, "%zu:%s", strlen(path), path);
    bool ok = loader_identity(identity, "LD_LIBRARY_PATH") &&
              loader_identity(identity, "LD_PRELOAD") && !ferror(identity) &&
              !fseek(identity, 0, SEEK_SET) && fe_sha256_stream(identity, key);
    if (fclose(identity)) {
        ok = false;
    }
    return ok;
}

static bool regular_owned(const char *path, bool executable) {
    struct stat info;
    return !lstat(path, &info) && S_ISREG(info.st_mode) && info.st_uid == getuid() &&
           !(info.st_mode & 022) && (!executable || (info.st_mode & S_IXUSR));
}

static bool valid_entry(const char *entry, char output[4096]) {
    if (!directory_owned(entry, 077)) {
        return false;
    }
    char manifest[4128], actual[65], expected[66];
    snprintf(manifest, sizeof(manifest), "%s/sha256", entry);
    if (snprintf(output, 4096, "%s/program", entry) >= 4096 || !regular_owned(output, true) ||
        !regular_owned(manifest, false)) {
        return false;
    }
    FILE *file = fopen(manifest, "r");
    if (!file) {
        return false;
    }
    bool ok = fgets(expected, sizeof(expected), file) && strlen(expected) == 65 &&
              expected[64] == '\n' && fgetc(file) == EOF && !ferror(file);
    if (fclose(file)) {
        ok = false;
    }
    if (!ok || !fe_sha256_file(output, actual)) {
        return false;
    }
    expected[64] = 0;
    return !strcmp(actual, expected);
}

/* Remove only our named artifacts. Unknown files prevent directory removal. */
static bool remove_entry(const char *entry) {
    if (!directory_owned(entry, 077)) {
        return false;
    }
    char path[4128];
    snprintf(path, sizeof(path), "%s/program", entry);
    unlink(path);
    snprintf(path, sizeof(path), "%s/sha256", entry);
    unlink(path);
    return !rmdir(entry);
}

int fe_prepare_run_cache(FeProgram *program, const FeOptions *options, char output[4096]) {
    char root[4096], key[65], entry[4096], stage[4096];
    if (!cacheable_environment() || !cache_root(root)) {
        return -1;
    }
    FILE *code = tmpfile();
    if (!code) {
        return -1;
    }
    if (!fe_emit_executable(program, code)) {
        fclose(code);
        return 0;
    }
    if (!cache_key(code, options, key)) {
        fclose(code);
        return -1;
    }
    snprintf(entry, sizeof(entry), "%s/%s", root, key);
    if (valid_entry(entry, output)) {
        fclose(code);
        return 1;
    }
    snprintf(stage, sizeof(stage), "%s/.stage-XXXXXX", root);
    if (!mkdtemp(stage)) {
        fclose(code);
        return -1;
    }
    char source[4128], binary[4128], manifest[4128];
    snprintf(source, sizeof(source), "%s/main.c", stage);
    snprintf(binary, sizeof(binary), "%s/program", stage);
    snprintf(manifest, sizeof(manifest), "%s/sha256", stage);
    int result = -1;
    FILE *file = fopen(source, "wb");
    bool ok = file && !fseek(code, 0, SEEK_SET);
    char bytes[8192];
    size_t count;
    while (ok && (count = fread(bytes, 1, sizeof(bytes), code))) {
        ok = fwrite(bytes, 1, count, file) == count;
    }
    ok = ok && !ferror(code);
    if (file && fclose(file)) {
        ok = false;
    }
    fclose(code);
    if (!ok) {
        goto cleanup;
    }
    if (!fe_compile_executable(program, source, binary, options->optimization)) {
        result = 0;
        goto cleanup;
    }
    char digest[65];
    if (chmod(binary, 0700) || !fe_sha256_file(binary, digest)) {
        goto cleanup;
    }
    file = fopen(manifest, "w");
    ok = file && fprintf(file, "%s\n", digest) == 65;
    if (file && fclose(file)) {
        ok = false;
    }
    if (!ok || unlink(source)) {
        goto cleanup;
    }
    if (rename(stage, entry)) {
        if (valid_entry(entry, output)) {
            result = 1;
            goto cleanup;
        }
        if (!remove_entry(entry) || rename(stage, entry)) {
            goto cleanup;
        }
    }
    result = valid_entry(entry, output) ? 1 : -1;
cleanup:
    unlink(source);
    unlink(binary);
    unlink(manifest);
    rmdir(stage);
    return result;
}
