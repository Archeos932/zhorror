/* BOURDONNEMENT - textes du jeu (GENERE par outils/textes.py : modifier le script, pas ce fichier)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static const char *const texteIntro[] = {
    "Le 2 mars 1998, le centre commercial des Quatre Vents a ferm\xE9 ses portes",
    "apr\xE8s une \xAB fuite de gaz \xBB au sous-sol.",
    "",
    "Quatorze employ\xE9s de la soci\xE9t\xE9 Dalmont ont \xE9t\xE9 d\xE9" "clar\xE9s disparus.",
    "L'enqu\xEAte a \xE9t\xE9 class\xE9" "e sans suite.",
    "",
    "Le 14 octobre 2003, No\xE9 Marchal y est entr\xE9 pour filmer.",
    "Il n'est jamais ressorti.",
    "",
    "Trois semaines plus tard, tu as retrouv\xE9 son cam\xE9scope.",
    "Sur la derni\xE8re cassette, No\xE9 pose la main sur un mur du sous-sol...",
    "et passe au travers.",
    "",
    "Cette nuit, tu as pos\xE9 la main au m\xEAme endroit.",
    NULL };

static const char *const texteNiveau0[] = {
    "Des couloirs jaunes. De la moquette mouill\xE9" "e. Et ce bourdonnement.",
    "Quelque part, quelque chose marche. Ses pas sont lourds.",
    "",
    "Quelqu'un a gratt\xE9 sur un mur, avec les ongles :",
    "\xAB IL N'A PAS D'YEUX. IL ENTEND TOUT. NE COURS PAS. \xBB",
    "",
    "Trouve trois fusibles. Rallume l'ascenseur.",
    "M\xE9" "fie-toi de l'eau et du verre : ils parlent pour toi.",
    NULL };

static const char *const texteNiveau1[] = {
    "\xAB Six jours \xBB, a dit la voix \xE0 la radio.",
    "Le centre a ferm\xE9 il y a cinq ans.",
    "",
    "Une note est punais\xE9" "e \xE0 une porte :",
    "\xAB IL NE BOUGE QUE DANS LE NOIR.",
    "  TANT QUE TU LE REGARDES, IL ATTEND.",
    "  GARDE TA LUMI\xC8RE SUR LUI. \xBB",
    "",
    "Ta lampe ne durera pas \xE9ternellement.",
    NULL };

static const char *const texteNiveau2[] = {
    "L'eau monte jusqu'aux chevilles. Chaque pas r\xE9sonne.",
    "",
    "Cassette de No\xE9, num\xE9ro 3 :",
    "\xAB Elle rampe. Elle a des yeux \xE9normes. Elle adore la lumi\xE8re.",
    "  J'ai \xE9teint ma lampe et elle est pass\xE9" "e \xE0 c\xF4t\xE9 de moi.",
    "  Je l'entendais respirer contre ma joue. \xBB",
    "",
    "Dans le noir, ses yeux brillent. Les tiens, non.",
    NULL };

static const char *const texteNiveau3[] = {
    "La radio gr\xE9sille. La voix de Varga glisse...",
    "puis devient celle de No\xE9... puis la tienne.",
    "",
    "Ce n'\xE9tait pas elle. \xC7" "a n'a jamais \xE9t\xE9 elle.",
    "",
    "Ils sont deux, maintenant. L'un \xE9" "coute. L'autre cherche ta lumi\xE8re.",
    "Tais-toi. Et \xE9teins.",
    NULL };

static const char *const texteNiveau4[] = {
    "Des classeurs. Des milliers. Chaque dossier porte un nom.",
    "Tu trouves le tien. Date d'entr\xE9" "e : demain.",
    "",
    "\xAB IL NE BOUGE QUE DANS LE NOIR. \xBB",
    "\xAB IL ENTEND TOUT. \xBB",
    "",
    "Les deux \xE0 la fois. La lumi\xE8re te sauve de l'un.",
    "Le silence te sauve de l'autre. Rien ne te sauve des deux.",
    NULL };

static const char *const texteNiveau5[] = {
    "ON EST TOUJOURS 14.",
    "ON SERA TOUJOURS 14.",
    "",
    "Ils sont tous l\xE0. Tous les trois.",
    "Ils savent ton nom.",
    NULL };

static const char *const *const texteNiveaux[6] = { texteNiveau0, texteNiveau1, texteNiveau2, texteNiveau3, texteNiveau4, texteNiveau5 };

static const char *const titreNiveaux[6] = { "LE HALL JAUNE", "LES BUREAUX MORTS", "LE SOUS-SOL HUMIDE", "LES COULOIRS SANS FIN", "LES ARCHIVES", "NIVEAU %d" };

#define NTEXTES_NIVEAUX 6

static const char *const texteRadio[5][3] = {
    { "RADIO : ...all\xF4 ? Y a quelqu'un ? Je m'appelle H\xE9l\xE8ne Varga. Ne faites aucun bruit.", "RADIO : Bien. Encore deux. Il est pr\xE8s de vous... je l'entends respirer.", "RADIO : L'ascenseur a du courant. Courez, maintenant. Il arrive." },
    { "RADIO : Il est r\xE9veill\xE9. Ne le quittez pas des yeux.", "RADIO : Votre lampe faiblit. Lui, il ne se fatigue jamais.", "RADIO : C'est bien... continuez \xE0 descendre." },
    { "RADIO : \xC9teignez. Votre. Lampe.", "RADIO : Elle vous a senti. Ne bougez plus.", "RADIO : Descendez encore. Je vous attends en bas." },
    { "RADIO : (ta propre voix) ...all\xF4 ? Y a quelqu'un ?", "RADIO : Tu entends ? Il y en a un qui \xE9" "coute, et une qui regarde.", "RADIO : Plus que toi. Plus que nous." },
    { "RADIO : Quatorze.", "RADIO : Quinze.", "RADIO : Seize." },
};

#define NTEXTES_RADIO 5

static const char *const texteMort[3] = { "IL T'A ENTENDU.", "TU L'AS LAISS\xC9 DANS LE NOIR.", "ELLE A VU TA LUMI\xC8RE." };

static const char *const texteConseil[3] = { "Il n'entend que le bruit : accroupis-toi (Ctrl), \xE9vite l'eau et le verre.", "Il n'avance que dans le noir : garde ta lampe ou un n\xE9on sur lui.", "Elle fonce sur la lumi\xE8re : \xE9teins ta lampe (F) et ne bouge plus." };
