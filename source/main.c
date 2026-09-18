/* ==============================================================================
 * PROGETTO: Spider-Man Snake Standalone
 * PIATTAFORMA: Nintendo DS (devkitARM / libnds / Calico / Maxmod)
 * AUTORE: Francesco Pio Pipino
 * ==============================================================================
 * ARCHITETTURA GRAFICA E GESTIONE HARDWARE:
 * 
 * 1. SCHERMO SUPERIORE (Main 2D Engine):
 *    - Modalita' Video: MODE_5_2D (supporta framebuffer bitmap lineari a 16-bit).
 *    - Sfondo (Layer 3): BgType_Bmp16 con risoluzione 256x192 allocato in VRAM_A.
 *      Gestisce l'immagine dell'intro alla massima luminosita' (100%),
 *      lo sfondo di gioco a luminosita' ridotta (40%) per far risaltare gli elementi,
 *      e l'effetto oscurato di pausa (12%, dimming ~70%).
 *    - Sprite (OAM - Object Attribute Memory): Allocati in VRAM_B con mappatura 1D.
 *      Gestiscono i segmenti dello Snake e il cibo (16x16 pixel a 4bpp).
 * 
 * 2. SCHERMO INFERIORE (Sub 2D Engine):
 *    - Modalita' Video: MODE_0_2D (modalita' standard basata su tessere di testo).
 *    - Memoria Video: VRAM_C dedicata unicamente alla console di sistema.
 *    - Console: Inizializzata tramite consoleDemoInit(), garantisce un font nitido,
 *      leggibile, bianco su fondo nero solido, privo di conflitti VRAM o righe.
 *    - Impaginazione: Calibrata scrupolosamente per la griglia 32x24 caratteri,
 *      con layout centrate per Intro, HUD di gioco, Pausa e Menu di Game Over.
 * 
 * 3. AUDIO (Maxmod Stream):
 *    - Canali PCM stereo a 16-bit e 22050 Hz gestiti in streaming dal filesystem virtuale NitroFS.
 *    - Riavvolgimento automatico a fine file per garantire il loop continuo.
 *    - Silenziamento istantaneo del buffer sia durante la Pausa che sul Game Over.
 * 
 * 4. EFFETTI HARDWARE:
 *    - Transizioni tra gli stati gestite tramite i registri di luminosita' master
 *      del DS (Fade Out / Fade In da -16 a 0).
 * ============================================================================== */

#include <nds.h>
#include <maxmod9.h>
#include <fat.h>
#include <filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------------
 * RIFERIMENTI AI DATI GRAFICI BINARI (Incorporati tramite objcopy nel Makefile)
 * ------------------------------------------------------------------------------
 * Questi simboli puntano all'inizio degli array di pixel lineari a 16-bit (RGB555)
 * convertiti in precedenza da FFmpeg.
 */
extern const u16 _binary_build_background_raw_start[];
extern const u16 _binary_build_intro_background_raw_start[];

/* ------------------------------------------------------------------------------
 * COSTANTI DI GIOCO E DIMENSIONI GRIGLIA
 * ------------------------------------------------------------------------------
 * Lo schermo del Nintendo DS ha una risoluzione nativa di 256x192 pixel.
 * Con blocchi da 16x16 pixel, otteniamo una matrice di:
 * 16 colonne (256 / 16) per 12 righe (192 / 16) = 192 celle totali.
 */
#define GRID_W 16
#define GRID_H 12
#define CELL 16
#define MAX_SEGMENTS (GRID_W * GRID_H)

/* Parametri per lo streaming audio via Maxmod */
#define MUSIC_RATE 22050
#define MUSIC_BUFFER 4096

/* ------------------------------------------------------------------------------
 * MACCHINA A STATI PRINCIPALE
 * ------------------------------------------------------------------------------ */
typedef enum {
    STATE_INTRO, /* Schermata introduttiva con musica intro.pcm */
    STATE_GAME   /* Partita attiva con musica music.pcm */
} GameState;

/* Struttura per rappresentare un nodo del corpo del serpente sulla griglia */
typedef struct {
    s8 x;
    s8 y;
} Segment;

/* ------------------------------------------------------------------------------
 * VARIABILI GLOBALI DI STATO DEL GAMEPLAY
 * ------------------------------------------------------------------------------ */
