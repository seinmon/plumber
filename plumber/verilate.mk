# wllvm environment variables
export LLVM_COMPILER=clang
export LLVM_CC_NAME=clang-11
export LLVM_CXX_NAME=clang++-11

PROJECT_ROOT := $(shell readlink -f ..)
KLEE_BUILD_DIR := $(PROJECT_ROOT)/../plumber-klee
VERILATOR_ROOT := ../../sydex/tools/verilator
VERILATOR := $(VERILATOR_ROOT)/bin/verilator

OUT_DIR ?= out
PREFIX ?= top
OUT_NAME ?= top
COMPILED_FILE := $(OUT_DIR)/$(OUT_NAME)

STD_LIB := -std=c++11 -stdlib=libc++

VFLAGS:= --build --exe -O3 --Wall -Wno-fatal -o
CFLAGS := -CFLAGS "-O0 -Xclang -disable-O0-optnone -fno-discard-value-names \
		  -I $(KLEE_BUILD_DIR)/include $(STD_LIB)"
LDFLAGS := -LDFLAGS "-L $(KLEE_BUILD_DIR)/build/lib/ -lkleeRuntest $(STD_LIB)"

.PHONY: build
build: verilate bitcode

.PHONY: verilate
verilate:
	$(VERILATOR) $(VFLAGS) $(OUT_NAME) +incdir+$(INC_DIR) $(CFLAGS) $(LDFLAGS) \
		-Mdir $(OUT_DIR) --prefix $(PREFIX) \
		--cc $(SRC_OBJS)

.PHONY: bitcode
bitcode:
	extract-bc -l llvm-link-11 $(COMPILED_FILE)
