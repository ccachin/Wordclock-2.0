## 3.6.1-dev
- Validate bounded weather HTTP body framing before JSON decoding, distinguish incomplete bodies and decoding failures.
- Release TCP resources before heap-backed filtered JSON parsing; restore read-only input and simplify serial diagnostics.
- One accelerated retry on incomplete responses or timeout; preserve last valid data and update protections.

## 3.6.0-dev
- Move Wi-Fi portal styling and factory theme catalogue to flash; allocate portal HTML only during setup.
- Resolve built-in themes directly, with unchanged IDs, colours, speeds and JSON catalogue; skip hidden personal themes when loading the embedded UI.
- Reduce web verification buffer, reserve JSON response size and avoid temporary gzip header tokens.
- Reuse weather request/header buffer and parse bounded JSON without copying strings.
- Show sampled memory minima and indicative update memory margin in system information.
- Restore opt-in USB weather diagnostics; preserve all signing, transfer and storage formats.
- Add guarded legacy-header cleanup and a repeatable memory report.

## 3.5.2-dev

- Harmonisation des contrôles météo et aperçu de la véritable grille de lettres, avec couleur de façade et mélange de thème.
- Diagnostic ArduinoJson détaillé, lecture filtrée en entrée constante et compatibilité WiFiClient ESP8266 3.1.2.
- Distinction entre anciennes données valides et échec de la dernière actualisation.

## 3.5.1-dev

- Recherche de localité HTTPS depuis le navigateur, coordonnées automatiques et saisie manuelle de secours.
- Client météo HTTP/1.0 borné, JSON filtré et diagnostics détaillés.

## 3.5.0-dev

- Experimental autonomous HTTP weather mode, disabled by default.
- Current conditions or near-hour forecast, bounded response and stale fallback.
- Local weather settings and indicative preview.

## 3.4.2-dev

- Descriptions des menus, catalogue limité aux thèmes usine et aperçus animés indicatifs.
- Programmation par cartes, ajout, modification et suppression, sans import/export. Firmware 3.4.1-dev inchangé.

## 3.4.1-dev

- Réglages en six sections, aperçu local animé, états de sauvegarde confirmés, annulation des brouillons.
- Dissolution, diagonale, scanner, étincelles et onde circulaire ; fondu et enveloppes adoucis.

## 3.4.0-dev

- Bibliothèque locale simplifiée : sélection des thèmes uniquement, sans import/export ni menu Options.

- Automatisations locales : 16 règles, plages horaires, jours et dates, thèmes usine, plafond de luminosité, animations ponctuelles.
- Import/export JSON, stockage séparé avec contrôle d’intégrité et récupération.
- Priorité au mode nuit et à l’arrêt manuel ; pas de service météo ni solaire dans cette version.

## 3.3.0-dev

- Animations de façade : fondu, balayage, Matrix, apparition progressive, explosion, coucou lumineux.
- Arc-en-ciel et chenillard sur les lettres, palette horaire, réglages persistants et test ponctuel.
- Effets calculés à la volée, sans nouveau tableau d’images ; nouvelles options désactivées par défaut.
- Tests natifs du moteur et contrôle HTTP de la nouvelle API. Validation matérielle requise.

## 3.2.0-dev

- Offline access point and local clock control.
- Phone timestamp synchronization and manual local date/time.
- NTP source indication and existing timezone/DST rules retained.
- HTTP validation includes clock and Wi-Fi endpoints.

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
