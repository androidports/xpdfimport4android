HOST=arm-linux-androideabi
BASE_DIR=$(shell pwd)

PROJECT_NAME=poppler

PREFIX_DIR=$(BASE_DIR)/prebuild
POPPLER_SOURCE_DIR=$(BASE_DIR)/gpl/poppler-0.8.0
FONTCONFIG_SOURCE_DIR=$(BASE_DIR)/other/fontconfig-2.11.1
ZLIB_SOURCE_DIR=$(BASE_DIR)/other/zlib-1.2.3
EXPAT_SOURCE_DIR=$(BASE_DIR)/other/expat-2.1.0
FREETYPE_SOURCE_DIR=$(BASE_DIR)/bsd/freetype-2.4.10
JPEG_SOURCE_DIR=$(BASE_DIR)/other/jpeg-8c

DEST_DIR=$(PREFIX_DIR)/$(PROJECT_NAME)
PKG_CONFIG_PATH=$(DEST_DIR)/lib/pkgconfig


all: zlib_all jpeg_all expat_all freetype_all fontconfig_all poppler_all
	ndk-build

clean_all:
	rm -rf $(DEST_DIR)


poppler_all: poppler_configure poppler_build poppler_install

poppler_configure:
	cd $(POPPLER_SOURCE_DIR); PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) LDFLAGS=-L$(DEST_DIR)/lib ./configure --host=$(HOST) --prefix=$(DEST_DIR) --disable-cms --disable-libopenjpeg --disable-cairo-output --enable-xpdf-headers --disable-abiword-output --disable-poppler-glib

poppler_build:
	cd $(POPPLER_SOURCE_DIR); make

poppler_install:
	cd $(POPPLER_SOURCE_DIR); make install

poppler_clean:
	cd $(POPPLER_SOURCE_DIR); make clean


jpeg_all: jpeg_configure jpeg_build jpeg_install

jpeg_configure:
	cd $(JPEG_SOURCE_DIR); PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) ./configure --host=$(HOST) --prefix=$(DEST_DIR)

jpeg_build:
	cd $(JPEG_SOURCE_DIR); make

jpeg_install:
	cd $(JPEG_SOURCE_DIR); make install

jpeg_clean:
	cd $(JPEG_SOURCE_DIR); make clean


fontconfig_all: fontconfig_configure fontconfig_build fontconfig_install

fontconfig_configure:
	cd $(FONTCONFIG_SOURCE_DIR); PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) ./configure --host=$(HOST) --prefix=$(DEST_DIR) --enable-static

fontconfig_build:
	cd $(FONTCONFIG_SOURCE_DIR); make

fontconfig_install:
	cd $(FONTCONFIG_SOURCE_DIR); make install

fontconfig_clean:
	cd $(FONTCONFIG_SOURCE_DIR); make clean


zlib_all: zlib_configure zlib_build zlib_install

zlib_configure:
	cd $(ZLIB_SOURCE_DIR); PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) CC=arm-linux-androideabi-gcc LD=arm-linux-androideabi-ld AR="arm-linux-androideabi-ar -rc" RANLIB=arm-linux-androideabi-ranlib ./configure --prefix=$(DEST_DIR)

zlib_build:
	cd $(ZLIB_SOURCE_DIR); make

zlib_install:
	cd $(ZLIB_SOURCE_DIR); make install

zlib_clean:
	cd $(ZLIB_SOURCE_DIR); make clean


expat_all: expat_configure expat_build expat_install

expat_configure:
	cd $(EXPAT_SOURCE_DIR); PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) ./configure --host=$(HOST) --prefix=$(DEST_DIR)

expat_build:
	cd $(EXPAT_SOURCE_DIR); make

expat_install:
	cd $(EXPAT_SOURCE_DIR); make install

expat_clean:
	cd $(EXPAT_SOURCE_DIR); make clean


freetype_all: freetype_configure freetype_build freetype_install

freetype_configure:
	cd $(FREETYPE_SOURCE_DIR); PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) ./configure --host=$(HOST) --prefix=$(DEST_DIR)

freetype_build:
	cd $(FREETYPE_SOURCE_DIR); make

freetype_install:
	cd $(FREETYPE_SOURCE_DIR); make install

freetype_clean:
	cd $(FREETYPE_SOURCE_DIR); make clean

