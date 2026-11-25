# 22.5 — Réduction de la pression mémoire

Le firmware précédent a validé le transfert signé, mais le client historique a reçu HTTP 503 `memory` en mettant à jour le web. Le parseur du manifeste réservait 4096 octets en plus du tampon multipart. Le parseur v1 spécialisé conserve les octets, contrôle chaque champ, limite les tailles et rejette les clés inconnues ou répétées. Aucun changement du format ni de la signature firmware. Les codes de détail du manifeste invalide peuvent changer (HTTP 400 inchangé).

Le catalogue usine est traité entrée par entrée. Les requêtes locales normales passent par une file d'attente côté navigateur; les requêtes périodiques de statut et de nuit sont suspendues pendant la maintenance. Le transfert web utilise 512 octets. La vieille interface ne dispose pas de ces optimisations, donc installer le nouveau firmware de développement par USB, puis tester le manifeste et charger le nouveau web via le script A/B avant de publier. Ne pas utiliser uploadfs.

Les releases 3.0.3-dev et 3.0.4-dev déjà publiées ne sont pas modifiées. 3.0.5-dev reste locale avant validation. Les tests d'interruption physique et l'audit de sécurité restent ouverts.
