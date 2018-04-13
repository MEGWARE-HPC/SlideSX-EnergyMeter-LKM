ifneq ($(KERNELRELEASE),)
	obj-m := slidesx-energymeter.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

RELEASEVER := $(shell grep "Version" slidesx-energymeter.spec | tr "\t" "\n" | tail -n 1)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
insert:
	insmod slidesx-energymeter.ko

remove:
	rmmod slidesx-energymeter

rpm:
	mkdir slidesx-energymeter-$(RELEASEVER)
	cp Makefile slidesx-energymeter-$(RELEASEVER)/
	cp slidesx-energymeter.c slidesx-energymeter-$(RELEASEVER)/
	cp slidesx-energymeter.h slidesx-energymeter-$(RELEASEVER)/
	tar -czvf slidesx-energymeter.tar.gz slidesx-energymeter-$(RELEASEVER)
	rm -rf slidesx-energymeter-$(RELEASEVER)
	mkdir -p ~/rpmbuild/BUILD
	mkdir -p ~/rpmbuild/RPMS
	mkdir -p ~/rpmbuild/SOURCES
	mkdir -p ~/rpmbuild/SPECS
	mkdir -p ~/rpmbuild/SRPMS
	mv slidesx-energymeter.tar.gz ~/rpmbuild/SOURCES/
	cp slidesx-energymeter.spec ~/rpmbuild/SPECS/
	rpmbuild -ba ~/rpmbuild/SPECS/slidesx-energymeter.spec
endif
