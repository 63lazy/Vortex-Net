testserver:
	g++ -o Echo_server_1 Echo_server_1.cc -lVortex-Net -lpthread
	g++ -o Echo_server_2 Echo_server_2.cc -lVortex-Net -lpthread
	g++ -o Echo_server_3 Echo_server_3.cc -lVortex-Net -lpthread
	g++ -o h_test h_test.cc -lL4_Proxy -lVortex-Net -lpthread
L7_test:
	g++ -o L7_main L7/main.cc -lL7_Proxy -lVortex-Net -lpthread
clean:
	rm -f Echo_server_1 Echo_server_2 Echo_server_3 h_test L7_main