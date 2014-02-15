all: filesrv dirsrv cli

filesrv: file_server.c
	gcc -g file_server.c -lsocket -lnsl -lresolv -o filesrv

dirsrv: directory_server.c
	gcc -g directory_server.c -lsocket -lnsl -lresolv -o dirsrv

cli: client.c
	gcc -g client.c -lsocket -lnsl -lresolv -o cli

clean:
	rm -rf *o directory.txt filesrv dirsrv cli 

