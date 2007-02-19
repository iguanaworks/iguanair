######################################################
# Makefile
#
# This fairly complicated makefile should be able to replace many
# others which I have written.  Essentially it is a general enough
# makefile to compile nearly any kind of project I work on.  Multiple
# executables and .so's can be specified for compilation.  One trick
# was used that I would like to replace: in order to detect whether to
# link with CXX or CC the strings command is run on the prerquisites,
# and the string __gxx_personality is searched for.  If
# __gxx_personality is found then we assume the CXX linker is
# necessary.
#
# Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
#
# Distributed under the GPL version 2. See LICENSE for details.
######################################################

#list of source extensions (not header files, they're autodetected)
SRC_EXTENSIONS:=*.c *.cc *.cpp

#name of the information makefile that will be loaded
INFOFILE:=mymake.info
#name of the toplevel information file (included by all sub builds)
TOPFILE:=top.info
#name of the dependency information makefile that will be loaded
DEPFILE:=.depends

#list of user targets (first is default)
TARGETS:=all clean nodeps cleaner distclean mrproper realclean install uninstall
#list of targets that are only meaningful at the toplevel
LOCALTARGETS:=$(INFOFILE)-sample help
#drop some messages (V=1 to turn them on)
V ?= 0
#normally don't do a static build (STATIC=1 to enable)
STATIC ?= 0
# list of extensions that should be deleted when making pdfs or pses
__INTDOCEXTS__:=aux log dvi out lof lot toc bbl blg
#list of directories that should never be entered
__AVOIDDIRS__:=CVS RCS .svn

#force help to be the default target if no infofile
ifeq ($(shell ls $(INFOFILE) 2>/dev/null),)
  ifeq ($(_SYSTEM_SET_),)
    help:
	@echo No $(INFOFILE) found.
  else
    all:
  endif
else
  __GOT_INFOFILE__=1
  all:
endif

#need to know the path to the current makefile
__THISFILE__:=$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
ifeq ($(__THISFILE__), $(__THISFILE__:/%=%))
    NEXTFILE:=../$(__THISFILE__)
endif

#Set the default PREFIX early so that infofiles can use it
#NOTE: setting this to blank or / will both install to /
PREFIX=/usr/local
ROOTDIR=

# determine the system at compile time
ifeq ($(_SYSTEM_SET_),)
  _UNAME_=$(shell uname 2>/dev/null)
  ifneq ($(_UNAME_),)
    SYSTEM=$(shell echo $(_UNAME_) | tr 'A-Z' 'a-z')
  endif
  SYSTEM ?= UNKNOWN
  export SYSTEM

  SYSVAR=$(shell echo $(_UNAME_) | tr 'a-z' 'A-Z')
  CC  += -D$(SYSVAR)
  export CC
  CXX += -D$(SYSVAR)
  export CXX

  # set some defaults for freebsd
  ifeq ($(SYSTEM),freebsd)
    MAKE=gmake
  endif

  SED=sed
  export MAKE
  export SED

  ifeq ($(SYSTEM),darwin)
    GCC_SHARED_ARG=-dynamiclib
    SO_SUFFIX=dylib
  else
    GCC_SHARED_ARG=-shared
    SO_SUFFIX=so
  endif
  export GCC_SHARED_ARG
  export SO_SUFFIX

  # verbose output of the compile time system name
  ifneq ($(V),0)
    $(warning SYSTEM set to '$(SYSTEM)')
  endif

mkPath=$(if $(1),.. $(patsubst %,%/..,$(call mkPath,$(wordlist 2,100,$(1)))))
searchUp=$(patsubst %/$(1),%,$(firstword $(wildcard $(addsuffix /$(1),. $(call mkPath,$(subst /, ,$(shell pwd)))))))

  TOPDIR=$(call searchUp,top.info)
  _TOP_IN_=$(call searchUp,top.info.in)
  ifeq ($(TOPDIR),)
    TOPDIR=$(_TOP_IN_)
  endif
  ifneq ($(findstring $(_TOP_IN_),$(TOPDIR)),)
    ifneq ($(_TOP_IN_),$(TOPDIR))
      TOPDIR=$(_TOP_IN_)
    endif
  endif
  export TOPDIR

  # note that this work was already done
  export _SYSTEM_SET_ := 1
