obj-m		:= fliusb.o

KDIR		?= /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

EXTRA_CFLAGS		+= -O2 -Wall

#EXTRA_CFLAGS		+= -DDEBUG	# enable debug messages
#EXTRA_CFLAGS		+= -DASYNCWRITE	# enable asynchronous writes
EXTRA_CFLAGS		+= -DSGREAD	# enable scatter-gather reads

all: module cleanup

module:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

cleanup:
	rm -f *.o .*.cmd *.mod.c; rm -rf .tmp_versions

clean: cleanup
	rm -f *.ko test
