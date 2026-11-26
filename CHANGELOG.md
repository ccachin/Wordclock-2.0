## 3.1.0-dev

- Interface locale allégée : bibliothèque et import/export JSON, Studio séparé.
- Ressources regroupées à la publication, cache versionné et diagnostic HTTP.
- Interrogations moins fréquentes, ArduinoOTA réservé au développement.
- Validation matérielle requise.

## 3.0.5-dev

- Analyse du manifeste web sans allocation dynamique.
- Catalogue des thèmes traité entrée par entrée et réduction des allocations.
- Sérialisation des requêtes locales et blocs web de 512 octets.
- Correctif de développement : validation matérielle requise.

## 3.0.4-dev

- Recherche indépendante des versions firmware et web.
- Installation du firmware signé depuis GitHub et reprise de l’autorisation en cas de mémoire insuffisante.
- Version web de développement : validation matérielle en cours.

## 3.0.3-dev

- Firmware signé RSA-2048 et transfert par blocs de 512 octets.
- Identifiants propres à chaque appareil, conservés lors des mises à jour.
- Vérification SHA-256 avant activation.
- Version de développement : tests d’interruption et installation automatique depuis GitHub encore à valider.

## 3.0.2-dev

- Vérification des octets des publications dans Git avant commit.
- Notes de publication repliables et état de mise à jour clarifié.

## 3.0.1-dev

- Section mises à jour : recherche, installation web et restauration.
- Installation manuelle du firmware local.
- Distribution web indépendante du firmware.

# Wordclock — Notes de version

## 3.0.0-dev — Développement

- Mise à jour du firmware via HTTP avec contrôle SHA-256.
- Mise à jour web sur deux emplacements, vérification et retour arrière.
- Page de secours indépendante des ressources web.
- Préparation locale des publications et identification des versions.

Version de développement : ne pas distribuer comme version stable.
