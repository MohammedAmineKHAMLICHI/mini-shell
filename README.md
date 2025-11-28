# Mini-shell (C/Unix)
[![CI](https://github.com/MohammedAmineKHAMLICHI/mini-shell/actions/workflows/ci.yml/badge.svg)](https://github.com/MohammedAmineKHAMLICHI/mini-shell/actions/workflows/ci.yml)

Auteur : Mohammed Amine KHAMLICHI
LinkedIn : https://www.linkedin.com/in/mohammedaminekhamlichi/

## 🎯 Résumé du projet
Mini-shell pédagogique en C illustrant la gestion des processus, des signaux, des pipes, des redirections et quelques builtins minimalistes.

## 🧭 Contexte et objectif
Projet orienté système et POSIX. Objectif principal : montrer un shell minimal qui gère l’exécution de commandes, les pipelines et les redirections tout en respectant les contraintes ASCII/Unix.

## 🔑 Fonctionnalités principales
- Exécution de commandes externes via `fork`/`execvp`.
- Pipelines avec `|`.
- Redirections d’entrée/sortie : `<`, `>` (troncature) et `>>` (ajout).
- Builtins : `cd` et `exit`.
- Gestion de `SIGINT` et reprise du terminal.

## 🛠️ Stack technique
- C11
- Outils Unix/WSL : gcc ou clang, make, bash

## ⚙️ Installation
1. Installer les outils de build (gcc/clang, make).
2. Cloner le dépôt.
3. Compiler : `make`.

## 🚀 Utilisation
Lancer le binaire compilé :
```bash
./minishell
```
Exemples :
```
mini-shell> echo hello
mini-shell> ls | wc -l
mini-shell> echo coucou > out.txt
mini-shell> cat < out.txt
mini-shell> cd ..
mini-shell> exit
```

## 🗂️ Structure du dépôt
- `minishell.c` : logique principale (parsing, exécution, signaux, builtins)
- `Makefile` : build `-Wall -Wextra -std=c11`
- `run-tests.sh` : tests de base
- `Cahier_de_charge.txt` : exigences fonctionnelles
- `.github/workflows/ci.yml` : CI GitHub Actions (build + tests)

## ✅ Tests
- Commande : `./run-tests.sh`
- CI : workflow GitHub Actions `ci.yml` (Ubuntu, build-essential)

## 🌟 Compétences mises en avant
- Programmation système POSIX (processus, signaux, I/O)
- Implémentation de pipelines et redirections
- Tests automatisés et CI pour du code C
- Rigueur ASCII et compatibilité Unix/WSL
