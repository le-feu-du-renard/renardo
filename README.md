# Dryer

```
pio run

# run and upload to pico
pio run -t upload -t monitor -e pico_w

# test on local machine
pio test -vvv -e native
```

TODO:
- faire fonctionner le systeme de base
  - faire fonctionner le display
    - values
    - font
    - image
  - faire fonctionner le menu
  - faire fonctionner le button start/stop

- permettre de gérer la strategy de chauffe par la config
  - on ajoute les heaters
  - attention particulière : ça impacte le display
- ajouter relay pour deshumidificateur
  - ajouter en option
- test coverage
- ajouter test pour les heaters
- ajouter test device : pour s'assurer que toutes les fonctionnalités sont OK