# Dépannage

[Retour au mode d'emploi](user-guide.md)

| Symptôme | Vérifications et actions |
|---|---|
| L'adresse `wordclock.local` ne répond pas | Vérifier que les appareils sont sur le même réseau, désactiver temporairement un VPN si nécessaire, essayer l'IP locale. |
| Réseau Wi-Fi indisponible | Rejoindre `Wordclock-…`, ouvrir `http://192.168.4.1/`. Le point d'accès peut apparaître après environ 30 secondes avec un réseau enregistré mais inaccessible. |
| Heure incorrecte | Vérifier le fuseau horaire et synchroniser depuis le téléphone ou Internet. |
| Horloge éteinte | Vérifier alimentation, commande d'arrêt, luminosité et mode nuit ; ne pas ouvrir un appareil sous tension. |
| Halo éteint | Vérifier la luminosité arrière, le thème et le mode nuit. |
| Ambiance différente du thème | Vérifier l'affichage effectif, les programmations, la météo et le mode nuit. |
| Météo non actualisée | Vérifier Internet, coordonnées et état météo ; des données précédemment valides peuvent rester affichées. |
| Mise à jour interrompue | Ne pas débrancher pendant l'activation ; vérifier les versions et le diagnostic avant toute nouvelle tentative. |
| Interface web inutilisable | Essayer la page locale `/recovery`, puis contacter le mainteneur. |
| Mot de passe administrateur oublié | Contacter le mainteneur ; ne pas tenter une réinitialisation destructive sans procédure validée. |

## Signaler un problème

Dans **Réglages → Système → Informations système**, utiliser **Copier le diagnostic**. Avant de le partager, supprimer les informations personnelles, adresses réseau et identifiants éventuels. Indiquer le symptôme, les versions, les étapes de reproduction et les modes actifs. Le [suivi des problèmes GitHub](https://github.com/ccachin/Wordclock-2.0/issues) est accessible publiquement.
