
all:
	$(CC) -I./libdsa -g blenderbrush.c libdsa/vector.c -lm -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
