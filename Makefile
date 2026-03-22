CC = gcc
TARGET = tcap-router

CFLAGS = -O2 -Wall -pthread
LIBS = -lsctp -losomocore

SRC = \
main.c \
core/worker_pool.c \
core/msg_pool.c \
core/transaction_table.c \
router/router.c \
router/tcap_parser.c \
router/sccp_gt.c \
sigtran/sigtran_stack.c \
sigtran/m3ua_client.c \
sigtran/m3ua_server.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS) -pthread

clean:
	rm -f $(TARGET) $(OBJ)
