NoNameServer : main.o StarSocketServer.o GameServer.o GameDatabase.o Utility.o
	g++ -o NoNameServer main.o StarSocketServer.o GameServer.o GameDatabase.o Utility.o -L/root/mysql/lib -lmysqlclient -lpthread

main.o : main.cpp GameServer.h StarSocketServer.h
	g++ -c main.cpp -I/root/mysql/include/

StarSocketServer.o : StarSocketServer.cpp StarSocketServer.h
	g++ -c StarSocketServer.cpp -I/root/mysql/include

GameServer.o : GameServer.cpp GameServer.h StarSocketServer.h Utility.h GameDatabase.h
	g++ -c GameServer.cpp -I/root/mtsql/include

GameDatabase.o : GameDatabase.cpp StarSocketServer.h GameDatabase.h
g++ -c GameServer.cpp -I/root/mtsql/include

Utility.o : Utility.cpp Utility.h
	g++ -c Utility.cpp -I/root/mysql/include