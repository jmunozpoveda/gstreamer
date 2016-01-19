all:
	gcc -Wall -fpermissive `pkg-config --cflags --libs gstreamer-1.0` emisor.cpp -o server -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0  
	gcc -Wall -fpermissive `pkg-config --cflags --libs gstreamer-1.0` receptor.cpp -o client -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0  

clean:
	touch server client
	rm server client
	rm -f *~ *.o ${TARGETS}



