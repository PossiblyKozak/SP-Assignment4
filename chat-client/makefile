#
# this makefile will compile and and all source
# found in the ~/SP-Assignment3/DR directory. 
#
# =======================================================
#                  Assignment #3
# =======================================================

BIN_DIR = ./bin
OBJ_DIR = ./obj
SRC_DIR = ./src
INC_DIR = ./inc

TARGET = ${BIN_DIR}/chat-client
OBJS = ${OBJ_DIR}/chat-client.o

CC = cc
CC_FLAGS = -I${INC_DIR} -g

# FINAL BINARY Target
${TARGET} : ${OBJS}
	${CC} ${OBJS} -o $@

#
# =======================================================
#                     Dependencies
# =======================================================
${OBJS}: ${OBJ_DIR}/%.o: ${SRC_DIR}/%.c
	${CC} -c ${CC_FLAGS} $< -o $@

#
# =======================================================
# Other targets
# =======================================================
.PHONY : clean all

all : ${TARGET}

clean:
	rm -f ${BIN_DIR}/*
	rm -f ${OBJ_DIR}/*.o
	rm -f ${INC_DIR}/*.h~
	rm -f ${SRC_DIR}/*.c~