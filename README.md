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

- s'assurer que le global time fonctionne
- s'assurer que les barres de progression fonctionne
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