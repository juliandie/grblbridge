obj-elf := grblbridge

### Simplified CFLAGS
# Includes will be prefixed with -I automatically
INCLUDES ?=
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
CFLAGS += $(addprefix  -I, $(INCLUDES))
CFLAGS += $(addprefix  -L, $(LIBPATHS))
CFLAGS += $(addprefix  -D, $(DEFINES))

### LDFLAGS
LDFLAGS ?= -Wl,--start-group $(addprefix  -l, $(DEFINES)) -Wl,--end-group

obj-c := $(wildcard *.c */*.c)
### Include subdirs
# -include src/subdir.mk

obj-o := $(obj-c:%.c=%.o)

all: $(obj-elf)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(obj-elf): $(obj-o)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LLINK) -o $@

clean:
	$(RM) -Rf $(obj-elf) $(obj-o)

.PHONY: all clean