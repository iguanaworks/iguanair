NAME=iguanaIR-reflasher

all:

dev devel:
	ln -s ../../usb_ir.body/output/usb_ir.hex hex/body-0.hex
	ln -s ../../usb_ir.loader/output/usb_ir.hex hex/loader-0.hex

install:
	install -d $(DESTDIR)/usr/lib/$(NAME)/hex
	install upgrade-usb $(DESTDIR)/usr/lib/$(NAME)
	install hex/*.hex $(DESTDIR)/usr/lib/$(NAME)/hex
	install -d $(DESTDIR)/usr/bin
	ln -s ../lib/$(NAME)/upgrade-usb $(DESTDIR)/usr/bin/$(NAME)

uninstall:
	rm -f $(DESTDIR)/usr/bin/$(NAME)
	rm -f $(DESTDIR)/usr/lib/$(NAME)/hex/*.hex
	rmdir $(DESTDIR)/usr/lib/$(NAME)/hex 2>/dev/null || true
	rm -f $(DESTDIR)/usr/lib/$(NAME)/upgrade-usb

.PHONY: all rpm dev devel tarball install
