#include "fermi/fe.h"
#include "fermi/hash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void stringLimits(void) {
    const size_t lengths[] = {SIZE_MAX, SIZE_MAX - 1, FE_MEMORY_LIMIT, FE_MEMORY_LIMIT + 1};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        FeContext context;
        assert(fe_context_init(&context, "string.fe", "", 0));
        /* A one-byte source proves rejection precedes any attempted copy. */
        assert(!fe_string(&context, "x", lengths[i]));
        assert(context.diagnostic.code && !strcmp(context.diagnostic.code, "E_LIMIT"));
        assert(!context.arena && !context.allocated);
        fe_context_destroy(&context);
    }
    FeContext context;
    assert(fe_context_init(&context, "string.fe", "", 0));
    char *empty = fe_string(&context, NULL, 0);
    assert(empty && empty[0] == 0);
    const char bytes[] = {'a', 0, 'b'};
    char *copy = fe_string(&context, bytes, sizeof(bytes));
    assert(copy && !memcmp(copy, bytes, sizeof(bytes)) && copy[sizeof(bytes)] == 0);
    fe_context_destroy(&context);
}

int main(void) {
    stringLimits();
    char hash[65];
    fe_sha256_bytes("", 0, hash);
    assert(!strcmp(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    FILE *stream = tmpfile();
    assert(stream);
    assert(fwrite("abc", 1, 3, stream) == 3);
    assert(fseek(stream, 0, SEEK_SET) == 0);
    assert(fe_sha256_stream(stream, hash));
    assert(!strcmp(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    assert(fclose(stream) == 0);
    fe_sha256_bytes("abc", 3, hash);
    assert(!strcmp(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    fe_sha256_bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hash);
    assert(!strcmp(hash, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    FeContext c;
    assert(!fe_context_init(&c, "large.fe", "", FE_SOURCE_LIMIT + 1));
    assert(!strcmp(c.diagnostic.code, "E_LIMIT"));
    fe_context_destroy(&c);
    assert(fe_context_init(&c, "memory.fe", "", 0));
    assert(!fe_alloc(&c, FE_MEMORY_LIMIT + 1));
    assert(c.diagnostic.code);
    fe_context_destroy(&c);
    puts("PASS: SHA-256 vectors and compiler resource limits");
    return 0;
}
