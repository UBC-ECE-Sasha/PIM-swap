################################################################################
#
# wiredtiger
#
################################################################################

WIREDTIGER_VERSION = 10.0.0
WIREDTIGER_SOURCE = wiredtiger-$(WIREDTIGER_ERSION).tar.bz2
WIREDTIGER_SITE = http://source.wiredtiger.com/releases/
WIREDTIGER_LICENSE = GPL-3.0+
WIREDTIGER_LICENSE_FILES = COPYING
WIREDTIGER_INSTALL_STAGING = YES
WIREDTIGER_CONFIG_SCRIPTS = libfoo-config
WIREDTIGER_DEPENDENCIES =

define WIREDTIGER_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

# define WIREDTIGER_INSTALL_STAGING_CMDS
# 	$(INSTALL) -D -m 0755 $(@D)/libfoo.a $(STAGING_DIR)/usr/lib/libfoo.a
# 	$(INSTALL) -D -m 0644 $(@D)/foo.h $(STAGING_DIR)/usr/include/foo.h
# 	$(INSTALL) -D -m 0755 $(@D)/libfoo.so* $(STAGING_DIR)/usr/lib
# endef

# define WIREDTIGER_INSTALL_TARGET_CMDS
# 	$(INSTALL) -D -m 0755 $(@D)/libfoo.so* $(TARGET_DIR)/usr/lib
# 	$(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/foo.d
# endef

# define WIREDTIGER_USERS
# 	foo -1 libfoo -1 * - - - LibFoo daemon
# endef

# define WIREDTIGER_DEVICES
# 	/dev/foo  c  666  0  0  42  0  -  -  -
# endef

# define WIREDTIGER_PERMISSIONS
# 	/bin/foo  f  4755  foo  libfoo   -  -  -  -  -
# endef

$(eval $(generic-package))
