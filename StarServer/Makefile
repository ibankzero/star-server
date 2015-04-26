GameServer : main.o StarSocketServer.o GameServer.o GameDatabase.o
	g++ -o GameServer main.o StarSocketServer.o GameServer.o GameDatabase.o -L/root/mysql/lib -lmysqlclient -lpthread

main.o : main.cpp GameServer.h StarSocketServer.h
	g++ -c main.cpp -I/root/mysql/include/

StarSocketServer.o : StarSocketServer.cpp StarSocketServer.h
	g++ -c StarSocketServer.cpp -I/root/mysql/include

GameServer.o : GameServer.cpp GameServer.h StarSocketServer.h GameDatabase.h
	g++ -c GameServer.cpp -I/root/mysql/include

GameDatabase.o : GameDatabase.cpp StarSocketServer.h GameDatabase.h
	g++ -c GameDatabase.cpp -I/root/mysql/include
