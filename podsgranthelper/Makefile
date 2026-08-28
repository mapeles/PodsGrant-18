TARGET := iphone:clang:latest:15.0
ARCHS := arm64e

include $(THEOS)/makefiles/common.mk

TOOL_NAME = podsgrant-respringd

podsgrant-respringd_FILES = main.c
podsgrant-respringd_INSTALL_PATH = /usr/libexec
podsgrant-respringd_CFLAGS = -fblocks -Wall -Wextra -Werror

include $(THEOS_MAKE_PATH)/tool.mk
