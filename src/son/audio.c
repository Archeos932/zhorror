/* BOURDONNEMENT - mixeur waveOut / AudioQueue / ALSA (repris de VELOCITY)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Les sons sont fabriques une fois au demarrage par son/sons.c (aucun fichier son). */

#define SR      44100
#define NBUF    4
#define BUFLEN  512
#define MAXV    32
enum { S_BOURDON,                                         /* neons : boucle de 2 s, 120 Hz + harmoniques */
       S_GRESILLE,                                        /* neon qui clignote */
       S_PAS,                                             /* pas sur moquette mouillee */
       S_SOUFFLE,                                         /* a bout de souffle */
       S_CLIC,                                            /* bouton de menu */
       S_LAMPE,                                           /* clic de la lampe torche */
       S_FUSIBLE,                                         /* fusible ramasse */
       S_DING,                                            /* ascenseur alimente / arrive */
       S_PORTE,                                           /* portes d'ascenseur */
       S_COEUR,                                           /* battement de coeur */
       S_CRI,                                             /* le joueur crie */
       S_DRONE,                                           /* nappe grave de tension (boucle de 4 s) */
       S_RODEUR_PAS,                                      /* pas lourd et mouille du Rodeur */
       S_RODEUR_SOUFFLE,                                  /* respiration rauque (boucle de 3 s) */
       S_RODEUR_CLIC,                                     /* claquements : il "ecoute" */
       S_HURLEMENT,                                       /* screamer de mort */
       S_STINGER,                                         /* apparition : cordes qui crissent */
       S_FAUX_BRUIT,                                      /* choc sourd au loin (faux depart) */
       S_FLAQUE,                                          /* pas dans l'eau */
       S_VERRE,                                           /* pas sur du verre brise */
       S_MACHOIRE,                                        /* machoire de loup qui se referme */
       S_ALARME,                                          /* bip du faux fusible */
       S_CRAQUE,                                          /* os qui craquent : le Locataire bouge */
       S_GRATTE,                                          /* griffes sur la moquette : la Rampante */
       S_RUGISSEMENT,                                     /* le Rodeur se lance a ta poursuite */
       S_CRI_RAMPANTE,                                    /* la Rampante a vu ta lumiere */
       S_RADIO,                                           /* gresillement de radio (messages) */
       S_TELE,                                            /* neige d'une tele (boucle de 2 s) */
       S_FRIGO,                                           /* ronronnement d'un frigo (boucle de 2 s) */
       S_BOITE,                                           /* boite a musique : la melodie du centre commercial */
       S_GOUTTE,                                          /* goutte qui tombe dans l'eau */
       S_GRINCEMENT,                                      /* porte d'armoire / de frigo qui grince */
       S_JOUET,                                           /* un jouet qui tombe et roule */
       S_CASSETTE,                                        /* cassette VHS ramassee */
       S_COUNT };
typedef struct { short *data; int len; } Sound;
/* loop : voix en boucle, jamais volee par play ; fade : baisse de volume par echantillon (arret sans clic) */
typedef struct { int snd, active, loop; float pos, step, vol, fade, gl, gr; } Voice;   /* gl, gr : gains gauche / droite */
static Sound sounds[S_COUNT];
static Voice voices[MAXV];
#ifdef _WIN32
static HWAVEOUT wout;
static WAVEHDR whdr[NBUF];
#endif
static short wbuf[NBUF][BUFLEN * 2];                      /* BUFLEN images, 2 voies entrelacees */
static int audio_ok;
static float masterVol = 1.0f;
#include "ambiance.c"                                     /* la musique d'horreur, synthetisee en temps reel */

static int play_voice(int s, float vol, float pitch) {    /* renvoie l'index de la voix, -1 si rien */
    if (!audio_ok) return -1;
    int best = 0; float oldest = -1;
    for (int i = 0; i < MAXV; i++) {
        if (!voices[i].active) { best = i; break; }
        if (voices[i].loop) continue;                     /* une boucle n'est jamais volee */
        if (voices[i].pos > oldest) { oldest = voices[i].pos; best = i; }
    }
    if (voices[best].active && voices[best].loop) return -1;
    voices[best].snd = s; voices[best].active = 1; voices[best].pos = 0;
    voices[best].step = pitch; voices[best].vol = vol; voices[best].loop = 0; voices[best].fade = 0;
    voices[best].gl = voices[best].gr = 1;
    return best;
}
static void play(int s, float vol, float pitch) { play_voice(s, vol, pitch); }

