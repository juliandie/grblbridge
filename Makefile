
ifeq ($(BUILD_SRC),)
	SRC_DIR := .
else
	ifeq ($(BUILD_SRC)/,$(dir $(CURDIR)))
		SRC_DIR := ..
	else
		SRC_DIR := $(BUILD_SRC)
	endif
endif

### CFLAGS
CFLAGS := -Wextra -Wall -O2 -g
CFLAGS += $(call cc-option,-fno-PIE)
### Extend CFLAGS
INCLUDES := ./
DEFINES ?=
CFLAGS += $(INCLUDES:%=-I$(SRC_DIR)/%)
CFLAGS += $(DEFINES:%=-D%)

### Extend LDFLAGS
LIBPATHS :=
LIBRARIES := 

LDFLAGS += $(LIBPATHS:%=-L%)
LDFLAGS += $(DEFINES:%=-D%)
LDFLAGS += -Wl,--start-group $(LIBRARIES:%=-l%) -Wl,--end-group
LLINK := -pthread $(LIBRARIES:%=-l%)

C_SRC := $(wildcard $(SRC_DIR)/*.c)
C_OBJ := $(C_SRC:%.c=%.c.o)

PHONY += all
all: grblbridge

grblbridge: $(C_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LLINK) -o $@
	
%.c.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@
	
%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

PHONY += clean
clean:
	$(RM) -Rf grblbridge $(C_OBJ)

PHONY += mrproper
mrproper: clean

PHONY += re
re: clean all

.PHONY: $(PHONY)

# vim: noet ts=8 sw=8
