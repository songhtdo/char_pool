CC=g++

basepath=$(cd $(dirname $0); pwd)
ODIR=obj
BDIR=bin

objects = $(ODIR)/test_char_pool.o

bin/test_char_pool : $(objects)
	test -d $(BDIR) || mkdir -p $(BDIR)
	$(CC) -o $(BDIR)/test_char_pool $(objects)

obj/test_char_pool.o : tests/test_char_pool.cpp src/char_pool.hpp
	test -d $(ODIR) || mkdir -p $(ODIR)	
	$(CC) -std=c++11 -g -o $(ODIR)/test_char_pool.o -c tests/test_char_pool.cpp -Isrc

.PHONY : clean
clean :
	-rm -rf $(ODIR) $(BDIR)