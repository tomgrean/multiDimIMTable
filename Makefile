CC=gcc

CFLAGS = -O2

GEN_TABLE_SRC = src/text_to_table.c

TABLE_ENGIN_SRC = src/table_engine.c

INC = -Isrc
all: genTable table_engine


genTable: $(GEN_TABLE_SRC)
	$(CC) $(CFLAGS) -o $@ $^

table_engine: $(TABLE_ENGIN_SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

clean:
	-rm -vf genTable table_engine
