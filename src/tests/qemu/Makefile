PWD := $(shell pwd)

# Set these from the environment to override
KERNEL_VERSION ?= 4.13.5
BUILD_PATH ?= $(PWD)/../../../qemu-build
DISTFILES_PATH ?= $(PWD)/distfiles
DEBUG_KERNEL ?= no
NR_CPUS ?= 2

DOWNLOAD := wget -O
# DOWNLOAD := curl -f -o
MIRROR := https://download.wireguard.com/qemu-test/distfiles/

CHOST := $(shell gcc -dumpmachine)
ARCH := $(shell uname -m)
WIREGUARD_SOURCES := $(wildcard ../../*.c ../../*.h ../../selftest/*.h ../../crypto/*.c ../../crypto/*.h ../../crypto/*.S ../../compat/*.h)
TOOLS_SOURCES := $(wildcard ../../tools/*.c ../../tools/*.h ../../uapi/*.h)

default: qemu

# variable name, tarball project name, version, tarball extension, default URI base
define tar_download =
$(1)_VERSION := $(3)
$(1)_NAME := $(2)-$$($(1)_VERSION)
$(1)_TAR := $(DISTFILES_PATH)/$$($(1)_NAME)$(4)
$(1)_PATH := $(BUILD_PATH)/$$($(1)_NAME)
$(call file_download,$$($(1)_NAME)$(4),$(5))
endef

define file_download =
$(DISTFILES_PATH)/$(1):
	mkdir -p $(DISTFILES_PATH)
	flock -x $$@.lock -c '[ -f $$@ ] && exit 0; $(DOWNLOAD) $$@ $(MIRROR)$(1) || $(DOWNLOAD) $$@ $(2)$(1) || rm -f $$@'
endef

ifeq ($(findstring -rc,$(KERNEL_VERSION)),)
KERNEL_URL_DIRECTORY := https://cdn.kernel.org/pub/linux/kernel/v$(firstword $(subst ., ,$(KERNEL_VERSION))).x/
else
KERNEL_URL_DIRECTORY := https://git.kernel.org/torvalds/t/
endif

$(eval $(call tar_download,KERNEL,linux,$(KERNEL_VERSION),.tar.gz,$(KERNEL_URL_DIRECTORY)))
$(eval $(call tar_download,MUSL,musl,1.1.16,.tar.gz,https://www.musl-libc.org/releases/))
$(eval $(call tar_download,LIBMNL,libmnl,1.0.4,.tar.bz2,http://ftp.netfilter.org/pub/libmnl/))
$(eval $(call tar_download,IPERF,iperf,3.1.7,.tar.gz,http://downloads.es.net/pub/iperf/))
$(eval $(call tar_download,BASH,bash,4.4.12,.tar.gz,https://ftp.gnu.org/gnu/bash/))
$(eval $(call tar_download,IPROUTE2,iproute2,4.13.0,.tar.gz,https://www.kernel.org/pub/linux/utils/net/iproute2/))
$(eval $(call tar_download,IPTABLES,iptables,1.6.1,.tar.bz2,http://ftp.netfilter.org/pub/iptables/))
$(eval $(call tar_download,NMAP,nmap,7.60,.tar.bz2,https://nmap.org/dist/))
$(eval $(call tar_download,IPUTILS,iputils,s20161105,.tar.gz,https://github.com/iputils/iputils/archive/s20161105.tar.gz/#))

ifeq ($(ARCH),aarch64)
KERNEL_ARCH := arm64
KERNEL_BZIMAGE := $(KERNEL_PATH)/arch/arm64/boot/Image
QEMU_MACHINE := -machine virt,accel=kvm,gic_version=host
else
KERNEL_ARCH := $(ARCH)
KERNEL_BZIMAGE := $(KERNEL_PATH)/arch/x86/boot/bzImage
QEMU_MACHINE := -machine q35,accel=kvm
endif

export CFLAGS ?= -O3 -march=native -pipe
export CPPFLAGS := -I$(BUILD_PATH)/include

REAL_CC := $(CC)
MUSL_CC := $(BUILD_PATH)/musl-gcc
export CC := $(MUSL_CC)
USERSPACE_DEPS := $(MUSL_CC) $(BUILD_PATH)/include/.installed $(BUILD_PATH)/include/linux/.installed

build: $(KERNEL_BZIMAGE)
qemu: $(KERNEL_BZIMAGE)
	rm -f $(BUILD_PATH)/result
	qemu-system-$(ARCH) \
		-nodefaults \
		-nographic \
		$(QEMU_MACHINE) \
		-cpu host \
		-smp $(NR_CPUS) \
		-m 192M \
		-object rng-random,id=rng0,filename=/dev/urandom \
		-device virtio-rng-pci,rng=rng0 \
		-device virtio-serial,max_ports=2 \
		-chardev stdio,id=stdio \
		-device virtconsole,chardev=stdio \
		-chardev file,id=status,path=$(BUILD_PATH)/result \
		-device virtserialport,chardev=status \
		-no-reboot \
		-monitor none \
		-kernel $<
	grep -Fq success $(BUILD_PATH)/result

$(BUILD_PATH)/init-cpio-spec.txt:
	mkdir -p $(BUILD_PATH)
	echo "file /init $(BUILD_PATH)/init 755 0 0" > $@
	echo "file /init.sh $(PWD)/../netns.sh 755 0 0" >> $@
	echo "dir /dev 755 0 0" >> $@
	echo "nod /dev/console 644 0 0 c 5 1" >> $@
	echo "dir /bin 755 0 0" >> $@
	echo "file /bin/iperf3 $(IPERF_PATH)/src/iperf3 755 0 0" >> $@
	echo "file /bin/wg $(BUILD_PATH)/tools/wg 755 0 0" >> $@
	echo "file /bin/bash $(BASH_PATH)/bash 755 0 0" >> $@
	echo "file /bin/ip $(IPROUTE2_PATH)/ip/ip 755 0 0" >> $@
	echo "file /bin/ss $(IPROUTE2_PATH)/misc/ss 755 0 0" >> $@
	echo "file /bin/ping $(IPUTILS_PATH)/ping 755 0 0" >> $@
	echo "file /bin/ncat $(NMAP_PATH)/ncat/ncat 755 0 0" >> $@
	echo "file /bin/xtables-multi $(IPTABLES_PATH)/iptables/xtables-multi 755 0 0" >> $@
	echo "slink /bin/iptables xtables-multi 777 0 0" >> $@
	echo "slink /bin/ping6 ping 777 0 0" >> $@
	echo "dir /lib 755 0 0" >> $@
	echo "file /lib/libc.so $(MUSL_PATH)/lib/libc.so 755 0 0" >> $@
	echo "slink /lib/ld-linux.so.1 libc.so 777 0 0" >> $@

$(KERNEL_PATH)/.installed: $(KERNEL_TAR)
	mkdir -p $(BUILD_PATH)
	tar -C $(BUILD_PATH) -xf $<
	sed -i "/^if INET\$$/a source \"net/wireguard/Kconfig\"" $(KERNEL_PATH)/net/Kconfig
	sed -i "/^obj-\$$(CONFIG_NETFILTER).*+=/a obj-\$$(CONFIG_WIREGUARD) += wireguard/" $(KERNEL_PATH)/net/Makefile
	ln -sfT $(shell readlink -f ../..) $(KERNEL_PATH)/net/wireguard
	touch $@

$(KERNEL_PATH)/.config: kernel.config | $(KERNEL_PATH)/.installed
	cp kernel.config $(KERNEL_PATH)/minimal.config
	printf 'CONFIG_NR_CPUS=$(NR_CPUS)\nCONFIG_INITRAMFS_SOURCE="$(BUILD_PATH)/init-cpio-spec.txt"\n' >> $(KERNEL_PATH)/minimal.config
	$(MAKE) -C $(KERNEL_PATH) ARCH=$(KERNEL_ARCH) allnoconfig
	cd $(KERNEL_PATH) && scripts/kconfig/merge_config.sh -n .config minimal.config
	-[ "$(DEBUG_KERNEL)" = "yes" ] && ( cd $(KERNEL_PATH) && scripts/kconfig/merge_config.sh -n .config $(PWD)/debug.config )

$(KERNEL_BZIMAGE): $(KERNEL_PATH)/.config $(BUILD_PATH)/init-cpio-spec.txt $(MUSL_PATH)/lib/libc.so $(IPERF_PATH)/src/iperf3 $(BUILD_PATH)/tools/wg $(IPUTILS_PATH)/ping $(BASH_PATH)/bash $(IPROUTE2_PATH)/misc/ss $(IPROUTE2_PATH)/ip/ip $(IPTABLES_PATH)/iptables/xtables-multi $(NMAP_PATH)/ncat/ncat $(BUILD_PATH)/init ../netns.sh $(WIREGUARD_SOURCES) $(TOOLS_SOURCES)
	LOCALVERSION="" $(MAKE) -C $(KERNEL_PATH)

$(BUILD_PATH)/include/linux/.installed: | $(KERNEL_PATH)/.config
	LOCALVERSION="" $(MAKE) -C $(KERNEL_PATH) INSTALL_HDR_PATH=$(BUILD_PATH) headers_install
	touch $@

$(MUSL_PATH)/lib/libc.so: $(MUSL_TAR)
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	cd $(MUSL_PATH) && CC=$(REAL_CC) ./configure --prefix=/ --disable-static
	$(MAKE) -C $(MUSL_PATH)
	strip -s $@

$(BUILD_PATH)/include/.installed: $(MUSL_PATH)/lib/libc.so
	$(MAKE) -C $(MUSL_PATH) DESTDIR=$(BUILD_PATH) install-headers
	touch $@

$(MUSL_CC): $(MUSL_PATH)/lib/libc.so
	sh $(MUSL_PATH)/tools/musl-gcc.specs.sh $(BUILD_PATH)/include $(MUSL_PATH)/lib /lib/ld-linux.so.1 > $(BUILD_PATH)/musl-gcc.specs
	printf '#!/bin/sh\nexec "$(REAL_CC)" "$$@" -specs "$(BUILD_PATH)/musl-gcc.specs" -no-pie\n' > $(BUILD_PATH)/musl-gcc
	chmod +x $(BUILD_PATH)/musl-gcc

$(IPERF_PATH)/.installed: $(IPERF_TAR)
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	sed -i '1s/^/#include <stdint.h>/' $(IPERF_PATH)/src/cjson.h $(IPERF_PATH)/src/timer.h
	sed -i -r 's/-p?g//g' $(IPERF_PATH)/src/Makefile*
	touch $@

$(IPERF_PATH)/src/iperf3: | $(IPERF_PATH)/.installed $(USERSPACE_DEPS)
	cd $(IPERF_PATH) && CFLAGS="$(CFLAGS) -D_GNU_SOURCE" ./configure --prefix=/ --host=$(CHOST) --enable-static --disable-shared
	$(MAKE) -C $(IPERF_PATH)
	strip -s $@

$(LIBMNL_PATH)/.installed: $(LIBMNL_TAR)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	touch $@

$(LIBMNL_PATH)/src/.libs/libmnl.a: | $(LIBMNL_PATH)/.installed $(USERSPACE_DEPS)
	cd $(LIBMNL_PATH) && ./configure --prefix=/ --host=$(CHOST) --enable-static --disable-shared
	$(MAKE) -C $(LIBMNL_PATH)
	sed -i 's:prefix=.*:prefix=$(LIBMNL_PATH):' $(LIBMNL_PATH)/libmnl.pc

$(BUILD_PATH)/tools/wg: $(TOOLS_SOURCES) | $(LIBMNL_PATH)/src/.libs/libmnl.a $(USERSPACE_DEPS)
	mkdir -p $(BUILD_PATH)
	cp -pr ../../uapi ../../tools $(BUILD_PATH)/
	$(MAKE) -C $(BUILD_PATH)/tools clean
	LDFLAGS="$(LDFLAGS) -L$(LIBMNL_PATH)/src/.libs" $(MAKE) -C $(BUILD_PATH)/tools LIBMNL_CFLAGS="-I$(LIBMNL_PATH)/include" LIBMNL_LDLIBS="-lmnl" wg
	strip -s $@

$(BUILD_PATH)/init: init.c | $(USERSPACE_DEPS)
	mkdir -p $(BUILD_PATH)
	$(MUSL_CC) -o $@ -O3 -std=gnu11 $<
	strip -s $@

$(IPUTILS_PATH)/.installed: $(IPUTILS_TAR)
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	touch $@

$(IPUTILS_PATH)/ping: | $(IPUTILS_PATH)/.installed $(USERSPACE_DEPS)
	$(MAKE) -C $(IPUTILS_PATH) USE_CAP=no USE_IDN=no USE_NETTLE=no USE_CRYPTO=no ping
	strip -s $@

$(BASH_PATH)/.installed: $(BASH_TAR)
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	touch $@

$(BASH_PATH)/bash: | $(BASH_PATH)/.installed $(USERSPACE_DEPS)
	cd $(BASH_PATH) && ./configure --prefix=/ --host=$(CHOST) --without-bash-malloc --disable-debugger --disable-help-builtin --disable-history --disable-multibyte --disable-progcomp --disable-readline --disable-mem-scramble
	$(MAKE) -C $(BASH_PATH)
	strip -s $@

$(eval $(call file_download,271-uapi-libc-compat.h-do-not-rely-on-__GLIBC__.patch,https://raw.githubusercontent.com/lede-project/source/master/package/network/utils/iproute2/patches/))
$(eval $(call file_download,272-uapi-if_ether.h-prevent-redefinition-of-struct-ethhd.patch,https://raw.githubusercontent.com/lede-project/source/master/package/network/utils/iproute2/patches/))

$(IPROUTE2_PATH)/.installed: $(IPROUTE2_TAR) | $(DISTFILES_PATH)/271-uapi-libc-compat.h-do-not-rely-on-__GLIBC__.patch $(DISTFILES_PATH)/272-uapi-if_ether.h-prevent-redefinition-of-struct-ethhd.patch
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	patch -d $(IPROUTE2_PATH) -p 1 < $(DISTFILES_PATH)/271-uapi-libc-compat.h-do-not-rely-on-__GLIBC__.patch
	patch -d $(IPROUTE2_PATH) -p 1 < $(DISTFILES_PATH)/272-uapi-if_ether.h-prevent-redefinition-of-struct-ethhd.patch
	printf 'CC=$(CC)\nPKG_CONFIG=pkg-config\nTC_CONFIG_XT=n\nTC_CONFIG_ATM=n\nTC_CONFIG_IPSET=n\nIP_CONFIG_SETNS=y\nHAVE_ELF=n\nHAVE_MNL=y\nHAVE_BERKELEY_DB=n\nHAVE_LATEX=n\nHAVE_PDFLATEX=n\n' > $(IPROUTE2_PATH)/Config
	printf 'lib: snapshot\n\t$$(MAKE) -C lib\nip/ip: lib\n\t$$(MAKE) -C ip ip\nmisc/ss: lib\n\t$$(MAKE) -C misc ss\n' >> $(IPROUTE2_PATH)/Makefile
	touch $@

$(IPROUTE2_PATH)/ip/ip: | $(IPROUTE2_PATH)/.installed $(LIBMNL_PATH)/src/.libs/libmnl.a $(USERSPACE_DEPS)
	LDFLAGS="$(LDFLAGS) -L$(LIBMNL_PATH)/src/.libs" PKG_CONFIG_LIBDIR="$(LIBMNL_PATH)" $(MAKE) -C $(IPROUTE2_PATH) PREFIX=/ ip/ip
	strip -s $(IPROUTE2_PATH)/ip/ip

$(IPROUTE2_PATH)/misc/ss: | $(IPROUTE2_PATH)/.installed $(LIBMNL_PATH)/src/.libs/libmnl.a $(USERSPACE_DEPS)
	LDFLAGS="$(LDFLAGS) -L$(LIBMNL_PATH)/src/.libs" PKG_CONFIG_LIBDIR="$(LIBMNL_PATH)" $(MAKE) -C $(IPROUTE2_PATH) PREFIX=/ misc/ss
	strip -s $(IPROUTE2_PATH)/misc/ss

$(IPTABLES_PATH)/.installed: $(IPTABLES_TAR)
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	sed -i -e "/nfnetlink=[01]/s:=[01]:=0:" -e "/nfconntrack=[01]/s:=[01]:=0:" $(IPTABLES_PATH)/configure
	touch $@

$(IPTABLES_PATH)/iptables/xtables-multi: | $(IPTABLES_PATH)/.installed $(LIBMNL_PATH)/src/.libs/libmnl.a $(USERSPACE_DEPS)
	cd $(IPTABLES_PATH) && PKG_CONFIG_LIBDIR="$(LIBMNL_PATH)" ./configure --prefix=/ --host=$(CHOST) --enable-static --disable-shared --disable-nftables --disable-bpf-compiler --disable-nfsynproxy --disable-libipq --with-kernel=$(BUILD_PATH)/include
	$(MAKE) -C $(IPTABLES_PATH)
	strip -s $@

$(NMAP_PATH)/.installed: $(NMAP_TAR)
	mkdir -p $(BUILD_PATH)
	flock -x $<.lock true
	tar -C $(BUILD_PATH) -xf $<
	touch $@

$(NMAP_PATH)/ncat/ncat: | $(NMAP_PATH)/.installed $(USERSPACE_DEPS)
	cd $(NMAP_PATH) && ./configure --prefix=/ --host=$(CHOST) --enable-static --disable-shared --without-ndiff --without-zenmap --without-nping --with-libpcap=included --with-libpcre=included --with-libdnet=included --without-liblua --with-liblinear=included --without-nmap-update --without-openssl --with-pcap=linux
	$(MAKE) -C $(NMAP_PATH) build-ncat
	strip -s $@

clean:
	rm -rf $(BUILD_PATH)

distclean: clean
	rm -rf $(DISTFILES_PATH)

.PHONY: qemu build clean distclean
.DELETE_ON_ERROR:
