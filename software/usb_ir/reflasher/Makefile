NAME=iguanaIR-reflasher

all:

dev devel:
	rm -f hex/body-0.hex
	ln -s ../../../../firmware/usb_ir.body/output/usb_ir.hex hex/body-0.hex
	rm -f hex/loader-0.hex
	ln -s ../../../../firmware/usb_ir.loader/output/usb_ir.hex hex/loader-0.hex

install:
	install -d $(DESTDIR)/usr/share/$(NAME)/hex
	install $(NAME) $(DESTDIR)/usr/share/$(NAME)
	install $(filter-out hex/loader-0.hex hex/body-0.hex, $(wildcard hex/*.hex)) $(DESTDIR)/usr/share/$(NAME)/hex
	install -d $(DESTDIR)$(BINDIR)
	rm -f $(DESTDIR)$(BINDIR)/$(NAME)
	ln -s /usr/share/$(NAME)/$(NAME) $(DESTDIR)$(BINDIR)/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(NAME)
	rm -f $(DESTDIR)/usr/share/$(NAME)/hex/*.hex
	rmdir $(DESTDIR)/usr/share/$(NAME)/hex 2>/dev/null || true
	rm -f $(DESTDIR)/usr/share/$(NAME)/upgrade-usb

.PHONY: all rpm dev devel tarball install
