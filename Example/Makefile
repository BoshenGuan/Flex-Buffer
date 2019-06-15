# Makefile

EXE = Example
SRC = Example.cpp FLEX.cpp FLEX_OS.cpp

CC      = g++
RM      = rm
CFLAGS  =
LDFLAGS = -lpthread

EXE: $(SRC)
	$(CC) $^ $(CFLAGS) -o $(EXE) $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) -rf $(EXE)
