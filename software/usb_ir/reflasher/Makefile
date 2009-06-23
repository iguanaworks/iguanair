NAME=iguanaIR-reflasher
prefix=/usr

all:

dev devel:
	rm -f hex/body-0.hex
	ln -s ../../../../firmware/usb_ir.body/output/usb_ir.hex hex/body-0.hex
	rm -f hex/loader-0.hex
	ln -s ../../../../firmware/usb_ir.loader/output/usb_ir.hex hex/loader-0.hex

install:
	install -d $(DESTDIR)$(prefix)/lib/$(NAME)/hex
	install upgrade-usb $(DESTDIR)$(prefix)/lib/$(NAME)
	install $(filter-out hex/loader-0.hex hex/body-0.hex, $(wildcard hex/*.hex)) $(DESTDIR)$(prefix)/lib/$(NAME)/hex
	install -d $(DESTDIR)$(prefix)/bin
	rm -f $(DESTDIR)$(prefix)/bin/$(NAME)
	ln -s ../lib/$(NAME)/upgrade-usb $(DESTDIR)$(prefix)/bin/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/$(NAME)
	rm -f $(DESTDIR)$(prefix)/lib/$(NAME)/hex/*.hex
	rmdir $(DESTDIR)$(prefix)/lib/$(NAME)/hex 2>/dev/null || true
	rm -f $(DESTDIR)$(prefix)/lib/$(NAME)/upgrade-usb

.PHONY: all rpm dev devel tarball install
