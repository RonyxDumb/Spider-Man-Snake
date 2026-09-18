# Spider-Man Snake Standalone (Nintendo DS)

Un homebrew game per Nintendo DS sviluppato in C nativo tramite la toolchain **devkitPro / devkitARM**, **libnds**, **Calico** e il sottosistema audio **Maxmod**.

Il gioco reinterpreta il classico gameplay di Snake a tema Spider-Man, sfruttando le caratteristiche hardware del Nintendo DS attraverso dissolvenze a livello di registri grafici, gestione a doppio schermo priva di conflitti VRAM, streaming audio via NitroFS e interfaccia testuale allineata su griglia fissa.

---

## Caratteristiche Principali

- **Schermo Superiore (Main 2D Engine - Mode 5)**:
  - Framebuffer bitmap 16-bit RGB555 nativo ($256 \times 192$).
  - Immagine introduttiva a piena brillantezza (100%).
  - Sfondo in-game scurito al 40% per far risaltare gli sprite bianchi ad alto contrasto.
  - Effetto oscuramento al 12% (dimming al ~70%) durante la pausa e al Game Over.
  - Sprite hardware (OAM) a 4bpp ($16 \times 16$ pixel) per corpo dello Snake e preda.
- **Schermo Inferiore (Sub 2D Engine - Mode 0)**:
  - Console testuale standard ad alta stabilità, priva di interferenze o glitch a righe verticali.
  - Layout calibrato rigorosamente entro il limite hardware di 32 colonne.
  - Box in stile rétro per schermata introduttiva, HUD di gioco, menu di pausa e selezione interattiva post-sconfitta.
- **Audio & Transizioni**:
  - Tracce audio dedicate (`intro.pcm` e `music.pcm`) in formato PCM a 16-bit / 22050 Hz stereo in streaming da NitroFS con riavvolgimento automatico.
  - Silenziamento istantaneo dell'audio durante la pausa e alla sconfitta.
  - Dissolvenze a nero hardware (`Fade Out` / `Fade In`) tra i cambi di stato.
- **Game Over Interattivo**:
  - Menu di scelta a fine partita: `[ SÌ ]` (ritorno all'intro con transizione e musica dedicata) o `[ NO ]` (riavvio immediato della partita).

---

## Struttura del Progetto

```text
spiderman-snake/
├── data/
│   ├── intro.mp3            # Traccia audio per la schermata iniziale (di "Spider-Man")
│   └── music.mp3            # Traccia audio per la partita (di "Spider-Man: Brand New Day")
├── gfx/
│   ├── background.png       # Sfondo di gioco (convertito a 256x192 RGB555 RAW)
│   └── intro_background.png # Sfondo della schermata iniziale (256x192 RGB555 RAW)
├── include/                 # Eventuali header file di supporto
├── source/
│   └── main.c               # Logica di gioco, gestione VRAM, audio e state machine
├── icon.bmp                 # Icona ROM (32x32 pixel a 16 colori)
├── Makefile                 # Script di compilazione devkitARM/ndstool
├── build_docker.bat         # Script di build su ambiente containerizzato
├── .gitignore
└── README.md
```

---

## Requisiti di Compilazione

Per compilare la ROM `.nds` sono richiesti:
- **devkitPro** con pacchetto **devkitARM**
- **libnds** e runtime **Calico**
- **ndstool**
- **FFmpeg** (utilizzato nel Makefile per convertire audio in PCM 16-bit e immagini in RAW lineari senza tiling grit)

---

## Istruzioni di Build

### Compilazione tramite ambiente Docker / Script batch
Se utilizzi il container Docker preconfigurato, avvia semplicemente:
```bat
build_docker.bat
```

### Compilazione manuale via Makefile
Assicurati che le variabili `DEVKITPRO`, `DEVKITARM` e `CALICO` siano configurate nel tuo ambiente, quindi esegui:
```bash
make clean
make
```

Al termine della compilazione, il file binario finale sarà generato in:
```text
build/spiderman-snake.nds
```

La ROM può essere testata direttamente su emulatori compatibili (come **melonDS** o **DeSmuME**) oppure eseguita su console reale Nintendo DS tramite flashcard compatibile (R4, DSTWO, twilightMenu++).

---

## Controlli di Gioco

| Tasto | Azione |
| :--- | :--- |
| **D-PAD (Frecce)** | Movimento dello Snake / Selezione opzione nel menu di Game Over |
| **START** | Avvia la partita dall'intro / Attiva o disattiva la pausa |
| **A** | Conferma scelta nel menu di Game Over (`[ SÌ ]` o `[ NO ]`) |

---

## 👤 Autore

**Francesco Pio Pipino**  
- Progetto: *Spider-Man Snake Standalone*  
- Piattaforma: Nintendo DS Homebrew
