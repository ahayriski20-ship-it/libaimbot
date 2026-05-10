LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE    := riski_aimbot
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS    := -llog -landroid -lm
LOCAL_CPPFLAGS  := -std=c++17
include $(BUILD_SHARED_LIBRARY)
