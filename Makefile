# Auteur : Mohammed Amine KHAMLICHI
# LinkedIn : https://www.linkedin.com/in/mohammedaminekhamlichi/
# Makefile pour le mini-shell
# Auteur : Mohammed Amine KHAMLICHI

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
LDFLAGS = 

TARGET  = minishell

all: $(TARGET)

$(TARGET): minishell.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

minishell.o: minishell.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
