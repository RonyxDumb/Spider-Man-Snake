# ==============================================================================
# Spider-Man Snake - Nintendo DS
# Toolchain: devkitPro / devkitARM / libnds / Maxmod
# Tutti gli artefatti intermedi e finali vengono generati nella cartella build/
# ==============================================================================

.SUFFIXES:

# ------------------------------------------------------------------------------
# Verifica delle variabili d'ambiente essenziali di devkitPro
# ------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error La variabile d'ambiente DEVKITPRO non e' impostata)
endif

ifeq ($(strip $(DEVKITARM)),)
$(error La variabile d'ambiente DEVKITARM non e' impostata)
endif

ifeq ($(strip $(CALICO)),)
CALICO := $(DEVKITPRO)/calico
endif

# Inclusione delle regole standard per Nintendo DS fornite da devkitARM
include $(DEVKITARM)/ds_rules

# ------------------------------------------------------------------------------
# Configurazione percorsi e nomi target
# ------------------------------------------------------------------------------
TARGET_NAME := spiderman-snake
BUILD       := build
SOURCES     := source
INCLUDES    := include
GRAPHICS    := gfx
NITRO       := $(BUILD)/nitro

# ------------------------------------------------------------------------------
# Informazioni ROM e Banner Nintendo DS (3 stringhe separate)
# ------------------------------------------------------------------------------
ROM_TITLE    := SPIDER-MAN # Titolo ROM
ROM_SUBTITLE := Snake standalone # Sottotitolo
ROM_AUTHOR   := By Francesco Pio Pipino # Autore

ROM_BANNER_INFO := "$(ROM_TITLE);$(ROM_SUBTITLE);$(ROM_AUTHOR)"

# Flag di compilazione per l'architettura ARM9 (processore principale del Nintendo DS)
ARCH := -march=armv5te -mtune=arm946e-s -mthumb

# Specifiche Calico per la gestione della memoria e degli interrupt dell'ARM9
SPECS := -specs=$(CALICO)/share/ds9.specs

# Parametri del compilatore C
CFLAGS := $(SPECS) -g -O2 -Wall -Wextra -ffunction-sections -fdata-sections \
	$(ARCH) -DARM9 -D__CALICO_ARM9__ -D__NDS__ \
	-I$(CALICO)/include \
	-I$(DEVKITPRO)/libnds/include \
	-I$(CURDIR)/include \
	-I$(CURDIR)/$(BUILD)

# Parametri del linker
LDFLAGS := $(SPECS) -g $(ARCH) \
	-Wl,-Map,$(BUILD)/$(TARGET_NAME).map \
	-L$(DEVKITPRO)/libnds/lib \
	-L$(CALICO)/lib

# Librerie di sistema collegate (audio Maxmod, filesystem, libnds base e runtime Calico)
LIBS := -lmm9 -lfilesystem -lfat -lnds9 -lcalico_ds9

