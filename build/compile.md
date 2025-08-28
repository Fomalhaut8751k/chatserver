cd build
cmake ..
make
cd ..
cd bin
./ChatServer 127.0.0.1 6000
./ChatServer 127.0.0.1 6002
./ChatClient 127.0.0.1 8000
