REQ_LIBS = glib-2.0 gio-2.0 gio-unix-2.0 gobject-2.0
CFLAGS += -fpic

# The wrong pkg-config (the one from the build system) was running when these
# commands were executed, so the LDFLAGS and CFLAGS definitions below are based
# on manually running those commands with the toolchain version of pkg-config.
#CFLAGS += `pkg-config --cflags ${REQ_LIBS}`
#LDFLAGS += `pkg-config --libs ${REQ_LIBS}`
CFLAGS += -pthread -I${LEGATO_SYSROOT}/usr/include/gio-unix-2.0/ -I${LEGATO_SYSROOT}/usr/include/glib-2.0 -I${LEGATO_SYSROOT}/usr/lib/glib-2.0/include
LDFLAGS += -lgio-2.0 -lgobject-2.0 -lglib-2.0

.PRECIOUS: %.c %.h

libBluezDBus.so: org.bluez.Adapter1.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

%.h: %.xml
	gdbus-codegen --interface-prefix "org.bluez." --c-namespace bluez --header --output $@ $<

%.c: %.xml
	gdbus-codegen --interface-prefix "org.bluez." --c-namespace bluez --body --output $@ $<

clean:
	$(RM) *.c *.h *.o libBluezDBus.so
