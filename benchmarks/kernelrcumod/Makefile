BASEINCLUDE ?= /usr/lib/modules/$(shell uname -r)/build
mydemo-objs := kernelrcu.o

obj-m       := kernelrcu.o
all:
	$(MAKE) -C $(BASEINCLUDE) M=$(PWD) modules
clean:
	$(MAKE) -C $(BASEINCLUDE) M=$(PWD) modules clean

