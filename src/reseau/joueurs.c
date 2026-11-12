/* BOURDONNEMENT - les joueurs de la partie (solo ou en ligne) et les "cibles" des monstres
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   En ligne, chaque joueur a une place (slot) : 0 = l'hote, 1..MAXP-1 = les autres. Chacun simule son propre
   deplacement et envoie son etat (NetJ) ; l'hote simule les monstres, les pieges, les fusibles et l'ascenseur pour
   tout le monde. Les monstres ne voient pas "le joueur" mais des cibles : tous les joueurs vivants (cib[]). */

#define MAXP 4
enum { NET_OFF, NET_HOTE, NET_CLIENT };
static int netMode = NET_OFF, monSlot;                    /* role reseau ; ma place */
static char monNom[16] = "joueur";
static float spectateurK;                                 /* mort en ligne : place du joueur que l'on regarde (E : suivant) */

/* drapeaux de NetJ.flags */
enum { JF_LAMPE = 1, JF_ACCROUPI = 2, JF_MORT = 4, JF_PIEGE = 8, JF_UTILISE = 16, JF_COURT = 32, JF_CRI = 64, JF_CACHE = 128 };
#pragma pack(push, 1)
typedef struct {                                          /* l'etat d'un joueur, tel qu'il circule (32 octets) */
    float x, z, yaw, pitch, lampeYaw, lampePitch;
    unsigned char oeil, bruit, flags, batterie;           /* oeil en cm ; bruit x 100 ; batterie en % */
    unsigned short pas;                                   /* compteur de pas (sons des autres) */
    unsigned char mortType, pad;
} NetJ;
#pragma pack(pop)

typedef struct {
    int actif;
    char nom[16];
    NetJ st;                                              /* dernier etat recu */
    V3 disp;                                              /* position affichee (lissee) */
    unsigned short pasVu;                                 /* dernier pas joue */
    int utiliseVu;                                        /* hote : dernier bit "utilise" traite (front montant) */
    float criT;                                           /* son du cri deja joue recemment */
} Joueur;
static Joueur JP[MAXP];

static void netj_de_P(NetJ *s) {
    memset(s, 0, sizeof *s);
    s->x = P.pos.x; s->z = P.pos.z; s->yaw = P.yaw; s->pitch = P.pitch;
    s->lampeYaw = P.lampeYaw; s->lampePitch = P.lampePitch;
    s->oeil = (unsigned char)clampf(P.oeil * 100, 0, 255);
    s->bruit = (unsigned char)clampf(P.bruit * 100, 0, 255);
    s->batterie = (unsigned char)clampf(P.batterie / LAMPE_BATTERIE * 100, 0, 100);
    s->flags = (unsigned char)((P.lampe && P.batterie > 0 ? JF_LAMPE : 0) | (P.accroupi ? JF_ACCROUPI : 0) | (P.dead ? JF_MORT : 0) |
                               (P.piegeT > 0 ? JF_PIEGE : 0) | (act[A_UTILISER] ? JF_UTILISE : 0) | (P.court ? JF_COURT : 0) |
                               (P.bruitImpulse >= BRUIT_CRI - 0.05f ? JF_CRI : 0) | (P.cache ? JF_CACHE : 0));
    s->pas = (unsigned short)P.pas;
}

/* ---- cibles des monstres ------------------------------------------------------------------------ */
typedef struct { int actif, mort, lampe, cache; V3 pos; float oeil, bruit, lampeYaw, lampePitch; } Cible;   /* cache : sous un lit, dans une armoire */
static Cible cib[MAXP];

/* qui est dans la partie, ou, et avec quel bruit (solo : la place 0 est le joueur local) */
static void cibles_maj(void) {
    for (int i = 0; i < MAXP; i++) {
        Cible *c = &cib[i];
        memset(c, 0, sizeof *c);
        int moi = netMode == NET_OFF ? i == 0 : i == monSlot;
        if (moi) {
            c->actif = 1; c->mort = P.dead; c->pos = P.pos; c->oeil = P.oeil; c->bruit = P.bruit;
            c->lampe = P.lampe && P.batterie > 0; c->lampeYaw = P.lampeYaw; c->lampePitch = P.lampePitch; c->cache = P.cache != 0;
        } else if (netMode != NET_OFF && JP[i].actif) {
            const NetJ *s = &JP[i].st;
            c->actif = 1; c->mort = (s->flags & JF_MORT) != 0; c->pos = v3(s->x, 0, s->z); c->oeil = s->oeil / 100.0f;
            c->bruit = s->bruit / 100.0f; c->lampe = (s->flags & JF_LAMPE) != 0; c->lampeYaw = s->lampeYaw; c->lampePitch = s->lampePitch;
            c->cache = (s->flags & JF_CACHE) != 0;
        }
    }
}

/* une cible vivante au hasard (pour errer autour) ; -1 s'il n'y en a pas */
static int cible_hasard(void) {
    int v[MAXP], n = 0;
    for (int i = 0; i < MAXP; i++) if (cib[i].actif && !cib[i].mort) v[n++] = i;
    return n ? v[(int)(frand() * n) % n] : -1;
}

static int slot_local(void) { return netMode == NET_OFF ? 0 : monSlot; }
static void reseau_mort_distante(int slot, int type);     /* reseau/reseau.c : l'hote annonce la mort d'un autre joueur */
