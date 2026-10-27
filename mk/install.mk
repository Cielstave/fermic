# Compile a separate installed driver with final resource paths. DESTDIR only
# stages files; it must never become part of embedded runtime paths.
INSTALL ?= install
.PHONY: install
build/fermi-install: $(SOURCES) $(wildcard compiler/include/fermi/*.h) build/config.flags FORCE
	$(CC) -Icompiler/include -DFERMI_STDLIB_ROOT='"$(fermiDataDir)/library"' -DFERMI_RUNTIME_ROOT='"$(fermiDataDir)/runtime"' $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

install: build/fermi-install
	$(INSTALL) -d "$(DESTDIR)$(bindir)" "$(DESTDIR)$(fermiDataDir)"
	$(INSTALL) -m 755 build/fermi-install "$(DESTDIR)$(bindir)/fermi"
	cp -R library runtime "$(DESTDIR)$(fermiDataDir)/"
	find "$(DESTDIR)$(fermiDataDir)/library" "$(DESTDIR)$(fermiDataDir)/runtime" -type d -exec chmod 755 {} +
	find "$(DESTDIR)$(fermiDataDir)/library" "$(DESTDIR)$(fermiDataDir)/runtime" -type f -exec chmod 644 {} +
