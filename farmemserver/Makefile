.PHONY: clean

CFLAGS := -Wall -O2 -g -ggdb -Werror
LDLIBS := ${LDLIBS} -lrdmacm -libverbs -lpthread
CC := g++

APPS := rmserver

all: ${APPS}

clean:
	rm -f ${APPS}
