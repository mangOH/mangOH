REQ_LIBS = glib-2.0 gio-2.0 gio-unix-2.0 gobject-2.0
CFLAGS += -fpic
#CFLAGS += `pkg-config --cflags ${REQ_LIBS}`
CFLAGS += -pthread -I${LEGATO_SYSROOT}/usr/include/gio-unix-2.0/ -I${LEGATO_SYSROOT}/usr/include/glib-2.0 -I${LEGATO_SYSROOT}/usr/lib/glib-2.0/include
#LDFLAGS += `pkg-config --libs ${REQ_LIBS}`
LDFLAGS += -lgio-2.0 -lgobject-2.0 -lglib-2.0

.PRECIOUS: %.c %.h

libBluezDBus.so: org.bluez.Adapter1.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

#%.h: %.xml
#	gdbus-codegen --interface-prefix "org.bluez." --c-namespace bluez --header --output $@ $<
#
#%.c: %.xml
#	gdbus-codegen --interface-prefix "org.bluez." --c-namespace bluez --body --output $@ $<

%.c %.h: %.xml
	gdbus-codegen --generate-c-code=$(basename $<) --interface-prefix org.bluez. --c-namespace bluez $<

clean:
	$(RM) *.c *.h *.o libBluezDBus.so
