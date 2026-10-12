# sophon-dl.exe: make            (needs x86_64-w64-mingw32-gcc, curl)
# host self-test against live data: make check   (macOS: CommonCrypto MD5)

CROSS ?= x86_64-w64-mingw32-
HOSTCC ?= cc

ZSTD_VER  = 1.5.7
ZSTD_SHA  = eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
CJSON_VER = 1.7.18
CJSON_SHA = 3aa806844a03442c00769b83e99970be70fbef03735ff898f4811dd03b9f5ee5

Z = deps/zstd-$(ZSTD_VER)/lib
J = deps/cJSON-$(CJSON_VER)
DEPS_SRC = $(addprefix $(Z)/common/,debug.c entropy_common.c error_private.c fse_decompress.c \
             xxhash.c zstd_common.c) \
           $(addprefix $(Z)/decompress/,huf_decompress.c zstd_ddict.c zstd_decompress.c \
             zstd_decompress_block.c) \
           $(J)/cJSON.c
CFLAGS = -O2 -Wall -I$(Z) -I$(J) -DZSTD_DISABLE_ASM

sophon-dl.exe: main.c sophon.c sophon.h deps/.ok
	$(CROSS)gcc $(CFLAGS) -municode -mwindows -o $@ main.c sophon.c $(DEPS_SRC) \
	    -lwinhttp -ladvapi32 -lshell32 -static
	$(CROSS)strip $@

selftest: selftest.c sophon.c sophon.h deps/.ok
	$(HOSTCC) $(CFLAGS) -g -fsanitize=address,undefined -o $@ selftest.c sophon.c $(DEPS_SRC)

check: selftest
	./check.sh

deps/.ok:
	mkdir -p deps
	curl -fsSL -o deps/zstd.tar.gz \
	    https://github.com/facebook/zstd/releases/download/v$(ZSTD_VER)/zstd-$(ZSTD_VER).tar.gz
	curl -fsSL -o deps/cjson.tar.gz \
	    https://github.com/DaveGamble/cJSON/archive/refs/tags/v$(CJSON_VER).tar.gz
	echo "$(ZSTD_SHA)  deps/zstd.tar.gz" | shasum -a 256 -c
	echo "$(CJSON_SHA)  deps/cjson.tar.gz" | shasum -a 256 -c
	tar -xzf deps/zstd.tar.gz -C deps
	tar -xzf deps/cjson.tar.gz -C deps
	touch $@

clean:
	rm -rf sophon-dl.exe selftest selftest.dSYM live

.PHONY: check clean
