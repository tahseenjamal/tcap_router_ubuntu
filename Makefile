CC = gcc
TARGET = tcap-router

PKG_CONFIG = pkg-config

OSMO_CFLAGS = $(shell $(PKG_CONFIG) --cflags libosmocore libosmo-sigtran)
OSMO_LIBS   = $(shell $(PKG_CONFIG) --libs libosmocore libosmo-sigtran)

CFLAGS = -O2 -Wall -pthread $(OSMO_CFLAGS)
LIBS = -lsctp $(OSMO_LIBS)

SRC = \
main.c \
core/backend_server.c \
core/worker_pool.c \
core/msg_pool.c \
core/transaction_table.c \
router/router.c \
router/tcap_parser.c \
router/sccp_gt.c \
sigtran/sigtran_stack.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS) -pthread

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

rebuild: clean all

.PHONY: all clean rebuild
