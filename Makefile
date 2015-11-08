target = client
LIBS = -lpthread

all:
	gcc -o $(target) client.c $(LIBS)
clean:
	rm -rf client
