prefix = /usr/local

all: src/plx_eeprom src/ixnvdimm

src/plx_eeprom: src/plx_eeprom.c
		@echo "CFLAGS=$(CFLAGS)"
		$(CC) $(CPPFLAGS) $(CFLAGS) $(LDCFLAGS) -o $@ $^

src/ixnvdimm: src/ixnvdimm.c
		@echo "CFLAGS=$(CFLAGS)"
		$(CC) $(CPPFLAGS) $(CFLAGS) $(LDCFLAGS) -o $@ $^

install: src/plx_eeprom src/ixnvdimm
		install -D src/plx_eeprom \
			$(DESTDIR)$(prefix)/sbin/plx_eeprom
		install -D src/ixnvdimm \
			$(DESTDIR)$(prefix)/sbin/ixnvdimm

clean:
		-rm -f src/plx_eeprom src/ixnvdimm

distclean: clean

uninstall:
		-rm -f $(DESTDIR)$(prefix)/sbin/plx_eeprom \
			$(DESTDIR)$(prefix)/sbin/ixnvdimm

.PHONY: all install clean distclean uninstall
