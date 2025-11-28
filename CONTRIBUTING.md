# Contribuer au mini-shell

Quelques règles simples :

- Auteur principal : Mohammed Amine KHAMLICHI.
- Cible : Unix/WSL, C11, appels POSIX.
- Compilation sans avertissements : `-Wall -Wextra -std=c11`.
- ASCII uniquement dans le code et les fichiers textes.
- Comportement existant à conserver : pas de support de fonctionnalités avancées (quotes, jobs en arrière-plan) sans discussion préalable.
- Tests : `./run-tests.sh` doit rester vert. Ajouter des tests pertinents en cas de modification du parsing ou des redirections.

Processus proposé :
1. Fork puis branche dédiée.
2. Implémenter les changements avec des commits clairs.
3. Vérifier `make` puis `./run-tests.sh`.
4. Ouvrir une PR en décrivant le comportement attendu et les validations effectuées.
