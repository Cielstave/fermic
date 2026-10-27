#ifndef FERMI_HASH_H
#define FERMI_HASH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
void fe_sha256_bytes(const void *data, size_t length, char hex[65]);
bool fe_sha256_stream(FILE *, char hex[65]);
bool fe_sha256_file(const char *path, char hex[65]);
#endif
