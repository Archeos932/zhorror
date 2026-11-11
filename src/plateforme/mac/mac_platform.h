/*
 * BOURDONNEMENT (repris de VELOCITY) - interface de la couche plateforme macOS (implementee dans mac_platform.m).
 * C pur : inclus par main.c (via mac_compat.h) et par mac_platform.m.
 */
#ifndef MAC_PLATFORM_H
#define MAC_PLATFORM_H

/* --- fenetre + contexte OpenGL (pipeline fixe, surface non Retina : 1 point = 1 pixel) --- */
int  mac_window_create(int w, int h, const char *title, int visible);   /* 1 si ok */
void mac_gl_software(int on);                                           /* avant mac_window_create : rendu logiciel d'Apple (--soft) */
void mac_window_place(int x, int y, int cw, int ch);                    /* zone cliente cw x ch, coin haut gauche en (x, y) */
void mac_window_size(int *w, int *h);                                   /* taille de la zone cliente */
int  mac_pump(void);                                                    /* traite les evenements ; 0 = quitter */
void mac_swap(void);
void mac_set_fullscreen(int on);
void mac_set_capture(int on);                                           /* souris captee (curseur cache, deltas bruts) */
int  mac_has_focus(void);
/* manette (GameController) : ax = stick gauche x, y (haut +), stick droit x, y (haut +), L2, R2 (0..1) ;
   btn : bits dans l'ordre K_PAD_CROSS.. de main.c (croix, rond, carre, triangle, L1, R1, L2, R2, share, options,
   L3, R3, PS, pave tactile, haut, bas, gauche, droite). Renvoie 1 si une manette est branchee. */
int  mac_pad_read(float ax[6], unsigned *btn, char *name, int n);
void mac_quit(void);
void mac_shutdown(void);
void mac_key_name(int sc, char *out, int n);                            /* nom d'un scancode Windows selon le clavier */
int  mac_make_font(unsigned base, int px, int *cw, int *ch);            /* 224 listes d'affichage (CP1252 32..255) */
int  mac_map_update(int want);                                          /* second ecran : ouvre/deplace/ferme ; 1 si ouvert */
int  mac_map_begin(int *w, int *h);                                     /* rend son contexte courant ; 0 si ferme */
void mac_map_end(void);                                                 /* affiche, puis revient au contexte du jeu */

/* --- audio : n tampons mono 16 bits, remplis par le fil principal --- */
int  mac_audio_open(int rate, int nbuf, int buflen);
int  mac_audio_done(int i);                                             /* le tampon i est-il libre ? */
void mac_audio_write(int i, const short *samples);
void mac_audio_close(void);

/* --- micro : capture mono 16 bits ; niveau = RMS (0..1) du dernier tampon (BOURDONNEMENT) --- */
int   mac_mic_open(int rate);                                           /* 1 si ok (macOS : demande l'autorisation) */
float mac_mic_level(void);                                              /* -1 : rien de neuf depuis la derniere lecture */
void  mac_mic_close(void);

/* --- WebSocket texte (lobby) : bloquant, a utiliser depuis un fil --- */
typedef struct MacWs MacWs;
MacWs *mac_ws_open(const char *host, int port, int secure);             /* NULL si echec */
int  mac_ws_send(MacWs *w, const char *s, int n);                       /* 0 si ok */
int  mac_ws_recv(MacWs *w, char *buf, int cap);                         /* longueur (0 = ligne trop longue), -1 = ferme / mort */
void mac_ws_close(MacWs *w);                                            /* coupe la connexion, debloque un recv en cours */
void mac_ws_free(MacWs *w);                                             /* libere (apres l'arret des fils qui s'en servent) */

/* --- rappels fournis par main.c --- */
void plat_key(int sc, int down, int repeat);                            /* scancode Windows */
void plat_char(int ch);
void plat_mouse_move(int x, int y);
void plat_button(int id, int down);                                     /* 0 gauche, 1 droit, 2 milieu, 3 pouce arriere, 4 pouce avant */
void plat_wheel(void);
void plat_focus_lost(void);
void plat_resize(int w, int h);
void plat_raw_motion(float dx, float dy);

#endif
