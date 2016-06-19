all: robotClient robotServer

robotClient:
	gcc -o robotClient robotClient.c
	
robotServer:
	gcc -o robotServer robotServer.c
clean:
	rm -f robotClient robotServer
