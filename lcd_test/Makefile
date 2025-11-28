CC := gcc
CFLAGS := -Wall -Wextra -O2

# Pull gpiod flags via pkg-config if available, otherwise fall back to -lgpiod
GPIOD_CFLAGS := $(shell pkg-config --cflags gpiod 2>/dev/null)
GPIOD_LIBS := $(shell pkg-config --libs gpiod 2>/dev/null)
ifeq ($(strip $(GPIOD_LIBS)),)
GPIOD_LIBS := -lgpiod
endif

CFLAGS += $(GPIOD_CFLAGS)
LDLIBS := $(GPIOD_LIBS) -lm

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
TARGET := lcd_test

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(OBJS) $(TARGET)
