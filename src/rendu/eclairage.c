/* BOURDONNEMENT - faces du monde et lightmaps des neons (precalcul sur le processeur, sans OpenGL : testable)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Chaque case libre donne un sol et un plafond, chaque cote qui touche un mur donne un pan de mur : un quad.
   Chaque quad a sa vignette dans un atlas de lightmaps (rangees d'etageres, bord recopie, comme VELOCITY).
   Deux couches : FIXE (neons qui ne bougent pas) et DYNAMIQUE (pour chaque quad, la part du neon fou ou de
   l'ascenseur le plus proche, ajoutee a l'image quand ce neon est allume). Valeurs stockees / 2 : le rendu
   multiplie par 2 (GL_RGB_SCALE), 0,5 = texture telle quelle, au-dessus = surexposition des neons. */

typedef struct { V3 o, u, v, n; int mat, lx, ly, lw, lh, dyn; } Face;   /* coin o, cotes u et v ; vignette (lx, ly, lw, lh) */
#define MAXFACES 70000
static Face faces[MAXFACES];
static int nfaces;
static int lmTaille = 2048, lmDens = LM_DENS;             /* cote de l'atlas, texels par metre */
static unsigned char *lmFixe, *lmDyn;                     /* atlas RGB (malloc) */
static double lmBakeMs;

static V3 up3(void) { return v3(0, 1, 0); }

static void add_face(V3 o, V3 u, V3 v, int mat) {
    if (nfaces >= MAXFACES) return;
    Face *f = &faces[nfaces++];
    f->o = o; f->u = u; f->v = v; f->n = vnorm(vcross(u, v)); f->mat = mat; f->dyn = -1;
}

/* face verticale : centre du bas en c, normale n (horizontale), largeur w, de y0 a y1 */
static void face_v(V3 c, V3 n, float w, float y0, float y1, int mat) {
    V3 u = vcross(up3(), n);                              /* (haut x n) x haut = n */
    add_face(v3(c.x - u.x * w * 0.5f, y0, c.z - u.z * w * 0.5f), vmul(u, w), v3(0, y1 - y0, 0), mat);
}

/* decors poses sur les murs (prises, grilles d'aeration) : dessines par rendu/entites.c */
enum { DECAL_PRISE, DECAL_GRILLE, DECAL_INTER };
typedef struct { V3 pos, n; int type; } Decal;
#define MAXDECAL 900
static Decal decals[MAXDECAL];
static int ndecals;
#define LINTEAU_Y 2.15f                                   /* bas des linteaux au-dessus des passages */
#define TROU_H 0.45f                                      /* profondeur des trous du plafond */

/* Sol, plafond et murs a partir de la grille. Ordre des sommets : o, o+u, o+u+v, o+v ; u x v = normale (face avant).
   En plus des murs : plinthes et corniches, linteaux au-dessus des passages etroits, cloisons basses, dalles de
   plafond arrachees (un trou noir), et la liste des prises / grilles. */
