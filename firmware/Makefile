INCLUDES=usb_ir_body/usb_ir_body/body.inc usb_ir_loader/usb_ir_loader/loader.inc
HEADER=../software/usb_ir/iguanaIR.h
JUNK=project.* memory.inc mkdep linkfile opts.txt output obj backup lib/obj

all: $(INCLUDES)

%.inc: %.inc.in $(HEADER) Makefile
	./generate_incs --header $(HEADER) $@

clean:
	rm -rf $(INCLUDES) $(addprefix usb_ir.*/, $(JUNK))

.PHONY: all clean