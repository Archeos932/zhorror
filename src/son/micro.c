/* BOURDONNEMENT - le micro de l'ordinateur : ce que tu dis, le monstre l'entend. Chuchoter passe, parler s'entend,
   crier l'attire a coup sur. Windows : waveIn (winmm, deja lie) ; macOS / Linux : mac_mic_* (couche plateforme).
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Le niveau brut (RMS 0..1) est multiplie par micGain, puis converti en bruit (0..MICRO_MAX) par micro_bruit(). */

static int micOn;                                         /* reglage : utiliser le micro */
static float micGain = MICRO_GAIN_DEFAUT;                 /* reglage : sensibilite */
static int micOuvert;                                     /* le micro est vraiment ouvert */
static float micNiveau;                                   /* niveau lisse (crete qui retombe), 0..1 */

#ifdef _WIN32
#define MIC_NBUF 4
#define MIC_LEN  1024
static HWAVEIN micIn;
static WAVEHDR micHdr[MIC_NBUF];
static short micBuf[MIC_NBUF][MIC_LEN];
static int mic_ouvrir(void) {
    WAVEFORMATEX fmt = { 0 };
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 1; fmt.nSamplesPerSec = 22050;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 2; fmt.nAvgBytesPerSec = 22050 * 2;
    if (waveInOpen(&micIn, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return 0;
    for (int i = 0; i < MIC_NBUF; i++) {
        memset(&micHdr[i], 0, sizeof micHdr[i]);
        micHdr[i].lpData = (LPSTR)micBuf[i]; micHdr[i].dwBufferLength = sizeof micBuf[i];
        waveInPrepareHeader(micIn, &micHdr[i], sizeof(WAVEHDR));
        waveInAddBuffer(micIn, &micHdr[i], sizeof(WAVEHDR));
    }
    waveInStart(micIn);
    return 1;
}
static float mic_lire(void) {                             /* RMS des tampons arrives depuis la derniere image */
    float best = -1;
    for (int i = 0; i < MIC_NBUF; i++) {
        if (!(micHdr[i].dwFlags & WHDR_DONE)) continue;
        int n = (int)(micHdr[i].dwBytesRecorded / 2);
        double e = 0;
        for (int k = 0; k < n; k++) { double v = micBuf[i][k] / 32768.0; e += v * v; }
        if (n > 0) { float r = (float)sqrt(e / n); if (r > best) best = r; }
        micHdr[i].dwFlags &= ~WHDR_DONE;
        waveInAddBuffer(micIn, &micHdr[i], sizeof(WAVEHDR));
    }
    return best;
}
static void mic_fermer(void) {
    waveInStop(micIn); waveInReset(micIn);
    for (int i = 0; i < MIC_NBUF; i++) waveInUnprepareHeader(micIn, &micHdr[i], sizeof(WAVEHDR));
    waveInClose(micIn);
}
#else
static int mic_ouvrir(void) { return mac_mic_open(22050); }
static float mic_lire(void) { return mac_mic_level(); }
static void mic_fermer(void) { mac_mic_close(); }
#endif

/* ouvre / ferme selon le reglage (appele au demarrage et quand on change le reglage) */
static void micro_appliquer(void) {
    if (micOn && !micOuvert) {
        micOuvert = mic_ouvrir();
        if (!micOuvert) { set_status("micro introuvable (ou acces refuse)"); micOn = 0; }
    } else if (!micOn && micOuvert) { mic_fermer(); micOuvert = 0; micNiveau = 0; }
}

/* chaque image : niveau lisse (monte tout de suite, retombe en ~0,3 s) */
static void micro_maj(float dt) {
    if (!micOuvert) { micNiveau = 0; return; }
    float r = mic_lire();
    if (r >= 0 && r > micNiveau) micNiveau = r;
    else micNiveau *= expf(-dt * 3.5f);
}

/* bruit entendu par les monstres (0 : chuchotement ou silence) */
static float micro_bruit(void) {
    float v = micNiveau * micGain;
    return clampf((v - MICRO_SEUIL) / MICRO_PLAGE, 0, MICRO_MAX);
}
