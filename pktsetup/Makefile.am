sbin_PROGRAMS = pktsetup pktcdvd-check
pktsetup_SOURCES = pktsetup.c
pktcdvd_check_SOURCES = pktcdvd-check.c
EXTRA_DIST = pktsetup.rules

if UDEVDIR

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(UDEVDIR)/rules.d"
	$(INSTALL_DATA) "$(srcdir)/pktsetup.rules" "$(DESTDIR)$(UDEVDIR)/rules.d/80-pktsetup.rules"

uninstall-local:
	$(RM) "$(DESTDIR)$(UDEVDIR)/rules.d/80-pktsetup.rules"

endif
