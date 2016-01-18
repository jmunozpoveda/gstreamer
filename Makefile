all:
	gcc -Wall `pkg-config --cflags --libs gstreamer-1.0` emisor.cpp -o sender -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 
	gcc -Wall `pkg-config --cflags --libs gstreamer-1.0` receptor.cpp -o receiver -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 

clean:
	touch sender receiver
	rm sender receiver
	rm -f *~ *.o ${TARGETS}



