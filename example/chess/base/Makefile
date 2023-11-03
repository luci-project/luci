CFLAGS = --std=c11 -MD -MP -Wall -D_GNU_SOURCE
SOURCE = $(wildcard *.c)
OBJECT = $(SOURCE:.c=.o)
DEPEND = $(SOURCE:.c=.d)
TARGET = chess
# Use compiler front-end wrapper for linking by default
LD = $(CC)

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJECT)
	$(LD) -o $@ $^

clean:
	rm -f $(OBJECT) $(DEPEND)

mrproper: clean
	rm -f $(TARGET)

.PHONY: all clean mrproper

-include $(DEPEND)
