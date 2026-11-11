/* BOURDONNEMENT - connexion au serveur de salons (Cloudflare, worker/) : un WebSocket texte, lu et ecrit par deux
   fils (repris du lobby de VELOCITY). Le jeu n'y touche que par ws_envoyer() et ws_ligne().
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Windows : WinHTTP ; macOS / Linux : mac_ws_* (couche plateforme). Tout le jeu passe par ce WebSocket (relais) :
   un peu plus lent qu'une connexion directe, mais ca passe derriere n'importe quelle box, 4G ou reseau d'ecole. */

#define SERVEUR_DEFAUT "bourdonnement-salons.bourdonnementttt.workers.dev"   /* serveur= dans bourdonnement.ini le remplace */
#define WS_Q    96
#define WS_LIGNE 2048
enum { WS_OFF, WS_CONNEXION, WS_ON, WS_ECHEC };
static char serveur[96] = SERVEUR_DEFAUT;                 /* "hote" (wss, 443) ou "hote:port" (ws si le port n'est pas 443) */
static volatile LONG wsEtat, wsStop;
static CRITICAL_SECTION wsCs;
static int wsCsPret;
static char wsIn[WS_Q][WS_LIGNE], wsOut[WS_Q][WS_LIGNE];
static int wsInN, wsOutN;
static HANDLE wsEvt, wsLecteur, wsEcrivain;
#ifdef _WIN32
static HINTERNET wsSes, wsCon, wsH;
#define WS_SEND(s, n) (WinHttpWebSocketSend(wsH, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)(s), (DWORD)(n)) != NO_ERROR)
#else
static MacWs *wsH;
#define WS_SEND(s, n) (mac_ws_send(wsH, (s), (int)(n)) != 0)
#endif
static FILE *netLog;                                      /* --log : journal du reseau (tests a plusieurs) */

static void netlog(const char *fmt, ...) {
    if (!netLog) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(netLog, "[%7.2f] ", nowT);
    vfprintf(netLog, fmt, ap);
    fputc('\n', netLog);
    fflush(netLog);
    va_end(ap);
}

static DWORD WINAPI ws_ecrivain(LPVOID arg) {
    (void)arg;
    while (!wsStop && wsEtat == WS_ON) {
        DWORD w = WaitForSingleObject(wsEvt, 25000);
        if (wsStop || wsEtat != WS_ON) break;
        for (;;) {
            char ligne[WS_LIGNE];
            EnterCriticalSection(&wsCs);
            int a = wsOutN > 0;
            if (a) {
                memcpy(ligne, wsOut[0], WS_LIGNE);
                memmove(wsOut[0], wsOut[1], (size_t)(wsOutN - 1) * WS_LIGNE);
                wsOutN--;
            }
            LeaveCriticalSection(&wsCs);
            if (!a) break;
            if (WS_SEND(ligne, strlen(ligne))) { wsEtat = WS_ECHEC; return 0; }
        }
        if (w == WAIT_TIMEOUT && WS_SEND("ping", 4)) { wsEtat = WS_ECHEC; return 0; }   /* garde la connexion en vie */
    }
    return 0;
}

static void ws_recu(const char *buf, int n) {
    EnterCriticalSection(&wsCs);
    if (wsInN < WS_Q) { memcpy(wsIn[wsInN], buf, (size_t)n); wsIn[wsInN][n] = 0; wsInN++; }
    LeaveCriticalSection(&wsCs);
}

static DWORD WINAPI ws_lecteur(LPVOID arg) {
    (void)arg;
    char addr[96];
    snprintf(addr, sizeof addr, "%s", serveur);
    int port = 443;
    char *colon = strchr(addr, ':');
    if (colon) { *colon = 0; port = atoi(colon + 1); }
#ifndef _WIN32
    MacWs *ws = mac_ws_open(addr, port, port == 443);
    EnterCriticalSection(&wsCs);
    int arrete = wsStop;
    if (!arrete && ws) wsH = ws;
    LeaveCriticalSection(&wsCs);
    if (arrete || !ws) {
        if (ws) { mac_ws_close(ws); mac_ws_free(ws); }
        if (!arrete) wsEtat = WS_ECHEC;
        return 0;
    }
    wsEtat = WS_ON;
    wsEcrivain = CreateThread(NULL, 0, ws_ecrivain, NULL, 0, NULL);
    for (;;) {
        char buf[WS_LIGNE];
        int got = mac_ws_recv(ws, buf, sizeof buf);
        if (got < 0) break;
        if (got == 0) continue;                           /* ligne trop longue : jetee */
        ws_recu(buf, got);
    }
#else
    wchar_t whost[96];
    MultiByteToWideChar(CP_UTF8, 0, addr, -1, whost, 96);
    HINTERNET ses = WinHttpOpen(L"BOURDONNEMENT", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET con = NULL, req = NULL, ws = NULL;
    if (ses) {
        WinHttpSetTimeouts(ses, 5000, 5000, 5000, 60000);
        con = WinHttpConnect(ses, whost, (INTERNET_PORT)port, 0);
    }
    if (con) req = WinHttpOpenRequest(con, L"GET", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      port == 443 ? WINHTTP_FLAG_SECURE : 0);
    if (req && WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) &&
        WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL))
        ws = WinHttpWebSocketCompleteUpgrade(req, 0);
    if (req) WinHttpCloseHandle(req);
    EnterCriticalSection(&wsCs);
    int arrete = wsStop;
    if (!arrete) { wsSes = ses; wsCon = con; wsH = ws; }
    LeaveCriticalSection(&wsCs);
    if (arrete || !ws) {
        if (ws) WinHttpCloseHandle(ws);
        if (con) WinHttpCloseHandle(con);
        if (ses) WinHttpCloseHandle(ses);
        if (!arrete) wsEtat = WS_ECHEC;
        return 0;
    }
    wsEtat = WS_ON;
    wsEcrivain = CreateThread(NULL, 0, ws_ecrivain, NULL, 0, NULL);
    char buf[WS_LIGNE];
    int len = 0;
    for (;;) {
        DWORD got = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bt;
        if (WinHttpWebSocketReceive(ws, buf + len, (DWORD)(sizeof buf - 1 - len), &got, &bt) != NO_ERROR) break;
        if (bt == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;
        len += (int)got;
        if (bt == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) { ws_recu(buf, len); len = 0; }
        else if (len >= (int)sizeof buf - 8) len = 0;   /* beaucoup trop long : jete */
    }
#endif
    if (!wsStop) wsEtat = WS_ECHEC;
    SetEvent(wsEvt);                                      /* reveille l'ecrivain pour qu'il se termine */
    return 0;
}

