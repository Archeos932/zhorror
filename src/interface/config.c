/* BOURDONNEMENT - bourdonnement.ini : reglages du joueur (volume, musique, souris, micro, flashs, plein ecran,
   mise a jour automatique). Une ligne cle=valeur par reglage ; une cle inconnue est ignoree.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Windows : a cote de l'exe ; macOS : ~/Library/Application Support/Bourdonnement ; Linux : ~/.config/bourdonnement.
   Les modes --test et --shot ne le lisent ni ne l'ecrivent. */

static void cfg_save(void) {
    if (!cfgPath[0] || shotMode) return;
    FILE *f = fopen(cfgPath, "w");
    if (!f) return;
    fprintf(f, "volume=%.2f\nmusique=%.2f\nsensibilite=%.5f\ninverser_y=%d\nmicro=%d\nmicro_sensibilite=%.2f\n"
               "flashs=%d\nplein_ecran=%d\nmise_a_jour_auto=%d\nnom=%s\n",
            masterVol, ambVol, sens, invertY, micOn, micGain, flashsOn, fullscreen, updCheck, monNom);
    if (strcmp(serveur, SERVEUR_DEFAUT)) fprintf(f, "serveur=%s\n", serveur);   /* le defaut peut changer avec une mise a jour */
    fclose(f);
}

/* renvoie 1 si le plein ecran est demande (a appliquer apres la creation de la fenetre) */
static int cfg_load(void) {
    if (!cfgPath[0]) return 0;
    FILE *f = fopen(cfgPath, "r");
    if (!f) return 0;
    char l[160];
    int pe = 0;
    while (fgets(l, sizeof l, f)) {
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = 0;
        float v = (float)atof(eq + 1);
        if (!strcmp(l, "volume")) masterVol = clampf(v, 0, 1.5f);
        else if (!strcmp(l, "musique")) ambVol = clampf(v, 0, 1.5f);
        else if (!strcmp(l, "sensibilite")) sens = clampf(v, 0.0003f, 0.01f);
        else if (!strcmp(l, "inverser_y")) invertY = v != 0;
        else if (!strcmp(l, "micro")) micOn = v != 0;
        else if (!strcmp(l, "micro_sensibilite")) micGain = clampf(v, 0.5f, 12);
        else if (!strcmp(l, "flashs")) flashsOn = v != 0;
        else if (!strcmp(l, "plein_ecran")) pe = v != 0;
        else if (!strcmp(l, "mise_a_jour_auto")) updCheck = v != 0;
        else if (!strcmp(l, "nom") || !strcmp(l, "serveur")) {
            char *e = eq + 1;
            e[strcspn(e, "\r\n ")] = 0;
            if (!strcmp(l, "nom") && e[0]) snprintf(monNom, sizeof monNom, "%s", e);
            else if (e[0]) snprintf(serveur, sizeof serveur, "%s", e);
        }
    }
    fclose(f);
    return pe;
}
