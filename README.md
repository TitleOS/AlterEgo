# AlterEgo (WIP)

**Flipper Zero FAP — Amiibo AI Editor for Super Smash Bros. Ultimate & Smash Bros 4 Wii U & 3DS**

Reads an NTAG215 amiibo tag (or a `.nfc` dump file), decrypts the Figure Player
data, lets you tune the AI stats (Level, Attack, Defense, Speed, Persona) and
fine-tune the 64-entry move priority table, then re-encrypts and saves the result.

---

## Requirements

| Requirement | Notes |
|---|---|
| **Flipper firmware** | [Momentum](https://github.com/Next-Flip/Momentum-Firmware) (tested) OFW Firmware has not been tested |
| **`key_retail.bin`** | Place at `/ext/amiibo/key_retail.bin` on your SD card |
| **Amiibo** | Super Smash Bros. **Ultimate** (Switch) & **Smash Bros 4 Wii U & 3DS** Amiibo figures only |
| **Build tool** | [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) |

> `key_retail.bin` is **not included** — it contains Nintendo's proprietary AES
> master keys. Obtain it separately; its MD5 should be `75 54 c3 21 1e 03 f4 ...`
> (commonly known in the community).  The app checks for it at startup and shows
> a clear error if missing. 
> Drop key_retail.bin at `/ext/amiibo/key_retail.bin` on your SD card.

---

## Setup

```bash
# Install ufbt (Python 3.8+)
pip install ufbt

# Update SDK to Momentum firmware channel
ufbt update --index-url https://up.momentum-fw.dev/firmware/directory.json

# Clone / enter project directory
cd AlterEgo

# Build
ufbt build

# Deploy to connected Flipper
ufbt launch

OR 

Using qFlipper

1. Open qFlipper
2. Open Explorer to AlterEgo/dist/
3. Select the alter_ego.fap file
4. Drag and drop the alter_ego.fap file to the Apps/NFC/ folder on your Flipper
```

The compiled `.fap` appears in `dist/alter_ego.fap` and is automatically
installed to `/ext/apps/NFC/` on your Flipper if using ufbt launch.

---

## Usage

1. **Scan Amiibo** — hold the amiibo figure against the Flipper's NFC area  
   *or*  
   **Open .nfc File** — pick an existing dump from `/ext/nfc/`

2. **Edit Stats** screen:
   | Field | Range |
   |---|---|
   | Level | 1 – 50 |
   | Attack | –5000 to +5000 |
   | Defense | –5000 to +5000 |
   | Speed | –5000 to +5000 |
   | Persona | Normal / Aggressive / Calm / Careless / Cautious / Tricky / Joyful |

3. **Move Weights** — scroll the 64-entry table, press **Left/Right** to adjust
   by ±50, press **OK** to zero-out the selected move. **CURRENTLY STILL WIP**

4. **Save** — choose **Save .nfc** to write the re-encrypted file to  
   `/ext/amiibo/alterego_<UID>.nfc`, then use the Flipper stock NFC app
   to physically write it to a blank NTAG215 tag or back to the original amiibo.

---

## Project Layout

```
AlterEgo/
├── application.fam          # FAP manifest
├── alter_ego_app.h/.c       # Entry point, ViewDispatcher, SceneManager
├── amiibo_crypto.h/.c       # AES-CTR decrypt + HMAC-SHA256 sign/verify
├── amiibo_smash.h/.c        # SSBU Figure Player parse / serialize
├── nfc_helper.h/.c          # .nfc file I/O, NFC scan wrapper
├── scenes/
│   ├── alter_ego_scene_main_menu.c
│   ├── alter_ego_scene_reading.c   # NFC scan (worker thread)
│   ├── alter_ego_scene_editor.c    # Level / stat editor
│   ├── alter_ego_scene_weights.c   # Move priority table
│   ├── alter_ego_scene_save.c      # Save to SD / NFC write
│   └── alter_ego_scene_error.c     # Error display
└── images/
    └── alterego_10x10.png   # App icon
```

---

## Crypto Notes

AlterEgo uses **mbedTLS** (bundled in Flipper firmware) for:

- **Key derivation** — HMAC-SHA256 DRBG expansion from `key_retail.bin`
  following the amiitool keygen algorithm
- **AES-128-CTR** — decrypts/re-encrypts the 40-byte header section
- **HMAC-SHA256** — verifies and recomputes the tag/data integrity hashes

The game-specific SSBU Figure Player block (`AMIIBO_OFF_GAME_DATA = 0xDC`) is
**plaintext** (not encrypted by AES-CTR); only the 40-byte header is in the
encrypted region.

---

## Known Limitations

| Item | Status |
|---|---|
| Direct NFC write | ⏳ Not yet (save to SD and use stock NFC app) |
| Smash 4 (Wii U/3DS) | Partially Supported (Stats & Personality Only) |
| Spirit slot editing | ⏳ Reads bytes, editing TBD |
| Character rename | ❌ Stored in encrypted header |

---

## Disclaimer

Modifying amiibo data is for personal use with your own figures.  
AlterEgo does not include any Nintendo proprietary keys or data.
Do not use modified Amiibos in online play, don't be a dick.  
Use responsibly.
