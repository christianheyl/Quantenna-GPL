CFLAGS := -Os -Wall $(CFLAGS)
CC = $(CROSS_COMPILE)gcc
QWE_LIB_NAME = libqwe

all: $(QWE_LIB_NAME).so

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $<

$(QWE_LIB_NAME).so: qwe.o
	$(CC) -shared -o $@ $^

clean:
	rm -f *.o *.so
	rm -f $(TARGET_DIR)/usr/lib/$(QWE_LIB_NAME).so

install: all
	install -d $(TARGET_DIR)/usr/lib
	install -m0644 $(QWE_LIB_NAME).so $(TARGET_DIR)/usr/lib

