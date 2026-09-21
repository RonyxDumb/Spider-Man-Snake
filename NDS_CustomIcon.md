# NDS_CustomIcon

Guida per impostare un'icona personalizzata nella ROM Nintendo DS di questo progetto.

La soluzione utilizzata in questo port evita la conversione tramite GRIT/GRF per il banner e passa direttamente a `ndstool` un **BMP indicizzato 32×32**.

---

## Formato richiesto

L'icona finale deve essere:

```text
32×32 pixel
BMP indicizzato
16 indici di palette totali
15 colori visibili
indice 0 = trasparenza
```
---

## 1. Preparare la nuova immagine

Copiare la propria immagine personalizzata in:

```text
projectNDS/icon.png
```

Può essere anche più grande di 32×32.

È consigliato utilizzare:

- soggetto semplice;
- contrasto elevato;
- pochi dettagli;
- sfondo trasparente;
- soggetto ben centrato.

Le icone Nintendo DS sono molto piccole, quindi dettagli troppo fini verranno persi.

---

## 2. Creare una versione 32×32

Da WSL / Ubuntu, entrare nella root del progetto:

```bash
cd /mnt/c/Users/pipin/Desktop/projectNDS
```

Creare una versione 32×32 mantenendo le proporzioni:

```bash
magick icon.png \
  -filter Lanczos \
  -resize 32x32 \
  -background none \
  -gravity center \
  -extent 32x32 \
  icon_32.png
```

Verificare:

```bash
magick icon_32.png
```

Deve apparire:

```text
32x32
```

---

## 3. Convertire in BMP indicizzato Nintendo DS

È necessario creare un BMP con:

- palette indicizzata;
- indice 0 usato per la trasparenza;
- massimo 15 colori visibili.

Usare Python + Pillow:

```bash
python3 - <<'PY'
from PIL import Image

SRC = "icon_32.png"
DST = "icon.bmp"

img = Image.open(SRC).convert("RGBA")
alpha = img.getchannel("A")

rgb = Image.new("RGB", (32, 32), (255, 255, 255))
rgb.paste(img.convert("RGB"), mask=alpha)

q = rgb.quantize(
    colors=15,
    method=Image.Quantize.MEDIANCUT,
    dither=Image.Dither.FLOYDSTEINBERG
)

out = Image.new("P", (32, 32), 0)

qpal = q.getpalette()

# Palette index 0: trasparenza.
# Il magenta può comparire nei visualizzatori BMP,
# ma nel menu DS l'indice 0 viene trattato come trasparente.
palette = [255, 0, 255]
palette += qpal[:15 * 3]
palette += [0] * (768 - len(palette))

out.putpalette(palette)

qpx = q.load()
apx = alpha.load()
opx = out.load()

for y in range(32):
    for x in range(32):
        if apx[x, y] < 128:
            opx[x, y] = 0
        else:
            opx[x, y] = qpx[x, y] + 1

out.save(DST, format="BMP")

used = sorted(set(out.getdata()))

print("Creato:", DST)
print("Dimensione:", out.size)
print("Indici usati:", used)
PY
```

Se Pillow non è installato:

```bash
sudo apt install python3-pil
```

---

### Lo sfondo magenta è normale

Aprendo il BMP su Windows può apparire uno sfondo magenta.

Non è un errore.

Il magenta è semplicemente il colore assegnato alla **palette index 0**.

Nel banner Nintendo DS:

```text
index 0 = trasparenza
```

quindi il magenta non dovrebbe essere visibile nel menu della console.

---

## 5. Verificare gli indici della palette

Usare:

```bash
python3 - <<'PY'
from PIL import Image

im = Image.open("src/nds/gfx/icon.bmp").convert("P")

indices = sorted(set(im.getdata()))

print("Indici utilizzati:", indices)
print("Numero indici:", len(indices))
print("Pixel trasparenti/index 0:", list(im.getdata()).count(0))
PY
```

Il risultato deve utilizzare solo valori compresi tra:

```text
0 ... 15
```

Non devono esserci più di 16 indici.

---

## 6. Configurazione del Makefile

Il Makefile deve utilizzare direttamente il BMP.

La configurazione deve contenere:

```make
NDS_ICON_SRC := src/nds/gfx/icon.png
NDS_ICON     := src/nds/gfx/icon.bmp
```

La lista delle normali texture PNG deve continuare a escludere `icon.png`:

```make
PNG_FILES := $(filter-out $(NDS_ICON_SRC),$(foreach dir,$(GFX_DIRS),$(wildcard $(dir)/*.png)))
```

---

## Note sulla qualità

Il banner Nintendo DS utilizza una risoluzione molto ridotta.

Per ottenere un buon risultato:

- evitare immagini fotografiche troppo complesse;
- usare un soggetto grande;
- evitare testo molto piccolo;
- evitare dettagli sottili;
- mantenere il volto o il logo al centro;
- preferire colori molto distinguibili;
- verificare il risultato su hardware reale.

Un'immagine che appare molto dettagliata ad alta risoluzione può risultare molto meno leggibile una volta ridotta a 32×32.
