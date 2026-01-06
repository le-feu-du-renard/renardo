# Dryer

```
pio run

# run and upload to pico
pio run -t upload -t monitor -e pico_w

# test on local machine
pio test -vvv -e native
```

## generate fonts

## generate icons

TODO:

- ajouter params cycle curation configurable
- s'assurer que les barres de progression fonctionnent

- améliorer le menu : ajouter des unités + mettre en heure certains parametres
- probleme de valeur de l'hydraulic heater : 10% et non 100%
- ne pas armer le lock si pas de modification sur les heaters

- long press pour reset sechoir

- implementer support double coeur

- permettre de gérer la strategy de chauffe par la config
  - on ajoute les heaters
  - attention particulière : ça impacte le display
- ajouter relay pour deshumidificateur
  - ajouter en option
- test coverage
- ajouter scenario de tests pour s'assurer que ça fonctionne bien
- ajouter test device : pour s'assurer que toutes les fonctionnalités sont OK
- find a better font