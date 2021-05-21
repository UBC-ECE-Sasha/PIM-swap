################################################################################
#
# pimswap
#
################################################################################

PIMSWAP_VERSION = 1.0
PIMSWAP_SOURCE = pimswap-$(PIMSWAP_VERSION).tar.bz2
PIMSWAP_SITE ?= scp:///home/joel/projects/upmem/pim-swap
PIMSWAP_LICENSE = Dual BSD/GPL

PIMSWAP_CONF_OPTS = \
	--with-linux-dir=$(LINUX_DIR) \
	--with-module-dir=/lib/modules/$(LINUX_VERSION_PROBED)/pimswap

PIMSWAP_MODULE_MAKE_OPTS = EXTRA_CFLAGS=-DDEBUG
#PIMSWAP_MAKE = $(MAKE1)

$(eval $(kernel-module))
$(eval $(generic-package))