/* voix en boucle (audio_fill tourne dans le fil principal : aucun verrou) */
static int play_loop(int s, float vol, float pitch) {     /* renvoie index + 1, 0 si rien */
    int v = play_voice(s, vol, pitch);
    if (v < 0) return 0;
    voices[v].loop = 1;
    return v + 1;
}
static void loop_set(int v, float vol, float pitch) {
    if (v > 0 && voices[v - 1].active && voices[v - 1].loop) { voices[v - 1].vol = vol; voices[v - 1].step = pitch; }
}
static void voice_pan(int v, float gl, float gr) {       /* v : index de voix (play_voice) */
    if (v >= 0 && v < MAXV) { voices[v].gl = gl; voices[v].gr = gr; }
}
static void loop_stop(int v) {                            /* fin en 30 ms, sans clic */
    if (v <= 0 || !voices[v - 1].active || !voices[v - 1].loop) return;
    voices[v - 1].loop = 0;
    voices[v - 1].fade = fmaxf(voices[v - 1].vol, 0.001f) / (0.03f * SR);
}

/* n = nombre d'images audio ; out = n paires gauche / droite entrelacees. Chaque voix a ses gains gauche / droite
   (son 3D : son/son3d.c). */
static void audio_fill(short *out, int n) {
    for (int i = 0; i < n; i++) {
        float accL = 0, accR = 0, mL, mR;
        amb_frame(&mL, &mR);
        accL += mL * 32768.0f; accR += mR * 32768.0f;
        for (int v = 0; v < MAXV; v++) {
            Voice *vo = &voices[v];
            if (!vo->active) continue;
            Sound *s = &sounds[vo->snd];
            if (vo->fade > 0 && (vo->vol -= vo->fade) <= 0) { vo->active = 0; continue; }
            int p = (int)vo->pos;
            if (p >= s->len - 1) {
                if (!vo->loop) { vo->active = 0; continue; }
                vo->pos -= (float)(s->len - 1);           /* boucle : retour au debut */
                p = (int)vo->pos;
            }
            float f = vo->pos - p;
            float smp = (s->data[p] * (1 - f) + s->data[p + 1] * f) * vo->vol;
            accL += smp * vo->gl; accR += smp * vo->gr;
            vo->pos += vo->step;
        }
        out[2 * i] = (short)(tanhf(accL / 32768.0f * 0.7f * masterVol) * 32000.0f);
        out[2 * i + 1] = (short)(tanhf(accR / 32768.0f * 0.7f * masterVol) * 32000.0f);
    }
}

static void audio_update(void) {
    if (!audio_ok) return;
    for (int i = 0; i < NBUF; i++) {
#ifdef _WIN32
        if (whdr[i].dwFlags & WHDR_DONE) {
            audio_fill(wbuf[i], BUFLEN);
            waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
        }
#else
        if (mac_audio_done(i)) {
            audio_fill(wbuf[i], BUFLEN);
            mac_audio_write(i, wbuf[i]);
        }
#endif
    }
}

static void audio_init(void) {
#ifndef _WIN32
    if (!mac_audio_open(SR, NBUF, BUFLEN)) return;
    audio_ok = 1;
    for (int i = 0; i < NBUF; i++) {
        audio_fill(wbuf[i], BUFLEN);
        mac_audio_write(i, wbuf[i]);
    }
#else
    WAVEFORMATEX fmt = { 0 };
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 2; fmt.nSamplesPerSec = SR;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 4; fmt.nAvgBytesPerSec = SR * 4;
    if (waveOutOpen(&wout, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;
    audio_ok = 1;
    for (int i = 0; i < NBUF; i++) {
        memset(&whdr[i], 0, sizeof(WAVEHDR));
        whdr[i].lpData = (LPSTR)wbuf[i];
        whdr[i].dwBufferLength = BUFLEN * 2 * sizeof(short);
        waveOutPrepareHeader(wout, &whdr[i], sizeof(WAVEHDR));
        audio_fill(wbuf[i], BUFLEN);
        waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
    }
#endif
}
