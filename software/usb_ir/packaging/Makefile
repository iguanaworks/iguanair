CURRENT = $(shell head -1 /etc/issue | sed 's/ .*//' | tr 'A-Z' 'a-z')

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

fedora: ../iguanaIR.spec
 ../iguanaIR.spec:
	ln -s packaging/fedora/iguanaIR.spec ..

fedora-clean:
	rm -f ../iguanaIR.spec

ubuntu: ../debian
../debian:
	ln -s packaging/debian ..
	cd ..; dpkg-buildpackage -uc -us -rfakeroot

ubuntu-clean:
	rm ../debian

.PHONY: help fedora ubuntu
