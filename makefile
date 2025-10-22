bloc: bloc.c rgfw.o
	gcc -Wall -Wextra -I./thirdparty -o bloc bloc.c rgfw.o -lm -lX11 -lXrandr

rgfw.o: rgfw.c
	gcc -I./thirdparty -c rgfw.c
