testserver:
	g++ -o Echo_server_1 Echo_server_1.cc -lVortex-Net -lpthread
	g++ -o Echo_server_2 Echo_server_2.cc -lVortex-Net -lpthread
	g++ -o Echo_server_3 Echo_server_3.cc -lVortex-Net -lpthread
	g++ -o h_test h_test.cc -lVortex-Net -lProxy -lpthread
clean:
	rm -f Echo_server_1
	rm -f Echo_server_2
	rm -f Echo_server_3
	rm -f h_test