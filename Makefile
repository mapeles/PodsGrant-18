TARGET := iphone:clang:latest:15.0
ARCHS := arm64e
THEOS_PACKAGE_SCHEME ?= rootless
include $(THEOS)/makefiles/common.mk

TWEAK_NAME = PodsGrant

PodsGrant_FILES = Tweak.c Sharing_Tweak.x general.c
PodsGrant_CFLAGS = -fobjc-arc -Wall -Wextra -Werror

include $(THEOS_MAKE_PATH)/tweak.mk
SUBPROJECTS += podsgranthelper
SUBPROJECTS += podsgrantsettings
include $(THEOS_MAKE_PATH)/aggregate.mk
