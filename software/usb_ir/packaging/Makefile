NAME    = $(shell grep Name: fedora/*.spec | head -1 | sed 's/[^ ]*[ ]*//')
VERSION = $(shell grep Version: fedora/$(NAME).spec | sed 's/[^ ]*[ ]*//')
CURRENT = $(shell head -1 /etc/issue | sed 's/ .*//' | tr 'A-Z' 'a-z')
NAMEVER = $(NAME)-$(VERSION)

ifeq ($(CURRENT),fedora)
all: fedora
clean: fedora-clean
else 
ifeq ($(CURRENT),ubuntu)
all: ubuntu
clean: ubuntu-clean
endif
endif

help:
	@echo "Failed to detect the current OS."

fedora: ../$(NAME).spec tarball
#	rpmbuild -ts $(NAMEVER).tar.bz2
#	rpmbuild -tb $(NAMEVER).tar.bz2
../$(NAME).spec:
	ln -s packaging/fedora/$(NAME).spec ..
fedora-clean:
	rm -f ../$(NAME).spec

ubuntu: ../debian
	cd ..; dpkg-buildpackage -uc -us -rfakeroot
../debian:
	ln -s packaging/debian ..
ubuntu-clean:
	rm ../debian

gentoo: tarball
gentoo-clean: 

# commands to create a tarball
tarball:
	ln -s .. $(NAMEVER); \
	tar --dereference -cjf $(NAMEVER).tar.bz2 --exclude .svn --exclude packaging/$(NAMEVER)* --exclude win32 $(NAMEVER); \
	rm $(NAMEVER);

.PHONY: help fedora ubuntu tarball
