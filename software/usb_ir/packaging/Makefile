NAME    = $(shell grep Name: fedora/*.spec | head -1 | sed 's/[^ ]*[ ]*//')
VERSION = $(shell grep Version: fedora/$(NAME).spec | sed 's/[^ ]*[ ]*//')
CURRENT = $(shell head -1 /etc/issue | sed 's/ .*//' | tr 'A-Z' 'a-z')
NAMEVER = $(NAME)-$(VERSION)

all:
clean:

ifeq ($(CURRENT),fedora)
all: fedora
clean: fedora-clean
else 
ifeq ($(CURRENT),ubuntu)
all: ubuntu
clean: ubuntu-clean
else 
ifeq ($(CURRENT),debian)
all: debian
clean: debian-clean
endif
endif
endif

help:
	@echo "Failed to detect the current OS."

PWD = $(shell pwd)
fedora: tarball
	rpmbuild -bs --define "_srcrpmdir $(PWD)" --define "_sourcedir $(PWD)" fedora/$(NAME).spec
	rpmbuild -bb --define "_rpmdir $(PWD)" --define "_sourcedir $(PWD)" --define "_builddir $(PWD)/fedora" fedora/$(NAME).spec
	rm -rf fedora/$(NAMEVER)
fedora-clean:
	rm -f $(NAME)-*.tar.bz2
	rm -f $(NAME)-*.rpm

ubuntu: ../debian
	cd ..; dpkg-buildpackage -uc -us -rfakeroot -I.svn -Ipackaging/$(NAMEVER)* -Iwin32 -I*.deb -I*.rpm
	mv ../../iguanair_* .
	mv ../../iguanair-python_* .
	mv ../../iguanair-reflasher_* .

../debian:
	ln -s packaging/debian ..
ubuntu-clean:
	rm -f ../debian
	rm -f iguanair*.deb
	rm -f iguanair*.changes
	rm -f iguanair*.tar.gz
	rm -f iguanair*.dsc
	rm -f debian/iguanair.postinst
	rm -f debian/iguanair-reflasher.debhelper.log
	rm -f debian/iguanair.postrm
	rm -f debian/iguanair.prerm
	rm -fr debian/iguanair-reflasher
	rm -fr debian/iguanair-python
	rm -fr debian/tmp
	rm -f debian/substvars
	rm -f debian/debhelper.log
	rm -f debian/iguanair-python.substvars
	rm -f debian/iguanair-python.debhelper.log
	rm -f debian/files	

debian: ubuntu
debian-clean: ubuntu-clean

gentoo: tarball
gentoo-clean: 

# commands to create a tarball
tarball:
	ln -s .. $(NAMEVER); \
	tar --dereference -cjf $(NAMEVER).tar.bz2 --exclude .svn --exclude packaging/$(NAMEVER)* --exclude win32 $(NAMEVER); \
	rm $(NAMEVER);

.PHONY: help fedora ubuntu tarball
