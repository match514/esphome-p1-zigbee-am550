# ESPHome P1 → Zigbee (AM550, ESP32-H2)

Lit le port P1 d'un compteur Iskraemeco AM550 (testé chez Romande Énergie,
Suisse) avec un ESP32-H2 et envoie les mesures par Zigbee vers Zigbee2MQTT.
Aucune connexion Wi-Fi ni clé de chiffrement n'est nécessaire (port P1 non chiffré).

## Fonctions
- Décodage HDLC/DLMS en C++ (découpage par longueur, contrôle FCS, assemblage
  des segments)
- Puissance active, énergie totale, tarif 1 (HP) et tarif 2 (HC)
- Tensions et courants des 3 phases
- Zigbee (profil Home Automation), compatible Zigbee2MQTT ≥ 2.8.0

## Fichiers
- `compteur-p1.yaml` : configuration ESPHome
- `p1_decoder.h` : décodeur DLMS (à placer à côté du YAML)

## Câblage (RJ12 compteur → ESP32-H2)
| Pin RJ12 | Rôle | Raccordement |
|---|---|---|
| 1 | +5 V | pin 2, et alimentation de la carte |
| 2 | Data Request | pin 1 |
| 3 et 6 | GND | GND de la carte |
| 5 | Data | GPIO4 + résistance 10 kΩ vers 3V3 |

Ne jamais relier le 5 V du compteur et celui de l'USB en même temps.

## Remarques
- Signal P1 inversé : `inverted: true` sur l'UART.
- Les entités apparaissent comme « Analog Input » dans Z2M ; pour le tableau de
  bord Énergie de Home Assistant, il faut un convertisseur Z2M ou des capteurs modèles.
- Après un changement de la liste des capteurs, retirer et réappairer l'appareil.

Licence : MIT