static Segment snake[MAX_SEGMENTS]; /* Array dei nodi occupati dallo snake */
static int snakeLength;             /* Lunghezza attuale del serpente */
static int foodX, foodY;            /* Posizione attuale del cibo sulla griglia */
static int dirX, dirY;              /* Direzione di movimento attuale */
static int nextDirX, nextDirY;      /* Prossima direzione (buffer per input veloci) */
static int score;                   /* Punteggio accumulato */
static bool gameOver;               /* Flag di sconfitta / partita terminata */
static bool isPaused = false;       /* Flag di pausa attiva */
static int moveTimer;               /* Contatore per temporizzare i movimenti */

/* 
 * Selezione nel menu di fine partita:
 * 0 = SÌ (Torna alla schermata introduttiva)
 * 1 = NO (Riavvia immediatamente una nuova partita)
 */
static int gameOverChoice = 0;

/* Puntatori alla memoria video per sprite e framebuffer principale */
static u16* snakeGfx;
static u16* foodGfx;
static u16* bgMainPtr;

/* ------------------------------------------------------------------------------
 * PALETTE COLORI SPRITE (Hardware OAM - Spazio colore RGB15: 5 bit per canale)
 * ------------------------------------------------------------------------------ */

/* Palette 0: Snake bianco puro ad alta visibilita' con bordo scuro */
static const u16 snakePalette[16] = {
    0,                          /* Indice 0: Trasparenza hardware OAM */
    RGB15(31, 31, 31),          /* Indice 1: Bianco pieno */
    RGB15(10, 10, 10),          /* Indice 2: Bordo scuro di stacco */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Palette 1: Cibo bianco pieno */
static const u16 foodPalette[16] = {
    0,                          /* Indice 0: Trasparenza hardware OAM */
    RGB15(31, 31, 31),          /* Indice 1: Bianco pieno */
    RGB15(12, 12, 12),          /* Indice 2: Bordo di contrasto */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------------------
 * TEXTURE DEGLI SPRITE (16x16 pixel in formato 4bpp a 16 colori)
 * ------------------------------------------------------------------------------
 * Nel Nintendo DS, uno sprite 16x16 a 4 bit per pixel (128 byte totali) e'
 * memorizzato internamente come 4 tessere contigue da 8x8 pixel:
 * [Tessera Alto-SX][Tessera Alto-DX][Tessera Basso-SX][Tessera Basso-DX].
 */

/* Corpo dello Snake */
static const u8 snakeBlockPixels[128] = {
    /* Tessera 0: Alto a sinistra (8x8) */
    0x22, 0x22, 0x22, 0x22, 0x21, 0x11, 0x11, 0x12,
    0x21, 0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11,
    0x21, 0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11,
    0x21, 0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11,
    /* Tessera 1: Alto a destra (8x8) */
    0x22, 0x22, 0x22, 0x22, 0x11, 0x11, 0x11, 0x21,
    0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11, 0x21,
    0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11, 0x21,
    0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11, 0x21,
    /* Tessera 2: Basso a sinistra (8x8) */
    0x21, 0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11,
    0x21, 0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11,
    0x21, 0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11,
    0x21, 0x11, 0x11, 0x12, 0x22, 0x22, 0x22, 0x22,
    /* Tessera 3: Basso a destra (8x8) */
    0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11, 0x21,
    0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11, 0x21,
    0x11, 0x11, 0x11, 0x21, 0x11, 0x11, 0x11, 0x21,
    0x11, 0x11, 0x11, 0x21, 0x22, 0x22, 0x22, 0x22
};

/* Cibo */
static const u8 foodBlockPixels[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x22, 0x22, 0x00, 0x21, 0x11, 0x11,
    0x00, 0x21, 0x11, 0x11, 0x00, 0x21, 0x11, 0x11,
    0x00, 0x21, 0x11, 0x11, 0x00, 0x21, 0x11, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x20, 0x00, 0x11, 0x11, 0x12, 0x00,
    0x11, 0x11, 0x12, 0x00, 0x11, 0x11, 0x12, 0x00,
    0x11, 0x11, 0x12, 0x00, 0x11, 0x11, 0x12, 0x00,
    0x00, 0x21, 0x11, 0x11, 0x00, 0x21, 0x11, 0x11,
    0x00, 0x21, 0x11, 0x11, 0x00, 0x21, 0x11, 0x11,
    0x00, 0x21, 0x11, 0x11, 0x00, 0x02, 0x22, 0x22,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x11, 0x12, 0x00, 0x11, 0x11, 0x12, 0x00,
    0x11, 0x11, 0x12, 0x00, 0x11, 0x11, 0x12, 0x00,
    0x11, 0x11, 0x12, 0x00, 0x22, 0x22, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ------------------------------------------------------------------------------
 * GESTIONE GRAFICA: COPIA SFONDO SUPERIORE E DISSOLVENZE
 * ------------------------------------------------------------------------------ */

/*
 * Copia un array RAW a 16-bit (256x192) nel framebuffer superiore,
 * applicando un fattore percentuale di luminosita' (factor):
 * - factor = 100: Luminosita' naturale (utilizzato nell'intro).
 * - factor = 40:  Sfondo scurito (gameplay normale).
 * - factor = 12:  Sfondo oscurato al ~70% (menu di pausa).
 * - factor = 15:  Sfondo oscurato al momento della sconfitta.
 */
static void copyMainBackground(const u16* rawSrc, int factor) {
    for (int y = 0; y < 192; y++) {
        for (int x = 0; x < 256; x++) {
            u16 pixel = rawSrc[y * 256 + x];

            int r = (pixel & 0x1F);
            int g = ((pixel >> 5) & 0x1F);
            int b = ((pixel >> 10) & 0x1F);

            r = (r * factor) / 100;
            g = (g * factor) / 100;
            b = (b * factor) / 100;

            /* BIT(15) e' obbligatorio per definire il pixel come opaco nel DS */
            bgMainPtr[y * 256 + x] = RGB15(r, g, b) | BIT(15);
        }
    }
}

/* Dissolvenza hardware verso il nero su entrambi gli schermi */
static void fadeOut(void) {
    for (int b = 0; b >= -16; b--) {
        setBrightness(1, b);
        setBrightness(2, b);
        swiWaitForVBlank();
        swiWaitForVBlank();
    }
}

/* Dissolvenza hardware dal nero alla luminosita' standard */
static void fadeIn(void) {
    for (int b = -16; b <= 0; b++) {
        setBrightness(1, b);
        setBrightness(2, b);
        swiWaitForVBlank();
        swiWaitForVBlank();
    }
}

/* ------------------------------------------------------------------------------
 * GESTIONE STREAMING AUDIO (MAXMOD)
 * ------------------------------------------------------------------------------ */
static FILE* musicFp = NULL;
static bool audioStreaming = false;

/*
 * Callback audio invocata periodicamente dal mixer hardware di Maxmod.
 * Riempie il buffer di destinazione con campioni PCM stereo a 16-bit.
 */
static mm_word musicCallback(mm_word length, mm_addr dest, mm_stream_formats format) {
    if (!musicFp || format != MM_STREAM_16BIT_STEREO)
        return 0;

    /*
     * Silenzia istantaneamente l'audio azzerando il buffer di campioni
     * quando la partita e' in pausa oppure quando si e' perso.
     */
    if (isPaused || gameOver) {
        memset(dest, 0, length * 4);
        return length;
    }

    size_t wantedBytes = (size_t)length * 4;
    size_t got = fread(dest, 1, wantedBytes, musicFp);

    /* Riavvolgimento all'inizio per creare un loop infinito */
    if (got < wantedBytes) {
        fseek(musicFp, 0, SEEK_SET);
        size_t remaining = wantedBytes - got;
        got += fread((u8*)dest + got, 1, remaining, musicFp);
    }

    return (mm_word)(got / 4);
}

/*
 * Cambia traccia audio chiudendo la precedente e aprendo il nuovo file PCM da NitroFS.
 */
static void switchTrack(const char* filename) {
    if (musicFp) {
        fclose(musicFp);
        musicFp = NULL;
    }

    char nitroPath[64];
    snprintf(nitroPath, sizeof(nitroPath), "nitro:/%s", filename);
    musicFp = fopen(nitroPath, "rb");
    if (!musicFp) musicFp = fopen(filename, "rb");

    if (!audioStreaming && musicFp) {
        mm_ds_system sys;
        memset(&sys, 0, sizeof(sys));
        mmInit(&sys);

        mm_stream stream;
        memset(&stream, 0, sizeof(stream));
        stream.sampling_rate = MUSIC_RATE;
        stream.buffer_length = MUSIC_BUFFER;
        stream.callback = musicCallback;
        stream.format = MM_STREAM_16BIT_STEREO;
        stream.timer = 0;
        stream.manual = false;

        mmStreamOpen(&stream);
        audioStreaming = true;
    }
}

/* ------------------------------------------------------------------------------
 * LOGICA E MECCANICHE DI GIOCO
 * ------------------------------------------------------------------------------ */

/* Verifica se una specifica coordinata della griglia e' occupata dal serpente */
static bool occupied(int x, int y) {
    for (int i = 0; i < snakeLength; ++i) {
        if (snake[i].x == x && snake[i].y == y) return true;
    }
    return false;
}

/* Genera casualmente una nuova posizione per il cibo su una cella non occupata */
static void spawnFood(void) {
    do {
        foodX = rand() % GRID_W;
        foodY = rand() % GRID_H;
    } while (occupied(foodX, foodY));
}

/* Inizializza i parametri di una nuova sessione di gioco */
static void startGame(void) {
    snakeLength = 3;
    snake[0] = (Segment){ GRID_W / 2, GRID_H / 2 };
    snake[1] = (Segment){ GRID_W / 2 - 1, GRID_H / 2 };
    snake[2] = (Segment){ GRID_W / 2 - 2, GRID_H / 2 };

    dirX = 1; dirY = 0;
    nextDirX = 1; nextDirY = 0;
    score = 0;
    gameOver = false;
    gameOverChoice = 0;
    isPaused = false;
    moveTimer = 0;
    spawnFood();
}

/*
 * Aggiorna lo stato del gioco, scansiona i tasti e gestisce collisioni e input.
 */
static void updateGame(GameState* state) {
    scanKeys();
    int keys = keysDown();

    /* Gestione menu interattivo post-sconfitta */
    if (gameOver) {
        if ((keys & KEY_LEFT) || (keys & KEY_RIGHT)) {
            gameOverChoice ^= 1; /* Inverte la selezione tra 0 (SÌ) e 1 (NO) */
        }

        if (keys & KEY_A) {
            if (gameOverChoice == 0) {
                /* Scelto SÌ: dissolvenza a nero e ritorno all'intro con audio dedicato */
                fadeOut();
                switchTrack("intro.pcm");
                copyMainBackground(_binary_build_intro_background_raw_start, 100);
                *state = STATE_INTRO;
                fadeIn();
            } else {
                /* Scelto NO: riavvio immediato della partita */
                startGame();
                copyMainBackground(_binary_build_background_raw_start, 40);
            }
        }
        return;
    }

    /* Metti / togli la pausa con il tasto START */
    if (keys & KEY_START) {
        isPaused = !isPaused;
        /* Se in pausa oscura al 12%, altrimenti ripristina al 40% */
        copyMainBackground(_binary_build_background_raw_start, isPaused ? 12 : 40);
        return;
    }

    if (isPaused) return;

    /* Acquisizione della direzione tramite la croce direzionale D-PAD */
    if ((keys & KEY_UP) && dirY == 0) {
        nextDirX = 0; nextDirY = -1;
    } else if ((keys & KEY_DOWN) && dirY == 0) {
        nextDirX = 0; nextDirY = 1;
    } else if ((keys & KEY_LEFT) && dirX == 0) {
        nextDirX = -1; nextDirY = 0;
    } else if ((keys & KEY_RIGHT) && dirX == 0) {
        nextDirX = 1; nextDirY = 0;
    }

    /* Calcolo della velocita' progressiva in base ai punti */
    moveTimer++;
    int interval = 8 - score / 5;
    if (interval < 3) interval = 3;
    if (moveTimer < interval) return;
    moveTimer = 0;

    dirX = nextDirX;
    dirY = nextDirY;

    int nx = snake[0].x + dirX;
    int ny = snake[0].y + dirY;

    /* Collisione con i limiti perimetrali della griglia */
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
        gameOver = true;
        copyMainBackground(_binary_build_background_raw_start, 15);
        return;
    }

    bool eat = (nx == foodX && ny == foodY);
    int collisionLen = snakeLength - (eat ? 0 : 1);

    /* Collisione con il proprio corpo */
    for (int i = 0; i < collisionLen; ++i) {
        if (snake[i].x == nx && snake[i].y == ny) {
            gameOver = true;
            copyMainBackground(_binary_build_background_raw_start, 15);
            return;
        }
    }

    /* Accrescimento corpo del serpente */
    if (eat && snakeLength < MAX_SEGMENTS) ++snakeLength;

    /* Avanzamento della catena dei nodi */
    for (int i = snakeLength - 1; i > 0; --i)
        snake[i] = snake[i - 1];

    snake[0] = (Segment){ nx, ny };

    if (eat) {
        ++score;
        spawnFood();
    }
}

/*
 * Invia i dati delle posizioni degli sprite alla memoria hardware OAM.
 */
static void drawSprites(bool hideAll) {
    /* Nasconde preventivamente tutti i 128 sprite disponibili */
    for (int i = 0; i < 128; ++i)
        oamSetHidden(&oamMain, i, true);

    if (hideAll) {
        oamUpdate(&oamMain);
        return;
    }

    /* Render dei nodi dello Snake */
    for (int i = 0; i < snakeLength && i < 100; ++i) {
        oamSet(
            &oamMain, i,
            snake[i].x * CELL, snake[i].y * CELL,
            0,
            0, /* Palette 0: Snake bianco */
            SpriteSize_16x16,
            SpriteColorFormat_16Color,
            snakeGfx,
            -1, false, false, false, false, false
        );
    }

    /* Render del cibo sullo slot hardware 120 */
    oamSet(
        &oamMain, 120,
        foodX * CELL, foodY * CELL,
        0,
        1, /* Palette 1: Cibo */
        SpriteSize_16x16,
        SpriteColorFormat_16Color,
        foodGfx,
        -1, false, false, false, false, false
    );

    oamUpdate(&oamMain);
}

/* ------------------------------------------------------------------------------
 * PUNTO DI INGRESSO (MAIN)
 * ------------------------------------------------------------------------------ */
int main(void) {
    /* Accensione dell'hardware 2D per entrambi gli schermi */
    powerOn(POWER_ALL_2D);

    /*
     * MODALITÀ GRAFICHE:
     * - Main Engine: MODE_5_2D (Supporto nativo per sfondi bitmap lineari a 16-bit)
     * - Sub Engine:  MODE_0_2D (Modalita' standard a tessere; solida, veloce,
     *   priva di qualsiasi sovrapposizione o conflitto VRAM)
     */
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_0_2D);

    /*
     * RIPARTIZIONE DEI BANCHI DI MEMORIA VRAM:
     * - VRAM A: Schermo Superiore - Framebuffer Bitmap 16-bit (128 KB)
     * - VRAM B: Schermo Superiore - Memoria Texture Sprite OAM (128 KB)
     * - VRAM C: Schermo Inferiore - Console Testuale Sub Engine (128 KB)
     */
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    vramSetBankC(VRAM_C_SUB_BG);

    /* Inizializzazione framebuffer superiore 16-bit (Layer 3) */
    int bgMain = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bgMainPtr = bgGetGfxPtr(bgMain);
    memset(bgMainPtr, 0, 256 * 256 * sizeof(u16));

    /* Inizializzazione motore sprite OAM per lo schermo superiore */
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    snakeGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);
    foodGfx  = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);

    /* Copia di palette e texture grafiche degli sprite in VRAM */
    dmaCopy(snakePalette, SPRITE_PALETTE, sizeof(snakePalette));
    dmaCopy(foodPalette, SPRITE_PALETTE + 16, sizeof(foodPalette));
    dmaCopy(snakeBlockPixels, snakeGfx, sizeof(snakeBlockPixels));
    dmaCopy(foodBlockPixels, foodGfx, sizeof(foodBlockPixels));

    /*
     * Inizializzazione pulita della console sullo schermo inferiore:
     * Carica il set di caratteri nativo e imposta sfondo nero solido con testo bianco.
     */
    consoleDemoInit();
    nitroFSInit(NULL);

    /* Imposta la luminosita' a nero su entrambi i display per preparare il Fade In */
    setBrightness(1, -16);
    setBrightness(2, -16);

    /* Avvia la schermata introduttiva */
    GameState currentState = STATE_INTRO;
    switchTrack("intro.pcm");

    /* Carica l'immagine dell'intro alla massima luminosita' (100%) */
    copyMainBackground(_binary_build_intro_background_raw_start, 100);
    drawSprites(true);

    int blinkTimer = 0;
    fadeIn();

    /* Ciclo Principale di Esecuzione */
    while (pmMainLoop()) {
        blinkTimer++;

        /* ----------------------------------------------------------------------
         * STATO: SCHERMATA INTRODUTTIVA
         * ---------------------------------------------------------------------- */
        if (currentState == STATE_INTRO) {
            scanKeys();
            int keys = keysDown();

            /* Pulisce lo schermo e posiziona il cursore a riga 1, colonna 1 */
            iprintf("\x1b[2J\x1b[1;1H");

            /* Cornice perfettamente centrata (28 colonne, colonna 3 -> 30) */
            iprintf("\x1b[2;3H+--------------------------+");
            iprintf("\x1b[3;3H|       SPIDER - MAN       |");
            iprintf("\x1b[4;3H|     SNAKE STANDALONE     |");
            iprintf("\x1b[5;3H+--------------------------+");

            iprintf("\x1b[9;8H- SVILUPPATO DA -");
            iprintf("\x1b[11;6HFRANCESCO PIO PIPINO");

            /* Testo lampeggiante centrato */
            if ((blinkTimer / 25) % 2 == 0) {
                iprintf("\x1b[16;5HPREMI START PER GIOCARE");
            } else {
                iprintf("\x1b[16;5H                       ");
            }

            /* Pressione START: transizione a nero e avvio del gameplay */
            if (keys & KEY_START) {
                fadeOut();
                switchTrack("music.pcm");
                startGame();
                copyMainBackground(_binary_build_background_raw_start, 40);
                currentState = STATE_GAME;
                fadeIn();
            }

            swiWaitForVBlank();
        } 
        /* ----------------------------------------------------------------------
         * STATO: PARTITA IN CORSO
         * ---------------------------------------------------------------------- */
        else if (currentState == STATE_GAME) {
            updateGame(&currentState);

            /* Pulisce e riposiziona il cursore */
            iprintf("\x1b[2J\x1b[1;1H");

            if (gameOver) {
                /*
                 * Layout Game Over:
                 * Box di 26 colonne (colonna 4 -> 29) con menu interattivo.
                 */
                iprintf("\x1b[3;4H+--------------------------+");
                iprintf("\x1b[4;4H|     H A I  P E R S O     |");
                iprintf("\x1b[5;4H|    PARTITA  TERMINATA    |");
                iprintf("\x1b[6;4H+--------------------------+");

                iprintf("\x1b[9;6HPUNTEGGIO FINALE: %-4d", score);
                iprintf("\x1b[12;3HTornare al menu principale?");

                /* Indicatore visivo di selezione */
                if (gameOverChoice == 0) {
                    iprintf("\x1b[15;6H==> [ SI ]      [ NO ]  ");
                } else {
                    iprintf("\x1b[15;6H    [ SI ]  ==> [ NO ]  ");
                }

                iprintf("\x1b[25;3HD-PAD: Scegli | A: Conferma");
            } 
            else if (isPaused) {
                /* Layout Menu di Pausa */
                iprintf("\x1b[4;4H+--------------------------+");
                iprintf("\x1b[5;4H|       *  PAUSA  *        |");
                iprintf("\x1b[6;4H+--------------------------+");

                iprintf("\x1b[16;7HPUNTI ATTUALI: %-4d", score);
                iprintf("\x1b[20;4HPREMI START PER CONTINUARE");
            } 
            else {
                /* HUD Principale della Partita */
                iprintf("\x1b[2;3H+--------------------------+");
                iprintf("\x1b[3;3H| PUNTI: %-4d   LIVELLO: %-2d |", score, (score / 5) + 1);
                iprintf("\x1b[4;3H+--------------------------+");

                iprintf("\x1b[12;5HCOMANDI DI GIOCO:");
                iprintf("\x1b[14;7H- D-PAD : Movimento");
                iprintf("\x1b[16;7H- START : Pausa");
            }

            drawSprites(isPaused || gameOver);
            swiWaitForVBlank();
        }
    }

    /* Rilascio delle risorse audio all'uscita */
    if (musicFp) fclose(musicFp);
    mmStreamClose();
    return 0;
}