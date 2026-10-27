test: build/fermi $(UNIT_TESTS) test-runtime
	./build/tests/ranges
	./build/tests/frontend
	./build/tests/semantics
	./build/tests/support
	./build/tests/diagnostic
	sh tests/run.sh
	sh tests/driver.sh
	sh tests/executable.sh
	sh tests/entryDiagnostics.sh
	sh tests/run-command.sh
	sh tests/test-command.sh
	sh tests/run-cache.sh
	sh tests/modules.sh
	sh tests/io.sh
	sh tests/printing.sh
	sh tests/input.sh
	sh tests/performance/check.sh
	sh tests/ranges.sh
	sh tests/function-exits.sh
	sh tests/control-flow.sh
	sh tests/buffers.sh
	sh tests/typed-buffers.sh
	sh tests/configuration.sh
	sh tests/bitwise.sh
	sh tests/arguments.sh
	sh tests/signed-decimal.sh
	sh tests/time.sh
	sh tests/unit-result.sh
	sh tests/try-result.sh
	sh tests/payload-enum.sh
	sh tests/resource-structs.sh
	sh tests/files.sh
	sh tests/file-output.sh
	sh tests/effect-contracts.sh
	sh tests/feature-examples.sh
	sh tests/integer-generics.sh
	sh tests/demand-generics.sh
	sh tests/data-generics.sh
	sh tests/record-buffers.sh
	sh tests/bufferReuse.sh
	sh tests/bootstrap.sh
	sh tests/bootstrapParser.sh
	sh tests/integer-domains.sh
	sh tests/scalar-match.sh
	sh tests/strings.sh
	sh tests/owned-text.sh
	sh tests/type-aliases.sh
	sh tests/struct-borrows.sh
	sh tests/array-repeat.sh
	sh tests/capability-lab.sh
	sh tests/library-system.sh
	sh tests/inventory.sh

sanitize: sanitize-runtime sanitize-support
	@mkdir -p build/sanitize
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Werror $(SANITIZERS) $(SOURCES) -o build/sanitize/fermi
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) -std=c11 -g -O1 $(SANITIZERS) tests/unit/semantics.c $(CORE) -o build/sanitize/semantics
	./build/sanitize/semantics
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) -std=c11 -g -O1 $(SANITIZERS) tests/unit/ranges.c $(CORE) -o build/sanitize/ranges
	./build/sanitize/ranges
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) -std=c11 -g -O1 $(SANITIZERS) tests/unit/diagnostic.c $(CORE) -o build/sanitize/diagnostic
	./build/sanitize/diagnostic
	FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/run.sh
	FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/driver.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/executable.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/entryDiagnostics.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/run-command.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/test-command.sh
	FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/run-cache.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/modules.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/io.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/printing.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/input.sh
	FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/performance/check.sh
	FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/ranges.sh
	FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/function-exits.sh
	FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/control-flow.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/buffers.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/typed-buffers.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/configuration.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/bitwise.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/arguments.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/signed-decimal.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/time.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/unit-result.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/try-result.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/payload-enum.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/resource-structs.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/files.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi FERMI_TEST_SANITIZE=1 sh tests/file-output.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/effect-contracts.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/feature-examples.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/integer-generics.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/demand-generics.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/data-generics.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/record-buffers.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/bufferReuse.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/bootstrap.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/bootstrapParser.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/integer-domains.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/scalar-match.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/strings.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/owned-text.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/type-aliases.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/struct-borrows.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/array-repeat.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/capability-lab.sh
	FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/library-system.sh
	FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh FERMI_TEST_COMPILER=./build/sanitize/fermi sh tests/inventory.sh


.PHONY: test-install
test-install: all
	sh tests/install.sh

.PHONY: sanitize-support
sanitize-support:
	@mkdir -p build/sanitize
	$(CC) $(FERMI_CPPFLAGS) $(CPPFLAGS) -std=c11 -g -O1 $(SANITIZERS) tests/unit/support.c $(CORE) $(LDFLAGS) $(LDLIBS) -o build/sanitize/support
	./build/sanitize/support
