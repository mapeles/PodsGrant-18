TARGET := iphone:clang:latest:15.0
ARCHS := arm64e

include $(THEOS)/makefiles/common.mk

BUNDLE_NAME = podsgrantsettings

podsgrantsettings_FILES = ../general.c PGSRootListController.m PGSProductIDMappingController.m PGSProductIDEditingViewController.m PGSSelectTargetProductIDViewController.m PGSCreditsViewController.m
podsgrantsettings_INSTALL_PATH = /Library/PreferenceBundles
podsgrantsettings_CFLAGS = -fobjc-arc -Wall -Wextra -Werror

include $(THEOS_MAKE_PATH)/bundle.mk
