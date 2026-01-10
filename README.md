# Dryer

```
pio run

# run and upload to pico
pio run -t upload -t monitor -e pico_w

# test on local machine
pio test -vvv -e native
```

## generate fonts

https://rop.nl/truetype2gfx/

## generate icons

TODO:

- ajouter visuellement tmax + hrmax

- ajouter taux d'hygrométrie max
  l'idée c'est de definir un taux maximum d'hygrométrie
  attention, il est possible de ne pas arriver à l'atteindre
  est-ce qu'il est décroissant ?

- rajouter la gestion du renouvellement d'air dans le heater manager si trop chaud
- gérer overshoot (anticiper dépassement de température)
- implementer support double coeur

- ajouter params cycle curation configurable
- améliorer le menu : ajouter des unités + mettre en heure certains parametres

- mode eco / hybride / perf
  - afficher a l'écran le mode
  - pouvoir le changer dans la config

- option pour le circulateur
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