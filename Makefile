prefix = /usr/local

all: src/plx_eeprom

src/plx_eeprom: src/plx_eeprom.c
		@echo "CFLAGS=$(CFLAGS)"
		$(CC) $(CPPFLAGS) $(CFLAGS) $(LDCFLAGS) -o $@ $^

install: src/plx_eeprom
		install -D src/plx_eeprom \
			$(DESTDIR)$(prefix)/sbin/plx_eeprom

clean:
		-rm -f src/plx_eeprom

distclean: clean

uninstall:
		-rm -f $(DESTDIR)$(prefix)/bin/plx_eeprom

.PHONY: all install clean distclean uninstall
