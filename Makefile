install:
	gcc src/atermd.c -o src/atermd `pkg-config --cflags --libs gtk+-3.0 vte-2.91` -lX11
	cp src/abind /usr/bin/
	cp src/atermd /usr/bin/
	cp alinuxd.desktop /usr/share/xsessions/
	cp alinuxd-session /usr/bin/
	chmod 777 /usr/bin/alinuxd-session
	cp obconf /usr/bin/
	cp openbox /usr/bin/
	rm /usr/bin/openbox-session
