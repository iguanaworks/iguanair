NAME=$(shell grep Name: *.spec | head -1 | sed 's/[^ ]*[ ]*//')
VERSION=$(shell grep Version: $(NAME).spec | sed 's/[^ ]*[ ]*//')

all:

rpm: tarball
	rpmbuild -ts $(NAME)-$(VERSION).tar.bz2
	rpmbuild -tb $(NAME)-$(VERSION).tar.bz2

tarball:
	ln -s . $(NAME)-$(VERSION); \
	tar --dereference -cjf $(NAME)-$(VERSION).tar.bz2 --exclude .svn --exclude *-0.hex `ls | grep -v '^$(NAME)-$(VERSION)\(.tar.bz2\)\?$$' | grep -v '^usb_ir.reflasher$$' | sed 's/^/$(NAME)-$(VERSION)\//'`; \
	rm $(NAME)-$(VERSION);

install:
	install -d $(DESTDIR)/usr/lib/$(NAME)/hex
	install upgrade-usb $(DESTDIR)/usr/lib/$(NAME)
	install hex/*.hex $(DESTDIR)/usr/lib/$(NAME)/hex
	install -d $(DESTDIR)/usr/bin
	ln -s ../lib/$(NAME)/upgrade-usb $(DESTDIR)/usr/bin/$(NAME)
