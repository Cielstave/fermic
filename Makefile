.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
CLANG_FORMAT ?= clang-format
prefix ?= /usr/local
bindir ?= $(prefix)/bin
datadir ?= $(prefix)/share
fermiDataDir = $(datadir)/fermi
LIBRARY_ROOT ?= $(CURDIR)/library
RUNTIME_ROOT ?= $(CURDIR)/runtime
FERMI_CPPFLAGS = -Icompiler/include -DFERMI_STDLIB_ROOT='"$(LIBRARY_ROOT)"' -DFERMI_RUNTIME_ROOT='"$(RUNTIME_ROOT)"'

CORE = \
	compiler/support/context.c \
	compiler/support/diagnostic.c \
	compiler/support/hash.c \
	compiler/frontend/lex.c \
	compiler/frontend/types.c \
	compiler/frontend/parse.c \
	compiler/analysis/check.c \
	compiler/analysis/budget.c \
	compiler/analysis/effects.c \
	compiler/analysis/ranges.c \
	compiler/ir/lower.c \
	compiler/ir/ownership.c \
	compiler/backend/c/statements.c \
	compiler/backend/c/function.c \
	compiler/backend/c/memory.c \
	compiler/backend/c/library.c \
	compiler/backend/c/io.c \
	compiler/backend/c/environment.c \
	compiler/backend/c/runtime.c \
	compiler/backend/c/arguments.c \
	compiler/backend/c/time.c \
	compiler/backend/c/file.c