static void construire_faces(void) {
    nfaces = 0; ndecals = 0;
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            unsigned h = hash32((unsigned)(x * 92821 + z * 68917) ^ 0x51ed27u);
            if (grille[z][x] == 1) continue;
            if (grille[z][x] == 2) {                      /* cloison basse : le dessus, et le plafond au-dessus */
                add_face(v3((float)x, MURET_H, (float)z), v3(0, 0, 1), v3(1, 0, 0), MAT_CLOISON);
                add_face(v3((float)x, HAUT_PLAFOND, (float)z), v3(1, 0, 0), v3(0, 0, 1), MAT_PLAFOND);
                continue;
            }
            add_face(v3((float)x, 0, (float)z), v3(0, 0, 1), v3(1, 0, 0), MAT_MOQUETTE);
            int neonIci = 0;
            for (int i = 0; i < nneons; i++) if ((int)neons[i].x == x && (int)neons[i].z == z) neonIci = 1;
            int estAsc = asc.actif && x == asc.gx && z == asc.gz;
            if (!neonIci && !estAsc && h % 100 < 4) {        /* dalle arrachee : un trou dans le noir */
                for (int k = 0; k < 4; k++)
                    face_v(v3(x + 0.5f + cotes[k][0] * 0.5f, 0, z + 0.5f + cotes[k][1] * 0.5f), v3((float)-cotes[k][0], 0, (float)-cotes[k][1]),
                           1, HAUT_PLAFOND, HAUT_PLAFOND + TROU_H, MAT_CORNICHE);
                add_face(v3((float)x, HAUT_PLAFOND + TROU_H, (float)z), v3(1, 0, 0), v3(0, 0, 1), MAT_PLAFOND);
            } else add_face(v3((float)x, HAUT_PLAFOND, (float)z), v3(1, 0, 0), v3(0, 0, 1), MAT_PLAFOND);
            for (int k = 0; k < 4; k++) {
                int nx = x + cotes[k][0], nz = z + cotes[k][1];
                int plein = cell_opaque(nx, nz), muret = cell_muret(nx, nz);
                if (!plein && !muret) continue;
                V3 n = v3((float)-cotes[k][0], 0, (float)-cotes[k][1]);   /* le mur regarde vers la case */
                V3 c = v3(x + 0.5f + cotes[k][0] * 0.5f, 0, z + 0.5f + cotes[k][1] * 0.5f);
                V3 u = vcross(up3(), n);
                float hm = plein ? HAUT_PLAFOND : MURET_H;
                face_v(c, n, 1, 0, hm, plein ? MAT_PAPIER : MAT_CLOISON);
                V3 cp = vadd(c, vmul(n, 0.015f));         /* plinthe : face avant et dessus */
                face_v(cp, n, 1, 0, 0.11f, MAT_PLINTHE);
                add_face(v3(c.x - u.x * 0.5f, 0.11f, c.z - u.z * 0.5f), vmul(n, 0.015f), u, MAT_PLINTHE);
                if (plein) {                              /* corniche : face avant et dessous */
                    V3 cc = vadd(c, vmul(n, 0.022f));
                    face_v(cc, n, 1, HAUT_PLAFOND - 0.09f, HAUT_PLAFOND, MAT_CORNICHE);
                    add_face(v3(c.x - u.x * 0.5f, HAUT_PLAFOND - 0.09f, c.z - u.z * 0.5f), u, vmul(n, 0.022f), MAT_CORNICHE);
                    unsigned hd = hash32(h ^ (unsigned)(k * 7919));
                    float off = ((hd >> 8) % 60) / 100.0f - 0.3f;
                    if (hd % 100 < 9 && ndecals < MAXDECAL)  /* prise electrique */
                        decals[ndecals++] = (Decal){ vadd(vadd(c, vmul(u, off)), v3(n.x * 0.006f, 0.3f, n.z * 0.006f)), n, DECAL_PRISE };
                    else if (hd % 100 < 13 && ndecals < MAXDECAL)   /* grille d'aeration, sous le plafond */
                        decals[ndecals++] = (Decal){ vadd(vadd(c, vmul(u, off * 0.5f)), v3(n.x * 0.008f, 2.5f, n.z * 0.008f)), n, DECAL_GRILLE };
                    else if (hd % 100 < 15 && ndecals < MAXDECAL)   /* interrupteur */
                        decals[ndecals++] = (Decal){ vadd(vadd(c, vmul(u, off)), v3(n.x * 0.006f, 1.25f, n.z * 0.006f)), n, DECAL_INTER };
                }
            }
            /* linteau : passage d'une case de large qui debouche sur une salle (on croit franchir une porte) */
            for (int k = 0; k < 4; k++) {
                int px = cotes[k][1], pz = cotes[k][0];     /* perpendiculaire */
                if (!cell_opaque(x + px, z + pz) || !cell_opaque(x - px, z - pz)) continue;
                int nx = x + cotes[k][0], nz = z + cotes[k][1];
                if (cell_mur(nx, nz) || (asc.actif && nx == asc.gx && nz == asc.gz) || estAsc) continue;
                if (cell_opaque(nx + px, nz + pz) && cell_opaque(nx - px, nz - pz)) continue;   /* toujours un couloir : pas de porte */
                V3 d = v3((float)cotes[k][0], 0, (float)cotes[k][1]);
                V3 e = v3(x + 0.5f + d.x * 0.5f, 0, z + 0.5f + d.z * 0.5f);
                face_v(vadd(e, vmul(d, 0.06f)), d, 1, LINTEAU_Y, HAUT_PLAFOND, MAT_PAPIER);           /* cote salle */
                face_v(vsub(e, vmul(d, 0.06f)), vmul(d, -1), 1, LINTEAU_Y, HAUT_PLAFOND, MAT_PAPIER); /* cote couloir */
                V3 u = vcross(up3(), d), o = v3(e.x - d.x * 0.06f - u.x * 0.5f, LINTEAU_Y, e.z - d.z * 0.06f - u.z * 0.5f);
                add_face(o, u, vmul(d, 0.12f), MAT_CORNICHE);   /* dessous */
            }
        }
}

