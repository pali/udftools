sbin_PROGRAMS = pktsetup
pktsetup_SOURCES = pktsetup.c
EXTRA_DIST = pktsetup.rules

install-data-local:
	mkdir -p "$(DESTDIR)$(libdir)/udev/rules.d"
	$(INSTALL_DATA) "$(srcdir)/pktsetup.rules" "$(DESTDIR)$(libdir)/udev/rules.d/80-pktsetup.rules"

uninstall-local:
	rm -f "$(DESTDIR)$(libdir)/udev/rules.d/80-pktsetup.rules"
