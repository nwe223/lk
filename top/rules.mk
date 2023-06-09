LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS := \
	app \
	arch \
	dev \
	kernel \
	platform \
	target

MODULE_SRCS := \
	$(LOCAL_DIR)/init.c \
	$(LOCAL_DIR)/main.c \

MODULE_OPTIONS := extra_warnings

include make/module.mk
