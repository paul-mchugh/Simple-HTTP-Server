CLINK = -lpthread -lrt -lm
COPT  = -o3
XNAME = cHTTPserver

program: main.o mtqueue.o
	gcc $(COPT) -o $(XNAME) main.o mtqueue.o $(CLINK)

main.o: main.c mtqueue.h
	gcc -c $(COPT) -o main.o main.c

mtqueue.o: mtqueue.h mtqueue.c
	gcc -c $(COPT) -o mtqueue.o mtqueue.c

.PHONY: clean

clean:
	rm *.o
