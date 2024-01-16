# Set parent folder name as target elf
#obj-elf := $(shell basename $(shell pwd))
obj-elf := grblbridge

### Simplified CFLAGS
# Includes will be prefixed with -I automatically
INCLUDES ?= ./src
# Defines will be prefixed with -D automatically
DEFINES ?=

### Simplified LDFLAGS
# Libpaths will be prefixed with -L automatically
LIBPATHS ?= 
# Libpaths will be prefixed with -l automatically
# Add libraries like pthread to LLINK directly
LIBRARIES ?= 

### Linked libraries
LLINK := -pthread $(LIBRARIES:%=-l%)

### CFLAGS
CFLAGS := -Wextra -Wall -Og -g
CFLAGS += $(INCLUDES:%=-I%) $(LIBPATHS:%=-L%) $(DEFINES:%=-D%)

### LDFLAGS
LDFLAGS ?= -Wl,--start-group $(DEFINES:%=-l%) -Wl,--end-group

obj-c := $(wildcard *.c */*.c)
obj-cpp := $(wildcard *.cpp */*.cpp)
obj-asm := $(wildcard *.s */*.s)
### Include subdirs
# -include src/subdir.mk

obj-o := $(obj-c:%.c=%.o)
obj-o += $(obj-cpp:%.cpp=%.o)
obj-o += $(obj-asm:%.s=%.o)

all: $(obj-elf)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(obj-elf): $(obj-o)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LLINK) -o $@

ohmylib: 
	$(MAKE) -C ../../ libohmylib.a

clean:
	$(RM) -Rf $(obj-elf) $(obj-o)

.PHONY: all clean