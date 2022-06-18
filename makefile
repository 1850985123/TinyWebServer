CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  ./CGImysql/mySqlApp.cpp webserver.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient -Wwrite-strings

clean:
	rm  -r server
client: ./clientApp/025_client.c 
	$(CXX) -o client  $^ $(CXXFLAGS) -lpthread -lmysqlclient -Wwrite-strings
