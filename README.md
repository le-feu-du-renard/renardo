# Dryer

```
pio run

# run and upload to pico
pio run -t uploadfs -e pico_w
pio run -t upload -t monitor -e pico_w
pio run -t upload -t monitor -e test_hydraulic
pio run -t upload -t monitor -e pico_w

# test on local machine
pio test -vvv -e native
```

## generate fonts

https://rop.nl/truetype2gfx/

## generate icons

TODO:

- ajouter visuellement tmax + hrmax + 1-1 + eco/hybride/perf
- rajouter icon IN et OUT pour les valeurs du sensor
- rajouter la gestion du renouvellement d'air dans le heater manager si trop chaud
- ajouter params cycle curation configurable
- améliorer le menu : ajouter des unités + mettre en heure certains parametres
- changer la font par une plus lisible
- mode eco / hybride / perf
  - eco : utilise hydro seulement, si pas assez d'énergie, objectifs réduits pour préserver l'hydro
  - hybride : utilise solaire, puis elect en réduisant les objectifs
  - perf : target les objectifs
  - afficher a l'écran le mode
  - pouvoir le changer dans la config
- créer documentation

Features next:

- implementer support double coeur

- option pour le circulateur dans les settings
  - min / max / frequence...

- permettre de gérer la strategy de chauffe par la config
  - on ajoute les heaters
  - attention particulière : ça impacte le display
- ajouter relay pour deshumidificateur
  - ajouter en option

- mode pour historique => récupérer les sessions de séchage

- coding style
- test coverage
- ajouter scenario de tests pour s'assurer que ça fonctionne bien
- ajouter test device : pour s'assurer que toutes les fonctionnalités sont OK
- find a better font