# Scansione automatica dei sorgenti C e calcolo degli oggetti intermedi
CFILES := $(wildcard $(SOURCES)/*.c)
OFILES := $(patsubst $(SOURCES)/%.c,$(BUILD)/%.o,$(CFILES))
DEPS   := $(OFILES:.o=.d)

# File RAW lineari e oggetti compilati per lo sfondo principale
BACKGROUND_RAW := $(BUILD)/background.raw
BACKGROUND_O   := $(BUILD)/background_bin.o

# File RAW lineari e oggetti compilati per lo sfondo dell'intro
INTRO_BG_RAW   := $(BUILD)/intro_background.raw
INTRO_BG_O     := $(BUILD)/intro_background_bin.o

# File per l'icona a 16 colori visualizzata nel menu di sistema del DS
ICON_SRC   := icon.bmp
ICON_FIXED := $(BUILD)/icon.bmp

# Tracce audio in formato MP3 e rispettivi flussi PCM per NitroFS
MUSIC_MP3 := data/music.mp3
MUSIC_PCM := $(NITRO)/music.pcm

INTRO_MP3 := data/intro.mp3
INTRO_PCM := $(NITRO)/intro.pcm

# Destinazioni finali del binario ELF e della ROM NDS
TARGET_ELF := $(BUILD)/$(TARGET_NAME).elf
TARGET_NDS := $(BUILD)/$(TARGET_NAME).nds

# ------------------------------------------------------------------------------
# Regole di Compilazione
# ------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET_NDS)

# Creazione delle directory interne per isolare la build dalla root
$(BUILD):
	mkdir -p "$@"

$(NITRO):
	mkdir -p "$@"

# Conversione dell'immagine di sfondo di gioco a 256x192 in RAW 15-bit RGB555
$(BACKGROUND_RAW): gfx/background.png | $(BUILD)
	@echo "  CONVERSIONE SFONDO GIOCO RAW (256x192): $< -> $@"
	ffmpeg -y -i "$<" -vf "scale=256:192" -f rawvideo -pix_fmt bgr555le "$@"

# Generazione dell'oggetto ELF linkabile dal file binario dello sfondo di gioco
$(BACKGROUND_O): $(BACKGROUND_RAW)
	@echo "  OBJCOPY SFONDO GIOCO: $< -> $@"
	arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata "$<" "$@"

# Conversione dell'immagine di sfondo dell'intro a 256x192 in RAW 15-bit RGB555
$(INTRO_BG_RAW): gfx/intro_background.png | $(BUILD)
	@echo "  CONVERSIONE SFONDO INTRO RAW (256x192): $< -> $@"
	ffmpeg -y -i "$<" -vf "scale=256:192" -f rawvideo -pix_fmt bgr555le "$@"

# Generazione dell'oggetto ELF linkabile dal file binario dello sfondo dell'intro
$(INTRO_BG_O): $(INTRO_BG_RAW)
	@echo "  OBJCOPY SFONDO INTRO: $< -> $@"
	arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata "$<" "$@"

# Quantizzazione e ridimensionamento dell'icona a 32x32 indicizzata a 16 colori
$(ICON_FIXED): $(ICON_SRC) | $(BUILD)
	@echo "  CONVERSIONE ICONA: $< -> $@"
	ffmpeg -y -i "$<" -filter_complex "[0:v]scale=32:32,split[s0][s1];[s0]palettegen=max_colors=16[p];[s1][p]paletteuse" -pix_fmt pal8 "$@"

# Compilazione di ciascun file C con generazione delle dipendenze automatiche
$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c "$<" -o "$@"

# Conversione della musica di gioco in PCM grezzo a 22050Hz stereo 16-bit
$(MUSIC_PCM): $(MUSIC_MP3) | $(NITRO)
	@echo "  CONVERSIONE AUDIO IN-GAME: $< -> $@"
	ffmpeg -y -i "$<" -ar 22050 -ac 2 -f s16le "$@"

# Conversione della musica dell'intro in PCM grezzo a 22050Hz stereo 16-bit
$(INTRO_PCM): $(INTRO_MP3) | $(NITRO)
	@echo "  CONVERSIONE AUDIO INTRO: $< -> $@"
	ffmpeg -y -i "$<" -ar 22050 -ac 2 -f s16le "$@"

# Collegamento dell'eseguibile ELF contenente codice, grafica e librerie
$(TARGET_ELF): $(BACKGROUND_O) $(INTRO_BG_O) $(OFILES) $(MUSIC_PCM) $(INTRO_PCM)
	$(CC) $(LDFLAGS) $(BACKGROUND_O) $(INTRO_BG_O) $(OFILES) $(LIBS) -o "$@"

# Creazione della ROM .nds finale tramite ndstool con banner e filesystem virtuale
$(TARGET_NDS): $(TARGET_ELF) $(MUSIC_PCM) $(INTRO_PCM) $(ICON_FIXED)
	ndstool -c "$@" \
		-9 "$(TARGET_ELF)" \
		-7 "$(CALICO)/bin/ds7_maine.elf" \
		-b "$(ICON_FIXED)" \
		$(ROM_BANNER_INFO) \
		-d "$(NITRO)"

# Pulizia di tutti gli artefatti generati
clean:
	rm -rf "$(BUILD)"

-include $(DEPS)