/* occlusion des coins : sol/plafond pres des murs, murs pres du sol, du plafond et des coins rentrants */
static float occlusion(const Face *f, V3 p) {
    float ao = 1;
    int gx = (int)floorf(p.x - f->n.x * 0.01f), gz = (int)floorf(p.z - f->n.z * 0.01f);
    if (fabsf(f->n.y) > 0.5f) {
        float fx = p.x - gx, fz = p.z - gz;
        float dmur[4] = { 1 - fx, fx, 1 - fz, fz };       /* distance aux cotes +x, -x, +z, -z */
        for (int k = 0; k < 4; k++) if (cell_mur(gx + cotes[k][0], gz + cotes[k][1])) ao *= 1 - 0.42f * expf(-dmur[k] * 7);
        float sol = f->n.y > 0 ? 1.0f : 0.7f;
        ao = 1 - (1 - ao) * sol;
    } else {
        ao *= (1 - 0.5f * expf(-p.y * 6)) * (1 - 0.3f * expf(-(HAUT_PLAFOND - p.y) * 8));
        V3 uu = vnorm(f->u);
        float s = vdot(vsub(p, f->o), uu);                /* 0..1 le long du pan */
        gx = (int)floorf(p.x + f->n.x * 0.5f); gz = (int)floorf(p.z + f->n.z * 0.5f);   /* la case devant le mur */
        int ax = (int)roundf(uu.x), az = (int)roundf(uu.z);
        if (cell_mur(gx + ax, gz + az)) ao *= 1 - 0.4f * expf(-(1 - s) * 7);
        if (cell_mur(gx - ax, gz - az)) ao *= 1 - 0.4f * expf(-s * 7);
    }
    return ao;
}

/* lumiere recue en p (normale n) des neons de la liste ; seul >= 0 : uniquement ce neon (couche dynamique) */
static void lum_point(V3 p, V3 n, const short *cand, int nc, int seul, float out[3]) {
    float direct = 0, rebond = 0;
    V3 po = vadd(p, vmul(n, 0.04f));
    for (int c = 0; c < nc; c++) {
        int i = cand[c];
        const Neon *ne = &neons[i];
        if (seul >= 0 ? i != seul : (ne->etat != NEON_OK)) continue;
        float dx = ne->x - p.x, dz = ne->z - p.z, dxz2 = dx * dx + dz * dz;
        if (dxz2 > NEON_PORTEE * NEON_PORTEE) continue;
        if (!visible_xz(po.x, po.z, ne->x, ne->z)) continue;
        float fade = 1 - sqrtf(dxz2) / NEON_PORTEE;
        float I = NEON_I * (ne->etat == NEON_ASC ? 1.3f : 1.0f);
        for (int s = -1; s <= 1; s += 2) {                /* le tube : deux points le long de x */
            V3 L = vsub(v3(ne->x + s * 0.35f, HAUT_PLAFOND - 0.02f, ne->z), p);
            float d2 = vdot(L, L), d = sqrtf(d2);
            if (d < 1e-3f) continue;
            float cs = vdot(n, L) / d, ce = L.y / d;
            if (cs <= 0 || ce <= 0) continue;
            direct += I * 0.5f * cs * (0.2f + 0.8f * ce) / (d2 * 0.35f + 0.5f) * fade;
        }
        rebond += I * HAUT_PLAFOND / (dxz2 + HAUT_PLAFOND * HAUT_PLAFOND) * fade;
    }
    float rk = REBOND_K * (n.y < -0.5f ? 1.0f : n.y > 0.5f ? 0.35f : 0.6f);
    float amb = seul >= 0 ? 0 : AMBIANCE_MIN;
    out[0] = direct * 1.00f + rebond * rk * 0.95f + amb;
    out[1] = direct * 0.97f + rebond * rk * 0.86f + amb * 0.95f;
    out[2] = direct * 0.84f + rebond * rk * 0.55f + amb * 0.8f;
}

