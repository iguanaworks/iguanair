INCLUDES=usb_ir.body/body.inc usb_ir.loader/loader.inc
HEADER=../software/usb_ir/iguanaIR.h
JUNK=project.* memory.inc mkdep linkfile opts.txt output obj backup

all: $(INCLUDES)

%.inc: $(HEADER) Makefile
	./utils/generate_incs --header $(HEADER) $@

clean:
	rm -rf $(INCLUDES) $(addprefix usb_ir.*/, $(JUNK))

.PHONY: all clean