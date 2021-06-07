################################################################################
#
# upmem-driver
#
################################################################################

UPMEM_DRIVER_VERSION = 2021.1.3
UPMEM_DRIVER_SOURCE = upmem-driver-$(UPMEM_DRIVER_VERSION).tar.gz
UPMEM_DRIVER_SITE = http://sdk-releases.upmem.com/$(UPMEM_DRIVER_VERSION)
UPMEM_DRIVER_LICENSE = NONE
UPMEM_DRIVER_LICENSE_FILES = COPYING

# Archive is not actually gzipped, just use tar
define UPMEM_DRIVER_EXTRACT_CMDS
	tar --strip-components=1 -C $(@D) -xf $(UPMEM_DRIVER_DL_DIR)/upmem-driver-$(UPMEM_DRIVER_VERSION).tar.gz
endef

$(eval $(kernel-module))
$(eval $(generic-package))
