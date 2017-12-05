# File: Makefile
# By: Andy Sayler <www.andysayler.com>
# Adopted from work by: Chris Wailes <chris.wailes@gmail.com>
# Project: CSCI 3753 Programming Assignment 4
# Creation Date: 2010/04/06
# Modififed Date: 2012/04/12
# Description:
#	This is the Makefile for PA5.


CC           = gcc

CFLAGSFUSE   = `pkg-config fuse --cflags`
LLIBSFUSE    = `pkg-config fuse --libs`
LLIBSOPENSSL = -lcrypto

CFLAGS = -c -g -Wall -Wextra
LFLAGS = -g -Wall -Wextra

FUSE_EXAMPLES = fusehello fusexmp 
XATTR_EXAMPLES = xattr-util
OPENSSL_EXAMPLES = aes-crypt-util 

SRC=./src
OBJ=./obj

.PHONY: all fuse-examples xattr-examples openssl-examples clean

all: dir efs fuse-examples xattr-examples openssl-examples

dir:
	@mkdir -p $(OBJ)

fuse-examples: $(FUSE_EXAMPLES)
xattr-examples: $(XATTR_EXAMPLES)
openssl-examples: $(OPENSSL_EXAMPLES)

efs: $(OBJ)/efs.o
	$(CC) $(LFLAGS) $^ -o $@ $(LLIBSFUSE)

fusehello: $(OBJ)/fusehello.o
	$(CC) $(LFLAGS) $^ -o $@ $(LLIBSFUSE)

fusexmp: $(OBJ)/fusexmp.o
	$(CC) $(LFLAGS) $^ -o $@ $(LLIBSFUSE)

xattr-util: $(OBJ)/xattr-util.o
	$(CC) $(LFLAGS) $^ -o $@

aes-crypt-util: $(OBJ)/aes-crypt-util.o $(OBJ)/aes-crypt.o
	$(CC) $(LFLAGS) $^ -o $@ $(LLIBSOPENSSL)

$(OBJ)/efs.o: $(SRC)/efs.c
	$(CC) $(CFLAGS) $(CFLAGSFUSE) $< -o $@

$(OBJ)/fusehello.o: $(SRC)/fusehello.c
	$(CC) $(CFLAGS) $(CFLAGSFUSE) $< -o $@

$(OBJ)/fusexmp.o: $(SRC)/fusexmp.c
	$(CC) $(CFLAGS) $(CFLAGSFUSE) $< -o $@

$(OBJ)/xattr-util.o: $(SRC)/xattr-util.c
	$(CC) $(CFLAGS) $< -o $@

$(OBJ)/aes-crypt-util.o: $(SRC)/aes-crypt-util.c $(SRC)/aes-crypt.h
	$(CC) $(CFLAGS) $< -o $@

$(OBJ)/aes-crypt.o: $(SRC)/aes-crypt.c $(SRC)/aes-crypt.h
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(FUSE_EXAMPLES)
	rm -f $(XATTR_EXAMPLES)
	rm -f $(OPENSSL_EXAMPLES)
	rm -f $(OBJ)/*.o
	rm -f *~
	rm -f handout/*~
	rm -f handout/*.log
	rm -f handout/*.aux
	rm -f handout/*.out