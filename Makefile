
EXT_INC := -I./libdsa -I./microtar
EXT_SRC := libdsa/vector.c microtar/microtar.c

all:
	$(CC) $(EXT_INC) -g blenderbrush.c $(EXT_SRC) -lm -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment

valgrind:
	$(CC) $(EXT_INC) -g blenderbrush.c $(EXT_SRC) -lm

release:
	$(CC) $(EXT_INC) -Os blenderbrush.c $(EXT_SRC) -lm 