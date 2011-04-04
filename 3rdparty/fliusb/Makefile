obj-m		:= fliusb.o

KDIR		?= /lib/modules/$(shell uname -r)/build
KROOT           ?= /lib/modules/$(shell uname -r)
PWD		:= $(shell pwd)

EXTRA_CFLAGS		+= -O2 -Wall

#EXTRA_CFLAGS		+= -DDEBUG	# enable debug messages
#EXTRA_CFLAGS		+= -DASYNCWRITE	# enable asynchronous writes
EXTRA_CFLAGS		+= -DSGREAD	# enable scatter-gather reads

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

clean:
	find . -type d -exec sh -c "cd {} && rm -f *.ko *.o *.mod.o *.mod.c" \;
	rm -rf .tmp_versions Module.symvers

