# ESPHome P1 → Zigbee pour compteur AM550 (ESP32-H2)

Lecteur du port P1 d'un compteur **Iskraemeco AM550** (testé chez Romande Énergie, Suisse) avec un **ESP32-H2**. Les mesures sont envoyées par **Zigbee** vers **Zigbee2MQTT**, sans Wi-Fi et sans clé de chiffrement (port P1 non chiffré).

Mesures : puissance active, énergie totale, énergie tarif 1 (HP) et tarif 2 (HC), tensions et courants des 3 phases.

> Projet de bricolage, testé sur un seul compteur. Aucune garantie. Ne touche jamais à l'installation électrique : le port P1 ne demande aucune intervention sur le compteur, seulement un câble RJ12 enfiché.

---

## 1. Matériel à acheter

| Article | Remarque |
|---|---|
| [Waveshare ESP32-H2 (carte de développement BLE/Zigbee/Thread)](https://www.digitec.ch/fr/s1/product/waveshare-carte-de-developpement-esp32-h2-avec-blezigbeethread-carte-de-developpement-kit-44700386) | Le H2 n'a pas de Wi-Fi : c'est voulu. |
| Câble **RJ12 (6P6C)** | Attention : un câble RJ11 est souvent câblé sur 2 ou 4 contacts seulement. Il faut les 6 contacts, ou au moins les pins 1, 2, 3, 5 et 6. |
| Résistance **10 kΩ** | Pull-up de la ligne Data. |
| Fils (type Dupont) et fer à souder | Pour relier le câble RJ12 à la carte. Un connecteur RJ12 à vis (breakout) évite la soudure. |
| Câble USB-C | Pour flasher la carte. |

## 2. Prérequis logiciels

- Home Assistant avec l'add-on **ESPHome Device Builder**
- **Zigbee2MQTT ≥ 2.8.0** (nécessaire pour le composant Zigbee d'ESPHome)
- Navigateur Chrome ou Edge (le flash USB passe par WebSerial)

## 3. Câblage

Pinout du port P1 (RJ12) :

| Pin RJ12 | Rôle | Raccordement |
|---|---|---|
| 1 | +5 V | pin 2 ; plus tard, 5 V de la carte |
| 2 | Data Request | pin 1 |
| 3 | GND | GND de la carte |
| 5 | Data | **GPIO4** de la carte, et résistance 10 kΩ vers **3V3** |
| 6 | GND alim | GND de la carte (jamais à la pin 5) |

```
pin 5 (Data) ───────┬──────── GPIO4
                    │
                10 kΩ
                    │
                  3V3
```

Points critiques :
- La résistance est une **pull-up** : une patte sur la ligne Data, l'autre sur le **3V3** (pas en série, pas sur le 5 V).
- La pin 6 est une masse : ne la relie pas à la pin 5.
- Ne branche **jamais** le 5 V du compteur et celui de l'USB en même temps. Pendant le flash, laisse la pin 1 débranchée de la carte.
- Les pins 1 et 2 doivent être reliées côté compteur : sans Data Request, le compteur n'envoie rien.

## 4. Installation avec ESPHome Device Builder

1. Dans Home Assistant, ouvre **ESPHome Device Builder** et crée un nouvel appareil (nom : `compteur-p1`, ESP32-H2).
2. Copie `p1_decoder.h` dans le dossier de configuration ESPHome (`/config/esphome/`), à côté du fichier YAML.
3. Remplace le contenu du YAML par celui de `compteur-p1.yaml`.
4. Branche la carte en USB (avec la pin 1 débranchée), clique sur **Install**, puis **Manual download / Plug into this computer**, et choisis le port série dans le navigateur.
5. Attends la fin du flash.

## 5. Appairage avec Zigbee2MQTT

1. Dans Z2M, active **Autoriser l'appairage** (Permit join).
2. Redémarre la carte (bouton reset). Elle rejoint le réseau et apparaît comme « Compteur P1 ».
3. Z2M affiche « NOT supported » : c'est normal, les entités sont exposées en « Analog Input ». L'erreur genOnOff au configure est sans conséquence.
4. Attends environ 1 minute : les 10 capteurs se remplissent avec les vraies valeurs.

Si tu changes la liste des capteurs : retire l'appareil de Z2M (« Force remove »), puis réappaire-le. Si la carte ne se réappaire pas, efface-la :

```
esptool --port COM5 erase-flash
```

(à adapter au port), puis reflashe.

## 6. Dépannage

| Symptôme | Cause probable |
|---|---|
| Valeurs N/A dans Z2M | Ajouter `report: force` sur chaque capteur du YAML |
| Capteur absent dans Z2M | Ajouter une `unit_of_measurement` ; réappairer l'appareil |
| Quelques octets reçus, jamais de trame | Pull-up absente ou mal branchée ; vérifier qu'elle va au 3V3 |
| Rien ne monte | Data Request (pin 2) non reliée à la pin 1 ; GND manquant |
| Erreur NaN dans les logs Z2M | Normal tant qu'aucun message complet n'a été décodé |
| Erreur de compilation « g_bytes not a member » | Ancienne version de `p1_decoder.h` dans `/config/esphome/` |

Pour diagnostiquer, tu peux temporairement ajouter deux capteurs (octets reçus, trames valides) qui lisent `p1::g_bytes` et `p1::g_frames_ok`.

## 7. Notes techniques

- Le signal P1 est inversé : `inverted: true` sur la broche RX de l'UART.
- UART 115 200 bauds, 8N1.
- Le décodeur découpe les trames HDLC par **longueur** (pas par 0x7E : une donnée peut valoir 0x7E), vérifie le FCS (CRC-16/X25) et assemble les segments avant de publier.
- Échelles : tensions ×0,1 V, courants ×0,01 A, énergies en Wh (÷1000 pour kWh).
- Les entités sont des « Analog Input » génériques : pour le tableau de bord Énergie de Home Assistant, il faudra probablement un convertisseur externe Z2M ou des capteurs modèles.

## 8. Fichiers

- `compteur-p1.yaml` : configuration ESPHome
- `p1_decoder.h` : décodeur DLMS/HDLC

## Licence

MIT
