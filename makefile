all : ut

ut : ut.c
	gcc -Wall -O3 ut.c -o $@
