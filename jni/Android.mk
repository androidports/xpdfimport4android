LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := xpdfimport
LOCAL_SRC_FILES := pdfioutdev_gpl.cxx pnghelper.cxx wrapper_gpl.cxx
LOCAL_CFLAGS := -DUNX -DUNIX -DGCC -DLINUX -DANDROID -DCPPU_ENV=gcc3 -DGXX_INCLUDE_PATH= -DARM32 -DARM -D__ARM_EABI__ -DGLIBC=2
LOCAL_CFLAGS += -DSYSTEM_ZLIB -DSYSTEM_POPPLER -fPIE -I$(LOCAL_PATH)/../prebuild/poppler/include -I$(LOCAL_PATH)/../prebuild/poppler/include/poppler
LOCAL_LDFLAGS := -fPIE -pie -L$(LOCAL_PATH)/../prebuild/poppler/lib -Wl,-Bstatic -lpoppler -lfontconfig -lfreetype -lexpat -ljpeg -lz -Wl,-Bdynamic -ldl

include $(BUILD_EXECUTABLE)
