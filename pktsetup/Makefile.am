sbin_PROGRAMS = pktsetup
pktsetup_SOURCES = pktsetup.c
EXTRA_DIST = pktsetup.rules

if UDEVDIR

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(UDEVDIR)/rules.d"
	$(INSTALL_DATA) "$(srcdir)/pktsetup.rules" "$(DESTDIR)$(UDEVDIR)/rules.d/80-pktsetup.rules"

uninstall-local:
	$(RM) "$(DESTDIR)$(UDEVDIR)/rules.d/80-pktsetup.rules"

endif
