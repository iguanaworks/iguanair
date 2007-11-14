all:

tarball:
	NAME=`pwd | xargs basename`; \
	VERSION=`grep Version: reflasher.spec | sed 's/[^ ]*[ ]*//'`; \
	cd ..; \
	tar --dereference -cjf $${NAME}-$${VERSION}.tar.bz2 $${NAME}; \
	mv $${NAME}-$${VERSION}.tar.bz2 $${NAME};

install:
	install -d $(DESTDIR)/usr/lib/iguanaIR-reflasher/hex
	install upgrade-usb $(DESTDIR)/usr/lib/iguanaIR-reflasher
	install hex/*.hex $(DESTDIR)/usr/lib/iguanaIR-reflasher/hex
	rm $(DESTDIR)/usr/lib/iguanaIR-reflasher/hex/*-0.hex
	install -d $(DESTDIR)/usr/bin
	ln -s ../lib/iguanaIR-reflasher/upgrade-usb \
              $(DESTDIR)/usr/bin/iguanaIR-reflasher
