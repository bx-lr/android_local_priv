LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := levitator
LOCAL_SRC_FILES := levitator.c

include $(BUILD_EXECUTABLE)
