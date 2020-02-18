TARGET = utl_fan
OBJ = $(TARGET).o 
CFILES = $(TARGET).c 
TARGET_DYNAMIC = $(TARGET)

ifeq ("$(BIN64_PATH)", "")
	LIB64_PATH=../../../bin/amd64/lib/
	BIN64_PATH=../../../bin/amd64/utils/
	LIB32_PATH=../../../bin/i386/lib/
	BIN32_PATH=../../../bin/i386/utils/
	LMBAPI_NAME=lmbapi
endif

RM=rm -rf
CP=cp -f
CC=gcc

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
   BIN_PATH=$(BIN64_PATH)
   LIB_PATH=$(LIB64_PATH)
   CBITS=-m64
else
   BIN_PATH=$(BIN32_PATH)
   LIB_PATH=$(LIB32_PATH)
   CBITS=-m32
endif


.PHONY : default clean

default: dependos

dependos:
	@echo -e "\e[1;32m==> build ${TARGET} $(LBITS)-bit........\e[m"
	@$(CC) $(CBITS) -c -o $(OBJ) $(CFILES)
	@$(CC) $(CBITS) -o $(TARGET_DYNAMIC) $(OBJ)  
	@-$(RM) *.o
	@echo -e "\e[1;32m .......................... done\e[m"

clean:
	@rm -f $(OBJ)
	@rm -f $(TARGET_DYNAMIC)

