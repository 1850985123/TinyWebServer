CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

test_time: time-test.c 
	$(CXX) -o test_time $^ $(CXXFLAGS) -lpthread -lmysqlclient -Wwrite-strings  -levent

clean:
	rm  -r test_time

