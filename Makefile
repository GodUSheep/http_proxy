all : http_proxy

http_proxy: http_proxy.cpp
	g++ -o http_proxy http_proxy.cpp -pthread

clean:
	rm -f http_proxy
	rm -f *.o

