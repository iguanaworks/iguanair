#
# Template for building a lirc userspace driver out of tree.
# Requires that lirc is installed in system locations, in
# particular that the /usr/lib[64]/pkgconfig/lirc-driver.pc
# is in place (/usr/local/lib/pkgconfig/... is also OK).
# The required file plugindocs.mk might live in a -doc
# package which then is needed.
#


driver          = iguanaIR

all:  $(driver).so

CFLAGS          += $(shell pkg-config --cflags lirc-driver)
LDFLAGS         += $(shell pkg-config --libs lirc-driver)
PLUGINDIR       ?= $(shell pkg-config --variable=plugindir lirc-driver)
CONFIGDIR       ?= $(shell pkg-config --variable=configdir lirc-driver)
PLUGINDOCS      ?= $(shell pkg-config --variable=plugindocs lirc-driver)

$(driver).o: $(driver).c

$(driver).so: $(driver).o
	gcc --shared -fpic $(LDFLAGS) -o $@ $<

install: $(driver).so
	install -D $< $(DESTDIR)$(PLUGINDIR)/$<
	install -D $(driver).conf $(DESTDIR)$(CONFIGDIR)/$(driver).conf
	install -D $(driver).html $(DESTDIR)$(PLUGINDOCS)/$(driver).html
	install -Dm 644 60-blacklist-kernel-iguanair.conf \
           $(DESTDIR)/etc/modprobe.d/60-blacklist-kernel-iguanair.conf
	-test -f $(DESTDIR)$(PLUGINDOCS)/Makefile && \
	    $(MAKE) -C $(DESTDIR)$(PLUGINDOCS)

clean:
	rm -f *.o *.so
