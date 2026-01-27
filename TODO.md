TODO:

corriger les states // sauvegardes PPROM
migrer les logger + document comment lancer avec un niveau de debug

UI
- changer la font par une plus lisible
- caractere pour degres
- améliorer le menu : ajouter des unités + mettre en heure certains parametres
- rajouter splashscreen
- rajouter demarrage UI : sd card, rtc, ... pour visuellement checker que c'est ok
- ajouter visuellement tmax + hrmax (en fonction du programme et de l'override)
- rajouter icon IN et OUT pour les valeurs du sensor

- rajouter la gestion du renouvellement d'air dans le heater manager si trop chaud
- ajouter params cycle duration configurable
- mode eco / hybride / perf
  - eco : utilise hydro seulement, si pas assez d'énergie, objectifs réduits pour préserver l'hydro
  - hybride : utilise solaire, puis elect en réduisant les objectifs
  - perf : target les objectifs
  - afficher a l'écran le mode
  - pouvoir le changer dans la config
- créer documentation
- ajouter BOM
- ajouter schematics

Features next:

- rajouter menu systeme pour regler l'heure
- implementer support double coeur : logging sd card + save to epprom
  => mettre en place une queue qui
- option pour le circulateur dans les settings
  - min / max / frequence...
- permettre de gérer la strategy de chauffe par la config
  - on ajoute les heaters
  - attention particulière : ça impacte le display
- ajouter relay pour deshumidificateur
  - ajouter en option
- access point pour récuperer les données
- mode pour historique => récupérer les sessions de séchage

- coding style
- test coverage
- ajouter scenario de tests pour s'assurer que ça fonctionne bien
- ajouter test device : pour s'assurer que toutes les fonctionnalités sont OK