else
  ifneq ($(TOPDIR),)
    TOPDIR:=../$(TOPDIR)
    export TOPDIR
  endif
endif

#include the toplevel info file for basic settings
ifneq ($(TOPDIR),)
  __TOPFILE__:=$(TOPDIR)/$(TOPFILE)

  ifneq ($(V),0)
    $(warning Importing settings from $(__TOPFILE__))
  endif

  # NOTE: 3.80 would fail the build when failing to build $(__TOPFILE__).  3.81 continues as if it had no rules to construct $(__TOPFILE__).  Correct behavior is kind of a tossup, but to support both this must be a regular include.
  include $(__TOPFILE__)

  ifneq ($(OUTDIR),)
    OUTDIR:=$(TOPDIR)/$(OUTDIR)
  endif
else
  __TOPFILE__:=
endif

# define functions useful to mymake.info files here
override define IntObjects
$(addsuffix .o,$(notdir $(basename $(1))))
endef

#include the infofile here for all user settings
-include $(INFOFILE)

# dummy target to eliminate "missing" error message
# This allows recursion into directories without mymake.info files
$(INFOFILE):

#add onto CPPFLAGS early so that the entire file sees it
ifneq ($(DEPENDS),)
    CPPFLAGS+=$(DEPENDS:%=-I%)
endif

#directory checking section----------------------
ifeq ($(OUTDIR),)
    OUTDIR=.
endif

ifeq ($(EXECDIR),)
    EXECDIR=$(OUTDIR)
endif

ifeq ($(LIBDIR),)
    LIBDIR=$(OUTDIR)
endif

ifeq ($(INTDIR),)
    INTDIR=.
endif

ifeq ($(DOCDIR),)
    DOCDIR=$(OUTDIR)
endif

ifeq ($(MANDIR),)
    MANDIR=$(OUTDIR)
endif

ifeq ($(INBINDIR),)
    INBINDIR=$(ROOTDIR)$(PREFIX)/bin
endif
ifeq ($(INLIBDIR),)
    INLIBDIR=$(ROOTDIR)$(PREFIX)/lib
endif
ifeq ($(INCDIR),)
    INCDIR=$(ROOTDIR)$(PREFIX)/include
endif
ifeq ($(ETCDIR),)
    ETCDIR=$(ROOTDIR)$(PREFIX)/etc
endif

ifneq ($(V),0)
  $(warning Basic settings are:)
  $(warning TOPDIR=$(TOPDIR))
  $(warning OUTDIR=$(OUTDIR))
  $(warning INTDIR=$(INTDIR))
  $(warning LIBDIR=$(LIBDIR))
  $(warning EXECDIR=$(EXECDIR))
endif

# collect all the dirs for creation and deletion
_OUTDIRS_=$(sort $(INTDIR) $(EXECDIR) $(LIBDIR) $(DOCDIR) $(MANDIR))

#recursion section--------------------------------
# NOTE: using ./ because then the sed command keeps us from recursing
# into the current directory.  The possible second / is for Darwin
__ALLDIRS__:=$(patsubst /%,%,$(patsubst .%,%,$(shell find . -maxdepth 1 -type d)))

# ensure listed directories exist
__BADDIRS__:=$(filter-out $(__ALLDIRS__), $(SUBDIRS) $(SKIPDIRS))
ifneq ($(__BADDIRS__),)
    $(error The following directories from $(INFOFILE) do not exist: $(__BADDIRS__))
endif

