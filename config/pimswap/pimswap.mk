################################################################################
#
# pimswap
#
################################################################################

PIMSWAP_VERSION = 1.0
PIMSWAP_SOURCE = pimswap-$(PIMSWAP_VERSION).tar.bz2
PIMSWAP_SITE ?= scp:///home/shaurya/PIM-swap
PIMSWAP_LICENSE = Dual BSD/GPL
PIMSWAP_DEPENDENCIES += upmem_driver

PIMSWAP_CONF_OPTS = \
	--with-linux-dir=$(LINUX_DIR) \
	--with-module-dir=/lib/modules/$(LINUX_VERSION_PROBED)/pimswap

PIMSWAP_MODULE_MAKE_OPTS = EXTRA_CFLAGS=-I$(PWD)/build/upmem_driver-2021.3.0/modules/uapi 
#PIMSWAP_MAKE = $(MAKE1)

$(eval $(kernel-module))
$(eval $(generic-package))