DRIVER = compiler/driver/main.c compiler/driver/options.c compiler/driver/run.c compiler/driver/cache.c compiler/driver/build.c compiler/driver/toolchain.c compiler/driver/executable.c compiler/driver/modules.c compiler/driver/source.c compiler/driver/test.c
SOURCES = $(CORE) $(DRIVER)
CORE_OBJECTS = $(CORE:%.c=build/%.o)
OBJECTS = $(SOURCES:%.c=build/%.o)
UNIT_TESTS = build/tests/ranges build/tests/frontend build/tests/semantics build/tests/support build/tests/diagnostic
FORMAT_SOURCES = $(SOURCES) $(wildcard compiler/include/fermi/*.h) \
	$(wildcard examples/sensor_hub/*.c) \
	$(wildcard tests/performance/*.c tests/performance/*.h) \
	$(wildcard tests/unit/*.c tests/integration/*.c tests/integration/*.h tests/fuzz/*.c)
RUNTIME_SOURCES = runtime/memory/buffer.c
ARGUMENT_SOURCES = runtime/process/arguments.c
FILE_SOURCES = runtime/io/file.c
RUNTIME_HEADERS = $(wildcard runtime/include/fermi/*.h)
FORMAT_SOURCES += $(FILE_SOURCES) $(ARGUMENT_SOURCES) $(RUNTIME_SOURCES) $(RUNTIME_HEADERS) $(wildcard tests/runtime/*.c)
SANITIZERS = -fsanitize=address,undefined

.PHONY: all test format check-format clean sanitize fuzz fuzz-telemetry benchmark simulate test-runtime sanitize-runtime
all: build/fermi

build/fermi: $(OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

build/%.o: %.c build/config.flags
	@mkdir -p $(dir $@)
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/tests/%: tests/unit/%.c $(CORE_OBJECTS)
	@mkdir -p build/tests
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

build/tests/runtime-buffer: tests/runtime/buffer.c $(RUNTIME_SOURCES) $(RUNTIME_HEADERS)
	@mkdir -p build/tests
	$(CC) -Iruntime/include $(CFLAGS) tests/runtime/buffer.c $(RUNTIME_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

build/tests/runtime-arguments: tests/runtime/arguments.c $(ARGUMENT_SOURCES) $(RUNTIME_HEADERS)
	@mkdir -p build/tests
	$(CC) -Iruntime/include $(CFLAGS) tests/runtime/arguments.c $(ARGUMENT_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

build/tests/runtime-file: tests/runtime/file.c $(FILE_SOURCES) $(RUNTIME_HEADERS)
	@mkdir -p build/tests
	$(CC) -Iruntime/include $(CFLAGS) tests/runtime/file.c $(FILE_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

build/tests/runtime-file-failures: tests/runtime/file_failures.c $(FILE_SOURCES) $(RUNTIME_HEADERS)
	@mkdir -p build/tests
	$(CC) -Iruntime/include $(CFLAGS) tests/runtime/file_failures.c $(LDFLAGS) $(LDLIBS) -o $@

test-runtime: build/tests/runtime-buffer build/tests/runtime-arguments build/tests/runtime-file build/tests/runtime-file-failures
	./build/tests/runtime-buffer
	./build/tests/runtime-arguments
	./build/tests/runtime-file
	./build/tests/runtime-file-failures

sanitize-runtime:
	@mkdir -p build/sanitize
	$(CC) -Iruntime/include -std=c11 -g -O1 -Wall -Wextra -Werror -Wpedantic $(SANITIZERS) tests/runtime/buffer.c $(RUNTIME_SOURCES) -o build/sanitize/runtime-buffer
	./build/sanitize/runtime-buffer
	$(CC) -Iruntime/include -std=c11 -g -O1 -Wall -Wextra -Werror -Wpedantic $(SANITIZERS) tests/runtime/arguments.c $(ARGUMENT_SOURCES) -o build/sanitize/runtime-arguments
	./build/sanitize/runtime-arguments
	$(CC) -Iruntime/include -std=c11 -g -O1 -Wall -Wextra -Werror -Wpedantic $(SANITIZERS) tests/runtime/file.c $(FILE_SOURCES) -o build/sanitize/runtime-file
	./build/sanitize/runtime-file
	$(CC) -Iruntime/include -std=c11 -g -O1 -Wall -Wextra -Werror -Wpedantic $(SANITIZERS) tests/runtime/file_failures.c -o build/sanitize/runtime-file-failures
	./build/sanitize/runtime-file-failures

format:
	$(CLANG_FORMAT) -i $(FORMAT_SOURCES)

check-format:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SOURCES)

build/fuzz/frontend: tests/fuzz/frontend.c $(CORE) $(wildcard compiler/include/fermi/*.h)
	@mkdir -p build/fuzz
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) -std=c11 -g -O1 -fsanitize=fuzzer,address,undefined tests/fuzz/frontend.c $(CORE) -o $@

fuzz: build/fuzz/frontend
	@mkdir -p build/fuzz/corpus
	cp examples/telemetry/telemetry.fe examples/design/sum_samples.fe tests/cases/native.fe tests/cases/ranges.fe tests/fuzz/seeds/buffer_owners.fe tests/fuzz/seeds/borrow_bounds.fe tests/fuzz/seeds/buffer_growth.fe library/std/num.fe library/std/binary.fe tests/cases/bitwise.fe tests/cases/unit_result.fe tests/cases/enum_match.fe tests/cases/integer_types.fe tests/cases/loop_control.fe tests/cases/try_result.fe tests/cases/payload_enum.fe tests/fuzz/seeds/payload_owner.fe tests/fuzz/seeds/resource_struct.fe tests/fuzz/seeds/effect_contracts.fe tests/fuzz/seeds/integer_generics.fe tests/fuzz/seeds/integer_domains.fe tests/fuzz/seeds/scalar_match.fe tests/fuzz/seeds/type_aliases.fe tests/fuzz/seeds/struct_borrows.fe tests/fuzz/seeds/array_repeat.fe tests/fuzz/seeds/private_fields.fe tests/fuzz/seeds/typed_buffers.fe tests/fuzz/seeds/string_index.fe tests/fuzz/seeds/demand_generics.fe tests/fuzz/seeds/read_slices.fe tests/fuzz/seeds/data_generics.fe tests/fuzz/seeds/record_buffers.fe build/fuzz/corpus/
	./build/fuzz/frontend -runs=10000 -max_len=8192 -artifact_prefix=build/fuzz/ build/fuzz/corpus >build/fuzz/frontend.log 2>&1 || { tail -60 build/fuzz/frontend.log; exit 1; }
	@tail -1 build/fuzz/frontend.log

fuzz-telemetry: build/fermi
	@mkdir -p build/fuzz/packets
	./build/fermi build --library examples/telemetry/telemetry.fe --out-dir build/fuzz/telemetry
	$(CC) -std=c11 -g -O1 -fsanitize=fuzzer,address,undefined -Ibuild/fuzz/telemetry tests/fuzz/telemetry.c build/fuzz/telemetry/telemetry.c -o build/fuzz/telemetry-fuzz
	printf '\106\105\001\001\000\377\177' >build/fuzz/packets/one
	printf '\106\105\001\000\000' >build/fuzz/packets/empty
	./build/fuzz/telemetry-fuzz -runs=10000 -max_len=8198 -artifact_prefix=build/fuzz/ build/fuzz/packets >build/fuzz/telemetry.log 2>&1 || { tail -60 build/fuzz/telemetry.log; exit 1; }
	@tail -1 build/fuzz/telemetry.log

benchmark: build/fermi
	CC="$(CC)" sh tests/performance/run.sh

simulate: build/fermi
	./build/fermi build --library examples/sensor_hub/hub.fe --out-dir build/sensor-hub -O2
	$(CC) $(CFLAGS) -Ibuild/sensor-hub examples/sensor_hub/simulate.c build/sensor-hub/hub.o $(LDFLAGS) $(LDLIBS) -o build/sensor-hub/simulate
	./build/sensor-hub/simulate

clean:
	rm -rf build

-include $(OBJECTS:.o=.d)

include mk/tests.mk
include mk/install.mk

.PHONY: help check test-unit FORCE clean-logs
help:
	@printf '%s\n' 'Fermi development targets:' '  all              Build compiler (default)' '  check            Unit and runtime tests' '  test             Complete regression suite (requires Node.js)' '  sanitize         ASan/UBSan suites' '  check-format     Verify C formatting' '  install          Install compiler, library, runtime' '  clean-logs       Remove historical build logs' '  clean            Remove generated build output' '' 'Variables: CC CPPFLAGS CFLAGS LDFLAGS LDLIBS CLANG_FORMAT' 'Installation: prefix bindir datadir DESTDIR'

check: test-unit test-runtime
test-unit: $(UNIT_TESTS)
	@set -e; for test in $(UNIT_TESTS); do "$$test"; done

build/config.flags: FORCE
	@mkdir -p build
	@printf '%s\n' '$(CC)' '$(FERMI_CPPFLAGS) $(CPPFLAGS)' '$(CFLAGS)' '$(LDFLAGS)' '$(LDLIBS)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp

clean-logs:
	find build -maxdepth 1 -type f -name '*.log' -delete
