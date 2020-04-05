module:=rewiring
obj-m += $(module).o
ccflags-y := -std=gnu11 -g -Wno-declaration-after-statement -I$(PWD)/inc
rewiring-objs := ./src/rewiring-lkm.o ./src/global_state.o ./src/local_state.o

all: build
build:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
