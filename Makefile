# ==============================================================================
# Spider-Man Snake - Nintendo DS
# Toolchain: devkitPro / devkitARM / libnds / Calico / Maxmod
# ==============================================================================
#
# Obiettivi principali di questo Makefile:
#   1. Compilare il codice ARM9 con il runtime Calico/libnds moderno.
#   2. Usare l'ARM7 standard di Calico (ds7_maine.elf), compatibile con i servizi
#      audio usati da libnds e Maxmod.
#   3. Convertire le due tracce MP3 in PCM stereo 16-bit / 22050 Hz.
#   4. Inserire i PCM in NitroFS, quindi dentro la ROM .nds finale.
#   5. Tenere tutti gli artefatti generati nella cartella build/.
#
# IMPORTANTE PER L'AUDIO:
# Il Makefile inserisce davvero intro.pcm e music.pcm nel NitroFS con l'opzione
# "ndstool -d". L'abilitazione dell'hardware audio, invece, viene fatta in
# main.c tramite soundEnable() prima dell'inizializzazione di Maxmod.
# ==============================================================================

.SUFFIXES:

# ------------------------------------------------------------------------------
# Verifica ambiente devkitPro
# ------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error La variabile DEVKITPRO non e' impostata)
endif

ifeq ($(strip $(DEVKITARM)),)
$(error La variabile DEVKITARM non e' impostata)
endif

# Le regole ufficiali impostano, tra le altre cose, CC e gli strumenti devkitARM.
include $(DEVKITARM)/ds_rules

# ------------------------------------------------------------------------------
# Percorsi del progetto
# ------------------------------------------------------------------------------
TARGET_NAME := spiderman-snake
BUILD       := build
SOURCES     := source
NITRO       := $(BUILD)/nitro

TARGET_ELF  := $(BUILD)/$(TARGET_NAME).elf
TARGET_NDS  := $(BUILD)/$(TARGET_NAME).nds

# Calico viene normalmente definito da ds_rules; questo fallback rende il file
# piu' leggibile e utilizzabile anche in ambienti dove non sia gia' esportato.
ifeq ($(strip $(CALICO)),)
CALICO := $(DEVKITPRO)/calico
endif

# ------------------------------------------------------------------------------
# Metadati visualizzati nel menu Nintendo DS
# ------------------------------------------------------------------------------
ROM_TITLE    := SPIDER-MAN
ROM_SUBTITLE := Snake standalone
ROM_AUTHOR   := By Francesco Pio Pipino
ROM_BANNER_INFO := $(ROM_TITLE);$(ROM_SUBTITLE);$(ROM_AUTHOR)

ICON_SRC   := icon.bmp
ICON_FIXED := $(BUILD)/icon.bmp

# ------------------------------------------------------------------------------
# Configurazione ARM9
# ------------------------------------------------------------------------------
ARCH  := -march=armv5te -mtune=arm946e-s -mthumb
SPECS := -specs=$(CALICO)/share/ds9.specs

# Una sola definizione di CFLAGS: nella versione precedente CFLAGS compariva due
# volte e la seconda definizione annullava -fdiagnostics-color=always.
CFLAGS := $(SPECS) -g -O2 -Wall -Wextra \
	-ffunction-sections -fdata-sections \
	-fdiagnostics-color=always \
	$(ARCH) \
	-DARM9 -D__CALICO_ARM9__ -D__NDS__ \
	-I$(CALICO)/include \
	-I$(DEVKITPRO)/libnds/include \
	-I$(CURDIR)/include \
	-I$(CURDIR)/$(BUILD)

LDFLAGS := $(SPECS) -g $(ARCH) \
	-Wl,-Map,$(BUILD)/$(TARGET_NAME).map \
	-L$(DEVKITPRO)/libnds/lib \
	-L$(CALICO)/lib

# Ordine intenzionale: Maxmod e filesystem prima delle librerie base.
LIBS := -lmm9 -lfilesystem -lfat -lnds9 -lcalico_ds9

# ------------------------------------------------------------------------------
# Sorgenti C
# ------------------------------------------------------------------------------
CFILES := $(wildcard $(SOURCES)/*.c)
OFILES := $(patsubst $(SOURCES)/%.c,$(BUILD)/%.o,$(CFILES))
DEPS   := $(OFILES:.o=.d)

# ------------------------------------------------------------------------------
# Grafica RAW incorporata nell'ARM9
# ------------------------------------------------------------------------------
BACKGROUND_RAW := $(BUILD)/background.raw
BACKGROUND_O   := $(BUILD)/background_bin.o

INTRO_BG_RAW   := $(BUILD)/intro_background.raw
INTRO_BG_O     := $(BUILD)/intro_background_bin.o

# ------------------------------------------------------------------------------
# Audio: sorgenti MP3 -> PCM grezzo nel NitroFS
# ------------------------------------------------------------------------------
MUSIC_MP3 := data/music.mp3
MUSIC_PCM := $(NITRO)/music.pcm

INTRO_MP3 := data/intro.mp3
INTRO_PCM := $(NITRO)/intro.pcm

# ------------------------------------------------------------------------------
# Target principali
# ------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET_NDS)
	@printf '\033[0m'

$(BUILD):
	@mkdir -p "$@"

$(NITRO):
	@mkdir -p "$@"

# ------------------------------------------------------------------------------
# Conversione sfondi: PNG 256x192 -> BGR555 little-endian
# ------------------------------------------------------------------------------
$(BACKGROUND_RAW): gfx/background.png | $(BUILD)
	@printf '\033[36mRAW\033[0m    %s -> %s\n' "$<" "$@"
	@ffmpeg -hide_banner -loglevel error -y \
		-i "$<" -vf "scale=256:192" \
		-f rawvideo -pix_fmt bgr555le "$@"

$(BACKGROUND_O): $(BACKGROUND_RAW)
	@printf '\033[36mBIN2O\033[0m  %s -> %s\n' "$<" "$@"
	@arm-none-eabi-objcopy \
		-I binary -O elf32-littlearm -B arm \
		--rename-section .data=.rodata "$<" "$@"

$(INTRO_BG_RAW): gfx/intro_background.png | $(BUILD)
	@printf '\033[36mRAW\033[0m    %s -> %s\n' "$<" "$@"
	@ffmpeg -hide_banner -loglevel error -y \
		-i "$<" -vf "scale=256:192" \
		-f rawvideo -pix_fmt bgr555le "$@"

$(INTRO_BG_O): $(INTRO_BG_RAW)
	@printf '\033[36mBIN2O\033[0m  %s -> %s\n' "$<" "$@"
	@arm-none-eabi-objcopy \
		-I binary -O elf32-littlearm -B arm \
		--rename-section .data=.rodata "$<" "$@"

# ------------------------------------------------------------------------------
# Icona banner
# ------------------------------------------------------------------------------
# ndstool vuole un BMP; non convertiamo falsamente il file in un formato diverso.
# La sorgente icon.bmp deve quindi essere gia' una BMP valida per il banner DS.
$(ICON_FIXED): $(ICON_SRC) | $(BUILD)
	@printf '\033[36mICON\033[0m   %s -> %s\n' "$<" "$@"
	@cp "$<" "$@"

# ------------------------------------------------------------------------------
# Compilazione C
# ------------------------------------------------------------------------------
$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	@printf '\033[34mCC\033[0m     \033[36m%s\033[0m\n' "$<"
	@$(CC) $(CFLAGS) -MMD -MP -c "$<" -o "$@"

# ------------------------------------------------------------------------------
# Conversione audio
# ------------------------------------------------------------------------------
# Il formato deve combaciare ESATTAMENTE con musicCallback() in main.c:
#   - signed PCM little-endian
#   - 16 bit
#   - stereo interleaved
#   - 22050 Hz
$(MUSIC_PCM): $(MUSIC_MP3) | $(NITRO)
	@printf '\033[36mAUDIO\033[0m  %s -> %s\n' "$<" "$@"
	@ffmpeg -hide_banner -loglevel error -y \
		-i "$<" -vn -ar 22050 -ac 2 \
		-c:a pcm_s16le -f s16le "$@"
	@test -s "$@" || (echo "ERRORE: music.pcm e' vuoto" && rm -f "$@" && false)

$(INTRO_PCM): $(INTRO_MP3) | $(NITRO)
	@printf '\033[36mAUDIO\033[0m  %s -> %s\n' "$<" "$@"
	@ffmpeg -hide_banner -loglevel error -y \
		-i "$<" -vn -ar 22050 -ac 2 \
		-c:a pcm_s16le -f s16le "$@"
	@test -s "$@" || (echo "ERRORE: intro.pcm e' vuoto" && rm -f "$@" && false)

# ------------------------------------------------------------------------------
# Link ARM9
# ------------------------------------------------------------------------------
$(TARGET_ELF): $(BACKGROUND_O) $(INTRO_BG_O) $(OFILES) | $(BUILD)
	@printf '\033[32mLD\033[0m     %s\n' "$@"
	@$(CC) $(LDFLAGS) \
		$(BACKGROUND_O) $(INTRO_BG_O) $(OFILES) \
		$(LIBS) -o "$@"

# ------------------------------------------------------------------------------
# Creazione ROM NDS
# ------------------------------------------------------------------------------
# ds7_maine.elf e' l'ARM7 standard previsto dalle ds_rules moderne.
# -d $(NITRO) incorpora intro.pcm e music.pcm nella ROM come NitroFS.
$(TARGET_NDS): $(TARGET_ELF) $(MUSIC_PCM) $(INTRO_PCM) $(ICON_FIXED)
	@printf '\033[32mNDS\033[0m    %s\n' "$@"
	@ndstool -c "$@" \
		-9 "$(TARGET_ELF)" \
		-7 "$(CALICO)/bin/ds7_maine.elf" \
		-b "$(ICON_FIXED)" \
		"$(ROM_BANNER_INFO)" \
		-d "$(NITRO)"
	@printf '\033[32mOK\033[0m     ROM creata: %s\n' "$@"

# ------------------------------------------------------------------------------
# Pulizia
# ------------------------------------------------------------------------------
clean:
	@printf '\033[33mCLEAN\033[0m  %s\n' "$(BUILD)"
	@rm -rf "$(BUILD)"
	@printf '\033[0m'

-include $(DEPS)
