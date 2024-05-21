all: build_server build_client build_tram

build_server:
	c++ -I. -DICE_CPP11_MAPPING -c mpk.cpp server.cpp
	c++ -o server mpk.o server.o -lIce++11
build_client:
	c++ -I. -DICE_CPP11_MAPPING -c mpk.cpp client.cpp
	c++ -o client mpk.o client.o -lIce++11

build_tram:
	c++ -I. -DICE_CPP11_MAPPING -c mpk.cpp tram.cpp
	c++ -o tram mpk.o tram.o -lIce++11