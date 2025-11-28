#!/usr/bin/env bash
# Auteur : Mohammed Amine KHAMLICHI
# LinkedIn : https://www.linkedin.com/in/mohammedaminekhamlichi/
set -e

echo "Compilation..."
make

echo
echo "Test 1 : commande simple"
echo 'echo hello world' | ./minishell

echo
echo "Test 2 : pipeline"
printf 'echo hello world | wc -w\n' | ./minishell

echo
echo "Test 3 : redirection > et <"
TMPFILE=/tmp/minishell_test.txt
printf 'echo coucou > '"$TMPFILE"'\ncat < '"$TMPFILE"'\n' | ./minishell
rm -f "$TMPFILE"

echo
echo "Test 4 : builtin cd"
printf 'pwd\ncd ..\npwd\n' | ./minishell

echo
echo "Tous les tests de base ont ete executes."