static void ws_arreter(void) {
    if (!wsCsPret) return;
    wsStop = 1;
    SetEvent(wsEvt);
#ifdef _WIN32
    EnterCriticalSection(&wsCs);
    HINTERNET ws = wsH, con = wsCon, ses = wsSes;
    wsH = wsCon = wsSes = NULL;
    LeaveCriticalSection(&wsCs);
    if (ws) WinHttpCloseHandle(ws);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
#else
    EnterCriticalSection(&wsCs);
    MacWs *ws = wsH;
    LeaveCriticalSection(&wsCs);
    if (ws) mac_ws_close(ws);
#endif
    if (wsLecteur) { WaitForSingleObject(wsLecteur, 1500); CloseHandle(wsLecteur); wsLecteur = NULL; }
    if (wsEcrivain) { WaitForSingleObject(wsEcrivain, 1500); CloseHandle(wsEcrivain); wsEcrivain = NULL; }
#ifndef _WIN32
    if (ws) { mac_ws_free(ws); wsH = NULL; }
#endif
    wsEtat = WS_OFF; wsStop = 0; wsInN = wsOutN = 0;
}

static void ws_demarrer(void) {
    if (wsEtat == WS_CONNEXION || wsEtat == WS_ON) return;
    if (!wsCsPret) {
        InitializeCriticalSection(&wsCs);
        wsEvt = CreateEventA(NULL, FALSE, FALSE, NULL);
        wsCsPret = 1;
    }
    if (wsLecteur || wsEcrivain) ws_arreter();
    wsStop = 0; wsInN = wsOutN = 0;
    wsEtat = WS_CONNEXION;
    wsLecteur = CreateThread(NULL, 0, ws_lecteur, NULL, 0, NULL);
    if (!wsLecteur) wsEtat = WS_ECHEC;
    netlog("serveur : connexion a %s", serveur);
}

static int ws_envoyer(const char *fmt, ...) {             /* 0 si pas connecte ou file pleine */
    char b[WS_LIGNE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (!wsCsPret || wsEtat != WS_ON) return 0;
    int ok = 0;
    EnterCriticalSection(&wsCs);
    if (wsOutN < WS_Q) { memcpy(wsOut[wsOutN++], b, strlen(b) + 1); ok = 1; }
    LeaveCriticalSection(&wsCs);
    SetEvent(wsEvt);
    if (b[0] != 'd') netlog(">> %s", b);
    return ok;
}

static int ws_ligne(char *out) {                          /* depile une ligne recue ; 0 s'il n'y en a pas */
    if (!wsCsPret) return 0;
    int a = 0;
    EnterCriticalSection(&wsCs);
    if (wsInN > 0) {
        memcpy(out, wsIn[0], WS_LIGNE);
        memmove(wsIn[0], wsIn[1], (size_t)(wsInN - 1) * WS_LIGNE);
        wsInN--; a = 1;
    }
    LeaveCriticalSection(&wsCs);
    return a;
}

/* base64 : les paquets du jeu voyagent en texte dans le WebSocket */
static const char b64c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int b64_enc(const unsigned char *d, int n, char *out) {
    int k = 0;
    for (int i = 0; i < n; i += 3) {
        unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        out[k++] = b64c[v >> 18 & 63]; out[k++] = b64c[v >> 12 & 63];
        out[k++] = i + 1 < n ? b64c[v >> 6 & 63] : '=';
        out[k++] = i + 2 < n ? b64c[v & 63] : '=';
    }
    out[k] = 0;
    return k;
}
static int b64_dec(const char *s, unsigned char *out, int cap) {   /* -1 si invalide */
    int k = 0, bits = 0;
    unsigned v = 0;
    for (; *s && *s != '='; s++) {
        const char *p = strchr(b64c, *s);
        if (!p) return -1;
        v = v << 6 | (unsigned)(p - b64c); bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (k >= cap) return -1;
            out[k++] = (unsigned char)(v >> bits);
        }
    }
    return k;
}
