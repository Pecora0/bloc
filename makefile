bloc: bloc.c thirdparty/devutils.h thirdparty/arena.h
	gcc -Wall -Wextra -I./thirdparty -o bloc bloc.c -lraylib -lm
