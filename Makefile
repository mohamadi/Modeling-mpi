CC=mpicc
CFLAGS=-c
LDFLAGS=-lm
EXEC=mnbody
SRCS=mnbody.c
OBJS=$(SRCS:.c=.o)
$(EXEC):$(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)
.c.o:
	$(CC) $(CFLAGS) $< -o $@