/* neons assez proches de la face (centre c) : candidats pour tous ses texels */
static int candidats(V3 c, short *out, int cap) {
    int n = 0;
    float r = NEON_PORTEE + 2.0f;
    for (int i = 0; i < nneons && n < cap; i++) {
        float dx = neons[i].x - c.x, dz = neons[i].z - c.z;
        if (dx * dx + dz * dz < r * r) out[n++] = (short)i;
    }
    return n;
}

static void lm_ecrire(unsigned char *atlas, int x, int y, const float c[3]) {
    unsigned char *q = atlas + ((size_t)y * lmTaille + x) * 3;
    for (int k = 0; k < 3; k++) q[k] = (unsigned char)(clampf(c[k] * 0.5f, 0, 1) * 255.0f + 0.5f);
}

/* place les vignettes dans l'atlas ; renvoie 0 si ca ne tient pas */
static int lm_ranger(int dens) {
    int x = 0, y = 0, hRang = 0;
    for (int i = 0; i < nfaces; i++) {
        Face *f = &faces[i];
        f->lw = (int)ceilf(vlen(f->u) * dens - 0.01f); f->lh = (int)ceilf(vlen(f->v) * dens - 0.01f);
        if (x + f->lw + 2 > lmTaille) { x = 0; y += hRang; hRang = 0; }
        if (y + f->lh + 2 > lmTaille) return 0;
        f->lx = x + 1; f->ly = y + 1;
        x += f->lw + 2;
        if (f->lh + 2 > hRang) hRang = f->lh + 2;
    }
    return 1;
}

static void bake_lightmaps(void) {
    LARGE_INTEGER t0, t1, fq;
    QueryPerformanceCounter(&t0); QueryPerformanceFrequency(&fq);
    construire_faces();
    for (lmDens = LM_DENS; lmDens > 2 && !lm_ranger(lmDens); lmDens -= 2) {}
    size_t sz = (size_t)lmTaille * lmTaille * 3;
    if (!lmFixe) { lmFixe = (unsigned char *)malloc(sz); lmDyn = (unsigned char *)malloc(sz); }
    memset(lmFixe, 0, sz); memset(lmDyn, 0, sz);
    short cand[256];
    for (int i = 0; i < nfaces; i++) {
        Face *f = &faces[i];
        V3 centre = vadd(f->o, vadd(vmul(f->u, 0.5f), vmul(f->v, 0.5f)));
        int nc = candidats(centre, cand, 256);
        float bd = 1e9f;                                  /* neon dynamique le plus proche (visible) */
        for (int c = 0; c < nc; c++) {
            int k = cand[c];
            if (!neon_dynamique(k)) continue;
            float dx = neons[k].x - centre.x, dz = neons[k].z - centre.z, d = dx * dx + dz * dz;
            V3 po = vadd(centre, vmul(f->n, 0.05f));
            if (d < bd && d < NEON_PORTEE * NEON_PORTEE && visible_xz(po.x, po.z, neons[k].x, neons[k].z)) { bd = d; f->dyn = k; }
        }
        for (int ty = -1; ty <= f->lh; ty++)              /* bord recopie : on calcule aussi une rangee autour */
            for (int tx = -1; tx <= f->lw; tx++) {
                float su = clampf((tx + 0.5f) / f->lw, 0, 1), sv = clampf((ty + 0.5f) / f->lh, 0, 1);
                V3 p = vadd(f->o, vadd(vmul(f->u, su), vmul(f->v, sv)));
                float ao = occlusion(f, p), c[3];
                lum_point(p, f->n, cand, nc, -1, c);
                for (int k = 0; k < 3; k++) c[k] *= ao;
                lm_ecrire(lmFixe, f->lx + tx, f->ly + ty, c);
                if (f->dyn >= 0) {
                    lum_point(p, f->n, cand, nc, f->dyn, c);
                    for (int k = 0; k < 3; k++) c[k] *= ao;
                    lm_ecrire(lmDyn, f->lx + tx, f->ly + ty, c);
                }
            }
        if (f->n.y > 0.5f) {                              /* sol : eclairement moyen de la case */
            float c[3];
            lum_point(vadd(centre, v3(0, 0.6f, 0)), v3(0, 1, 0), cand, nc, -1, c);
            lumCase[(int)floorf(centre.z)][(int)floorf(centre.x)] = (c[0] + c[1] + c[2]) / 3;
        }
    }
    QueryPerformanceCounter(&t1);
    lmBakeMs = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)fq.QuadPart;
}
