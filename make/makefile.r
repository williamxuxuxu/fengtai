# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation


ARCH       = X86  # Architecture, surport X86/ARM/PPC
TARGET     = router

INCLUDE    = -I./include
INCLUDE   += -I./lib/include
SRC        = ./src
BUILD      = ./build

WERROR_FLAGS  = -Wall
WERROR_FLAGS += -Wextra
WERROR_FLAGS += -Wstrict-prototypes
WERROR_FLAGS += -Wmissing-prototypes
WERROR_FLAGS += -Wold-style-definition
WERROR_FLAGS += -Wpointer-arith
WERROR_FLAGS += -Wcast-align
WERROR_FLAGS += -Wnested-externs
WERROR_FLAGS += -Wcast-qual
WERROR_FLAGS += -Wformat-nonliteral
WERROR_FLAGS += -Wformat-security
WERROR_FLAGS += -Wundef
WERROR_FLAGS += -Wwrite-strings
WERROR_FLAGS += -Wdeprecated
WERROR_FLAGS += -Wno-format-truncation
WERROR_FLAGS += -DALLOW_EXPERIMENTAL_API
ifeq ($(strip $(ARCH)), X86) 
	WERROR_FLAGS += -Wmissing-declarations
	WERROR_FLAGS += -Wimplicit-fallthrough=2
endif

ifeq ($(strip $(ARCH)), X86)
	CC      = gcc -D cmdline
endif

CFLAGS  = $(WERROR_FLAGS) -O0 -g  -std=gnu11 # compiler opts & include(use build/include)
CFLAGS += $(INCLUDE)
ifeq ($(strip $(ARCH)), X86)
	CFLAGS  += -mssse3
endif
LDFLAGS = -L./lib # lib-dir e.g. -L/libdir
LIBS    = -lpthread -lpcap -l:librte_mbuf.a -l:librte_mempool.a -l:librte_ring.a  # lib e.g. -l:/xx.a


SRCS = $(shell find $(SRC) -name "*.c")
OBJ  = $(patsubst %.c, %.o, $(SRCS) )

all : $(TARGET)
$(TARGET) : $(OBJ)
	@mkdir -p $(BUILD)
	$(CC) -g $(OBJ) -o ./build/$@  $(LDFLAGS) $(LIBS)

%.o : %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY : clean
clean:
	@rm -rf  $(BUILD)/$(TARGET) $(SRC)/*.o