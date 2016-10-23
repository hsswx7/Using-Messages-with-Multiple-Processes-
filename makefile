all: user oss
user: user.c 
	gcc -g -o user user.c
oss: oss.c 
	gcc -g -o oss oss.c
make clean: 
	rm -rf *.o *.out
