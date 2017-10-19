ifneq ($(KERNELRELEASE),)
	obj-m := slidesx-energymeter.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
insert:
	insmod slidesx-energymeter.ko

remove:
	rmmod slidesx-energymeter

rpm:
	mkdir slidesx-energymeter-1.0
	cp Makefile slidesx-energymeter-1.0/
	cp slidesx-energymeter.c slidesx-energymeter-1.0/
	cp slidesx-energymeter.h slidesx-energymeter-1.0/
	tar -czvf slidesx-energymeter.tar.gz slidesx-energymeter-1.0
	rm -rf slidesx-energymeter-1.0
	rpmdev-setuptree
	mv slidesx-energymeter.tar.gz ~/rpmbuild/SOURCES/
	cp slidesx-energymeter.spec ~/rpmbuild/SPECS/
	rpmbuild -ba ~/rpmbuild/SPECS/slidesx-energymeter.spec
endif