#populate __SUBDIRS__
ifeq ($(SUBDIRS),)
    __SUBDIRS__:=$(__ALLDIRS__)
else
    __SUBDIRS__:=$(SUBDIRS)
endif

#filter __SUBDIRS__
__SUBDIRS__:=$(filter-out $(SKIPDIRS) $(__AVOIDDIRS__) \
	$(firstword $(subst /, ,$(DOCDIR))) \
	$(firstword $(subst /, ,$(MANDIR))) \
	$(firstword $(subst /, ,$(INTDIR))) \
	$(firstword $(subst /, ,$(OUTDIR))) \
	$(firstword $(subst /, ,$(LIBDIR))) \
	$(firstword $(subst /, ,$(EXECDIR))), $(__SUBDIRS__))

# for each target we have to create a bunch of rules
override define TARGET_template
  .PHONY: $(1) pre-$(1) $(__SUBDIRS__:=-$(1)) internal-$(1) local-$(1)
  ifeq ($(V),0)
    .SILENT: $(__SUBDIRS__:=-$(1))
  endif

  $(1): pre-$(1) $(__SUBDIRS__:=-$(1)) internal-$(1) local-$(1)
  pre-$(1) local-$(1):

  $(__SUBDIRS__:=-$(1)):
	$(MAKE) -C $$(@:-$(1)=) -f $(NEXTFILE) $(1)
endef

# actually instantiate the rules
$(foreach target,$(TARGETS),$(eval $(call TARGET_template,$(target))))

#final object build section-----------------------------------
#define internal-all dependencies after setting up variables
#NOTE: patsubst makes one even when empty, add prefix/suffix does not
__EXECS__=$(addprefix $(EXECDIR)/, $(EXECS))
__SOBJS__=$(addprefix $(LIBDIR)/lib, $(addsuffix .$(SO_SUFFIX),$(SOBJS) $(filter-out $(SOBJS), $(PYSWIG))))
__STATICS__=$(addprefix $(LIBDIR)/lib, $(addsuffix .a,$(SOBJS)))
__PYSWIG__=$(addprefix $(LIBDIR)/_, $(addsuffix .$(SO_SUFFIX),$(PYSWIG)))
__PDFS__:=$(addprefix $(DOCDIR)/, $(addsuffix .pdf,$(PDFS)))
__PSES__:=$(addprefix $(DOCDIR)/, $(addsuffix .ps,$(PDFS) $(PSES)))
__EPSES__:=$(addsuffix .eps,$(basename $(EPSES)))
# tough to compute all the intermediates for the document commands
__INTDOC__:=$(foreach name, $(PDFS) $(PSES), $(foreach ext, $(__INTDOCEXTS__), $(name).$(ext)))

# might as well collect all the output files
__ALL_OUTS__=$(__SOBJS__) $(__PYSWIG__) $(__EXECS__) $(__PDFS__) $(__PSES__) $(__EPSES__)

# only build static libraries if requested
ifneq ($(STATIC),0)
  __ALL_OUTS__+=$(__STATICS__)
endif

ifneq ($(V),0)
  $(warning Building: $(__ALL_OUTS__))
endif

