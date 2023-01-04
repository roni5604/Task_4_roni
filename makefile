all: ping watchdog2 better_ping2
ping: ping.c
	gcc ping.c -o parta
watchdog2: watchdog2.c
	gcc watchdog2.c -o watchdog2
better_ping2: better_ping2.c
	gcc better_ping2.c -o partb

clean:
	rm -f *.o parta watchdog2 partb
