/* BOURDONNEMENT - police, texte et rectangles 2D (repris de VELOCITY)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void make_font(int i, int px) {
#ifndef _WIN32
    fontBase[i] = glGenLists(224);
    mac_make_font(fontBase[i], px, &fontW[i], &fontH[i]);
#else
    HFONT f = CreateFontA(-px, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ old = SelectObject(hdc, f);
    fontBase[i] = glGenLists(224);
    if (!wglUseFontBitmapsA(hdc, 32, 224, fontBase[i])) wglUseFontBitmapsA(hdc, 32, 224, fontBase[i]);
    TEXTMETRICA tm; GetTextMetricsA(hdc, &tm);
    fontW[i] = tm.tmAveCharWidth; fontH[i] = tm.tmHeight;
    SelectObject(hdc, old);
    DeleteObject(f);
#endif
}

/* police en texture : les glyphes bitmap (glBitmap, tres lent sur certains pilotes, dont macOS) sont dessines une
 * seule fois au demarrage puis relus dans un atlas ; chaque caractere devient un quad texture, au pixel pres */
#define FONT_PAD 4                                        /* marge autour d'un glyphe (debordements du gras) */
static GLuint fontTex[3];
static int fontCW[3], fontCH[3], fontAsc[3], fontAW[3], fontAH[3];   /* cellule, hauteur au-dessus de la ligne, atlas */
static void make_font_atlas(int f) {
    int cw = fontW[f] + 2 * FONT_PAD, ch = (int)(fontH[f] * 1.6f) + 2 * FONT_PAD, asc = (int)(fontH[f] * 1.2f) + FONT_PAD;
    int aw = pow2up(16 * cw), ah = pow2up(14 * ch), perRow = winW / cw, perCol = winH / ch;
    if (perRow < 1 || perCol < 1) return;
    fontCW[f] = cw; fontCH[f] = ch; fontAsc[f] = asc; fontAW[f] = aw; fontAH[f] = ah;
    unsigned char *atlas = (unsigned char *)calloc((size_t)aw * ah, 1), *rgb = (unsigned char *)malloc((size_t)winW * winH * 3);
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, winW, winH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    for (int start = 0; start < 224; start += perRow * perCol) {   /* par lots : la fenetre peut etre petite (bots) */
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(1, 1, 1);
        for (int g = start; g < 224 && g < start + perRow * perCol; g++) {
            int k = g - start;
            glRasterPos2f((float)((k % perRow) * cw + FONT_PAD), (float)((k / perRow) * ch + asc));
            glCallList(fontBase[f] + (GLuint)g);
        }
        glFinish();
        glReadPixels(0, 0, winW, winH, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        for (int g = start; g < 224 && g < start + perRow * perCol; g++) {
            int k = g - start, sx = (k % perRow) * cw, sy = (k / perRow) * ch, ax = (g % 16) * cw, ay = (g / 16) * ch;
            for (int r = 0; r < ch; r++)                  /* lignes relues : de bas en haut */
                for (int c = 0; c < cw; c++)
                    atlas[(size_t)(ay + r) * aw + ax + c] = rgb[((size_t)(winH - 1 - (sy + r)) * winW + sx + c) * 3];
        }
    }
    glGenTextures(1, &fontTex[f]);
    glBindTexture(GL_TEXTURE_2D, fontTex[f]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, aw, ah, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas);
    free(atlas); free(rgb);
    glEnable(GL_DEPTH_TEST);
}

static void text_quads(int f, float x, float y, const char *s) {
    float cw = (float)fontCW[f], ch = (float)fontCH[f], iw = 1.0f / fontAW[f], ih = 1.0f / fontAH[f];
    float px = floorf(x + 0.5f) - FONT_PAD, py = floorf(y + 0.5f) - fontAsc[f];
    for (; *s; s++, px += fontW[f]) {
        int g = (unsigned char)*s - 32;
        if (g <= 0) continue;                             /* espace (ou caractere de controle) : rien a dessiner */
        float u0 = (g % 16) * cw * iw, v0 = (g / 16) * ch * ih, u1 = u0 + cw * iw, v1 = v0 + ch * ih;
        glTexCoord2f(u0, v0); glVertex2f(px, py);
        glTexCoord2f(u1, v0); glVertex2f(px + cw, py);
        glTexCoord2f(u1, v1); glVertex2f(px + cw, py + ch);
        glTexCoord2f(u0, v1); glVertex2f(px, py + ch);
    }
}

static void text(int f, float x, float y, const char *s, float r, float g, float b, float a) {
    if (!fontTex[f]) {                                    /* atlas absent (fenetre trop petite) : glyphes bitmap */
        int n = (int)strlen(s);
        glColor4f(0, 0, 0, a * 0.7f);
        glRasterPos2f(x + 2, y + 2);
        glListBase(fontBase[f] - 32);
        glCallLists(n, GL_UNSIGNED_BYTE, s);
        glColor4f(r, g, b, a);
        glRasterPos2f(x, y);
        glCallLists(n, GL_UNSIGNED_BYTE, s);
        return;
    }
    GLboolean blend = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fontTex[f]);
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, a * 0.7f);                         /* ombre portee, puis le texte */
    text_quads(f, x + 2, y + 2, s);
    glColor4f(r, g, b, a);
    text_quads(f, x, y, s);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    if (!blend) glDisable(GL_BLEND);
}
static void text_c(int f, float cx, float y, const char *s, float r, float g, float b, float a) {
    text(f, cx - fontW[f] * (float)strlen(s) * 0.5f, y, s, r, g, b, a);
}
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
/* meme chose, mais alpha different en haut et en bas (ex : assombrir vers le haut, laisser voir le
 * decor vers le bas) */
static void rect_vgrad(float x, float y, float w, float h, float r, float g, float b, float aTop, float aBot) {
    glBegin(GL_QUADS);
    glColor4f(r, g, b, aTop); glVertex2f(x, y); glVertex2f(x + w, y);
    glColor4f(r, g, b, aBot); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