internal-all: $(__ALL_OUTS__)
ifeq ($(strip $(__ALL_OUTS__) $(__SUBDIRS__) $(__GOT_INFOFILE__)),)
	@echo --- USELESS RECURSION INTO \`$(notdir $(shell pwd))\' ---
endif

#cleaning section------------------------------------
internal-clean:
	rm -f *~ *.pyc $(addprefix $(INTDIR)/, $(foreach file,$(SOBJS) $(EXECS) $(PYSWIG),$($(file)))) $(foreach file,$(PYSWIG),$(LIBDIR)/$(file).py) $(foreach file,$(PYSWIG),$(LIBDIR)/$(file).pyc) $(__INTDOC__) $(__ALL_OUTS__) $(__STATICS__)

internal-nodeps:
	rm -f $(DEPFILE)

internal-cleaner: internal-clean internal-nodeps
	@rmdir -p $(_OUTDIRS_) 2>/dev/null || true

internal-distclean: internal-cleaner
internal-realclean: internal-cleaner
internal-mrproper: internal-cleaner

#installation section--------------------------------
#build the lists of installed file locations
#NOTE: patsubst makes one even when empty
__SOBJS_INSTALL__=$(SOBJS:%=$(INLIBDIR)/lib%.$(SO_SUFFIX))
__EXECS_INSTALL__=$(EXECS:%=$(INBINDIR)/%)
__EXTRA_SOBJS__=$(INSTALL_SOBJS:%=$(INLIBDIR)/lib%.$(SO_SUFFIX))
__EXTRA_EXECS__=$(INSTALL_EXECS:%=$(INBINDIR)/%)
__INCS_INSTALL__=$(INSTALL_HDRS:%=$(ININCDIR)/%)
__ETC_INSTALL__=$(INSTALL_ETC:%=$(ETCDIR)/%)

#TODO: be nice if this only ran when something actually was changed
internal-install: $(__SOBJS_INSTALL__) $(__EXECS_INSTALL__) $(__EXTRA_SOBJS__) $(__EXTRA_EXECS__) $(__INCS_INSTALL__) $(__ETC_INSTALL__)
	ldconfig -n $(INLIBDIR)

#no strip on extras (may be scripts)
$(INBINDIR)/%: %
	@mkdir -p $(INBINDIR)
	install -D $< $@ $(install-$(notdir $@))

$(INBINDIR)/%: $(EXECDIR)/%
	@mkdir -p $(INBINDIR)
	install -s -D $< $@ $(install-$(notdir $@))

$(INLIBDIR)/%: $(LIBDIR)/%
	@mkdir -p $(INLIBDIR)
	install -D $< $@ $(install-$(@:$(INLIBDIR)/lib%.$(SO_SUFFIX)=%))

#NOTE: naming convention breakage (listed in file as lib.so
#      but install options go under non-lib.so variable name)
$(INLIBDIR)/%: %
	@mkdir -p $(INLIBDIR)
	install -D $< $@ $(install-$(@:$(INLIBDIR)/lib%.$(SO_SUFFIX)=%))

# try to install header files
$(ININCDIR)/%: %
	@mkdir -p $(dir $@)
	install -D $< $@ $(install-$(notdir $@))

# try to install etc files
$(ETCDIR)/%: %
	@mkdir -p $(dir $@)
	install -D $< $@ $(install-$(notdir $@))

internal-uninstall:
	rm -f $(__SOBJS_INSTALL__) $(__EXECS_INSTALL__) $(__EXTRA_SOBJS__) $(__EXTRA_EXECS__) $(__INCS_INSTALL__) $(__ETC_INSTALL__)
	@rmdir -p $(INBINDIR) $(INLIBDIR) $(INCDIR) $(ETCDIR) 2>/dev/null || true

#dependency section---------------------------------
#determine the object dependencies from the info file
__OBJECTDEPS__=$(foreach target,$(EXECS),\$$(EXECDIR)/$(target): $(addprefix \$$(INTDIR)/,$($(target)));)
__OBJECTDEPS__+=$(foreach target,$(SOBJS) $(PYSWIG),\$$(LIBDIR)/lib$(target).$(SO_SUFFIX): $(addprefix \$$(INTDIR)/,$(notdir $($(target))));)
__OBJECTDEPS__+=$(foreach target,$(SOBJS),\$$(LIBDIR)/lib$(target).a: $(addprefix \$$(INTDIR)/,$(notdir $($(target))));)

#dynamically determine the sources which build all targets
__OBJS__=$(sort $(foreach target,$(EXECS) $(SOBJS) $(PYSWIG),$($(target))))
ifeq ($(GCCSRCS),)
  GCCSRCS=$(shell ls $(SRC_EXTENSIONS) 2>/dev/null)
endif
ifneq ($(V),0)
  $(warning gcc sources are: $(GCCSRCS))
endif
__TEXSRCS__=$(sort $(PDFS) $(PSES))

# all objects depend on a few things
$(__OBJS__:%=$(INTDIR)/%): $(__THISFILE__) $(INFOFILE) $(__TOPFILE__)


# intermediate build section--------------------------
#force a rebuild of all if the makefile or infofile changes
# NOTE: INTDIR must exist, but I cannot not let make check timestamps
ifeq ($(SYSTEM),linux)
    ifneq ($(shell uname -m | grep 64),)
        CFLAGS += -fpic
        CXXFLAGS += -fpic
    endif
endif

# have to define the compile command for each object because we need
# to know whether it is C or C++.  I use 2 templates because the
# mechanism for generating C rules and C++ rules are so similar, so I
# use a template for the mechanism as well,
override define OBJECT_template
  # if there are source files in GCCSRCS not used in objects then we
  # get called without an object file, ignore this.
  ifneq ($(1),)
    # allow a lookup of whether a target's objects uses C++
    $(1):=$(2)

    ifneq ($$(V),0)
      $$(warning Using $$$$($(2)) to build $(1) from $(4))
    endif

    $$(INTDIR)/$(1): $(4)
	@mkdir -p $$(INTDIR)
	$$($(2)) -c $$(CPPFLAGS) $$($$(@:$$(INTDIR)/%=%)-CPPFLAGS) $$($(3)) $$($$(@:$$(INTDIR)/%=%)-$(3)) $(4) -o $$@
  endif
endef
override define OBJECT_MAKER_template
  $$(foreach src,$$(filter $(1),$$(GCCSRCS)),$$(eval $$(call OBJECT_template,$$(filter $$(notdir $$(basename $$(src))).%,$$(__OBJS__)),$(2),$(3),$$(src))))
endef
# indirectly create all C compile commands from the OBJECT_template
$(eval $(call OBJECT_MAKER_template,%.c,CC,CFLAGS))
# indirectly create all C++ compile commands from the OBJECT_template
$(eval $(call OBJECT_MAKER_template,%.cc %.cpp,CXX,CXXFLAGS))

# The following two templates are used to generate the rules for
# building final outputs such as executables and shared objects.
override define DYNAMIC_OUTPUT_template
  ifneq ($$(V),0)
    $$(warning Using $$$$($(2)) to build $(4) ($(1)))
  endif

  $(1):
	@mkdir -p $$(dir $(1))
	$$($(2)) $(3) $$^ $$(LDFLAGS) $$($(4)-LDFLAGS) -o $$@
endef
override define OUTPUT_MAKER_template
  $$(foreach target,$(1),$$(eval $$(call DYNAMIC_OUTPUT_template,$(2),$$(if $$(filter CXX,$$(sort $$(foreach obj,$$($$(target)),$$($$(obj))))),CXX,CC),$(3),$$(target))))
endef
# Use the above templates to generate the necessary rules
$(eval $(call OUTPUT_MAKER_template,$$(SOBJS),$$$$(LIBDIR)/lib$$(target).$$(SO_SUFFIX),$$(GCC_SHARED_ARG)))

# NOTE: this should be in an ifeq statement, but freebsd gmake chokes on it
$(eval $(call OUTPUT_MAKER_template,$$(EXECS),$$$$(EXECDIR)/$$(target)))

# NOTE: this should be the else statement
ifeq ($(STATIC),1)
  # make the exec's static
  $(eval $(call OUTPUT_MAKER_template,$$(EXECS),$$$$(EXECDIR)/$$(target),-static))

  # make static libs
  override define STATIC_OUTPUT_template
    ifneq ($$(V),0)
      $$(warning Using $$$$(AR) to build $(2) ($(1)))
    endif

    $(1):
	@mkdir -p $$(dir $(1))
	$$(AR) cru $$@ $$^
	ranlib $$@
  endef
  $(foreach target,$(SOBJS),$(eval $(call STATIC_OUTPUT_template,$(LIBDIR)/lib$(target).a,$(target))))
endif

#include the DEPFILE, or construct it as necessary
-include $(DEPFILE)

# rule to build the DEPFILE (usually .depends)
.SILENT: $(DEPFILE)
ifeq ($(GCCSRCS)$(__TEXSRCS__)$(DEPENDS),)
$(DEPFILE):
else
$(DEPFILE): $(__THISFILE__) $(INFOFILE) $(__TOPFILE__)
	echo "Computing dependencies"
	echo "# Generated file DO NOT EDIT" > $@
    # Use gcc to dynamically determine dependencies.
    ifneq ($(GCCSRCS),)
	echo "  gcc dependencies"
	echo >> $@
	echo "# Computing dependencies for GCC sources" >> $@
	echo "# $(INFOFILE) dependencies" >> $@
	echo >> $@
	echo "$(__OBJECTDEPS__)" | tr ";" "\n" | $(SED) 's/^ //' >> $@
	echo "#GCC detected dependencies" >> $@
#	echo $(CC) -MM -MP $(CPPFLAGS) $(foreach target, $(patsubst %,%.o-CPPFLAGS,$(basename $(GCCSRCS))), $($(target))) $(GCCSRCS)
	$(CC) -MM -MP $(CPPFLAGS) $(foreach target, $(patsubst %,%.o-CPPFLAGS,$(basename $(GCCSRCS))), $($(target))) $(GCCSRCS) 2>/dev/null | \
	                tr '\n' ' ' | \
	                $(SED) -e 's/ [ ]*/ /g' \
	                       -e 's/\([^: ]*.h\):/\n_FOUND_\1/g' \
	                       -e 's/\([^: ]*.o\):/\n$$(INTDIR)\/\1:/g' | \
	                sort -u | \
	                tr '\n' '!' | \
	                $(SED) -e 's/!_FOUND_\([^!]*\)/\1/g' \
	                       -e 's/!/\n\n/g' \
	                       -e 's/^/__GCC_FOUND__:=/' \
	                       -e 's/\\/\\\n/g' >> $@
    endif
    ifneq ($(__TEXSRCS__),)
	echo "  tex dependencies"
	echo >> $@
	echo "# Computing dependencies for TEX sources" >> $@
	#TODO: \input has OPTIONAL {}'s
	for target in $(__TEXSRCS__); do \
	    echo -n "$${target}.dvi $(DEPFILE): $${target}.tex " >> $@; \
	    TEXFILES=$${target}; \
	    while [ -n "$${TEXFILES}" ]; do \
	        NEWFILES=; \
	        for basename in $${TEXFILES}; do \
	            TEXDEPENDS=`grep '^[^%]*\\input{[^}]*}' $${basename}.tex 2>/dev/null | $(SED) 's/^.*\\input{\(.*\)}.*$$/\1.tex/' | tr '\n' ' '`; \
		    if [ -n "$${TEXDEPENDS}" ]; then \
	                echo "$${TEXDEPENDS}" >> $@; \
	            fi; \
		    NEWFILES="$${NEWFILES} $${TEXDEPENDS}"; \
		    \
	            BIBFILES=`grep '^[^%]*bibliography{[^}]*}' $${basename}.tex 2>/dev/null`; \
		    if [ -n "$${BIBFILES}" ]; then \
	                echo "$${basename}.dvi: $${basename}.bbl" >> $@; \
	            fi; \
	        done; \
	        TEXFILES=$${NEWFILES}; \
	    done; \
	done
    endif
    ifneq ($(DEPENDS),)
	echo "  inter-project dependencies"
	echo "" >> $@
	echo "# LDFLAGS needs changes to find the dependencies" >> $@
	echo -n "LDFLAGS+=" >> $@
	for project in $(DEPENDS); do \
	    DIRECTORY=`grep --no-filename '^[ \t]*\(OUTDIR\|LIBDIR\)[ \t]*=' $${project}/$(INFOFILE) | $(SED) -e 's/^[ \t]*\OUTDIR[ \t]*=//' -e 's/^[ \t]*LIBDIR[ \t]*=//'`; \
	    echo -n " -L$${project}/$${DIRECTORY} " >> $@; \
	    if [ $(SYSTEM) != darwin ]; then \
                echo -n " -Wl,-rpath,$${project}/$${DIRECTORY}" >> $@; \
	    fi \
	done
	echo "" >> $@
	echo "LDFLAGS+=$(addprefix -l,$(shell grep --no-filename '^[ \t]*SOBJS[ \t]*=' $(DEPENDS:=/$(INFOFILE)) | $(SED) 's/[ \t]*SOBJS[ \t]*=//'))" >> $@
    endif
	echo "" >> $@
	echo "Done"
endif

# a few rules pertaining to the DEPFILE
$(DEPFILE): $(__GCC_FOUND__) $(GCCSRCS)
$(__GCC_FOUND__):
	@if [ ! -f $@ ]; then \
	  echo WARNING: $@ required by $(DEPFILE) does not exist; \
	fi;

# sometimes this will hide odd errors on bad information files
.DEFAULT:
	@if [ -n "$(findstring $(suffix $@), $(suffix $(SRC_EXTENSIONS)))" ]; then \
	    TARGET=$(INTDIR)/$(basename $@).o; \
	    echo $@ missing, removing $${TARGET}; \
	    rm -f $${TARGET}; \
	else \
	    echo make: \*\*\* No rule to make target \`$@\'.  Stopping.; \
	    false; \
	fi

# creates the makefile link
.SILENT: Makefile
Makefile:
	ln -s $(__THISFILE__) $@

#below here are additional general rules not usually in make
%.bbl: %.aux 
	bibtex $<

%.aux: %.tex
	@COMMAND="latex -interaction=scrollmode $<"; \
	OUTPUT=$(@:.aux=.out); \
	echo $${COMMAND}; \
	$${COMMAND} > $${OUTPUT}; \
	if [ "$$?" != "0" ]; then \
	    cat $${OUTPUT}; \
	    rm $@; \
	    false; \
	fi

$(DOCDIR)/%.pdf: $(DOCDIR)/%.ps
	@mkdir -p $(DOCDIR)
	ps2pdf $< $@

# force .ps and .dvi to not be deleted (could also be done with .SECONDARY)
.PRECIOUS: %.ps %.dvi %.aux

$(DOCDIR)/%.ps: %.dvi
	@mkdir -p $(DOCDIR)
	dvips $< -o $@

%.dvi: %.tex
	@COMMAND="latex -interaction=scrollmode $<"; \
	OUTPUT=$(@:.dvi=.out); \
	while true; do \
	    echo $${COMMAND}; \
	    $${COMMAND} > $${OUTPUT}; \
	    if [ "$$?" != "0" ]; then \
		ERROR=1; \
	        break; \
	    fi; \
	    grep "Rerun to get cross-references right" --quiet $${OUTPUT}; \
	    if [ "$$?" != "0" ]; then \
	        break; \
	    fi; \
	done; \
	if [ "$${ERROR}" == "1" ]; then \
	        cat $${OUTPUT}; \
	        rm $@; \
	        false; \
	fi

%.png: %.dia
	dia $< --export=$@ --filter=png

#assorted eps generating rules
%.eps:	%.dot
	dot -Tps < $< > $@

%.eps:	%.fig
	fig2dev -Leps < $< > $@

#run R for .r files
%.eps: %.r
	R --no-save < $<

#ploticus goodness
%.eps: %.p
	pl -eps $<

#dia can output eps
#NOTE: using the builtin, not eps which defaults to eps-pango
%.eps: %.dia
	dia $< --export=$@ --filter=eps-builtin

# wmf2eps is useful
%.eps: %.wmf
	wmf2eps -o $@ $<

# how to build python swig stuff
%_wrap.c: %.i
	swig ${CPPFLAGS} -python -outdir $(LIBDIR) $<

%_wrap.c: %.i %.h
	swig ${CPPFLAGS} -python -outdir $(LIBDIR) $<

# NOTE: cannot turn off strict-aliasing or pointer cast to different
# size warnings
$(LIBDIR)/_%.$(SO_SUFFIX): %_wrap.c $(LIBDIR)/lib%.$(SO_SUFFIX)
	$(CC) $(GCC_SHARED_ARG) $(CFLAGS) -Wno-strict-aliasing -Wno-unused -Wno-implicit-function-declaration -Wno-long-long $(CPPFLAGS) $($(<:%.c=%.o)-CFLAGS) -I/usr/include/python2.4 $< -L$(LIBDIR) -l$(patsubst %_wrap.c,%,$<) -o $@

override define SAMPLE
#This is a sample $(INFOFILE) file.  This file lists
#   possible arguments, and gives an example of extending
#   compilation variables for selected files.
#The following line specifies a single executable to be built.
EXECS=dummy-exec

#The next line defines what objects make up the executable
dummy-exec=dummy.o

#It is also possible to append text to LDFLAGS for a single executable
dummy-exec-LDFLAGS=-lgd

#All of the same steps can be done to shared objects.
SOBJS=dummy-shared
dummy-shared=dummy.o

#A number of directories can be specified:
#A general output directory
OUTDIR=.
#An intermediate directory to hold .o files
INTDIR=.
#Separate directories for the executables and shared objects
EXECDIR=\$$(OUTDIR)
LIBDIR=\$$(OUTDIR)

#Also general compiler flags can be specified
CPPFLAGS=-Wall -pedantic -g
LDFLAGS=-lm

#Lastly compiler flags can be extended for specific .o files
dummy.o-CPPFLAGS=-pg
endef

#odd define needed for formatting the help output
override define NEWLINE


endef

#target to make a default infofile
.SILENT: $(INFOFILE)-sample
$(INFOFILE)-sample:
	echo -e "$(subst $(NEWLINE),\\n,$(SAMPLE))" > $@
	echo A default $@ file has been created.

#silence for all the help targets and default "no help" text
.SILENT: $(TARGETS:=-help) $(LOCALTARGETS:=-help) help-message
%-help:
	echo "    $(@:-help=) - TODO: write help entry."
	echo

all-help:
	echo "    all - This target builds the entire system recursing as"
	echo "          necessary."
	echo

clean-help:
	echo "    clean - Remove intermediate and output files."
	echo

nodeps-help:
	echo "    nodeps - Remove $(DEPFILE) files."
	echo

cleaner-help:
	echo "    cleaner - Remove intermediate and output files and empty"
	echo "              intermediate and output directories as well as"
	echo "              $(DEPFILE) files."
	echo
distclean-help:
	echo "    distclean - See cleaner."
	echo
realclean-help:
	echo "    realclean - See cleaner."
	echo
mrproper-help:
	echo "    mrproper - See cleaner."
	echo

install-help:
	echo "    install - Install the binaries and libraries."
	echo

uninstall-help:
	echo "    uninstall - Delete previously installed the binaries and"
	echo "                libraries."
	echo

$(INFOFILE)-sample-help:
	echo "    $(INFOFILE)-sample - Output a sample $(INFOFILE)"
	echo "                         ($(INFOFILE)-sample)."
	echo

help-help:
	echo "    help - Display this help message."
	echo

help-message:
	echo "Below is the help text for each target."

help: help-message $(TARGETS:=-help) $(LOCALTARGETS:=-help)
