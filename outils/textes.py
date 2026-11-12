#!/usr/bin/env python3
# Genere src/jeu/textes.c : tous les textes du jeu ecrits ici en francais normal (UTF-8), convertis en chaines C
# CP1252 (la police du jeu) avec des \xNN. Lancer : python3 outils/textes.py  (depuis la racine du projet)
import os

INTRO = """Le 2 mars 1998, le centre commercial des Quatre Vents a fermé ses portes
après une « fuite de gaz » au sous-sol.

Quatorze employés de la société Dalmont ont été déclarés disparus.
L'enquête a été classée sans suite.

Le 14 octobre 2003, Noé Marchal y est entré pour filmer.
Il n'est jamais ressorti.

Trois semaines plus tard, tu as retrouvé son caméscope.
Sur la dernière cassette, Noé pose la main sur un mur du sous-sol...
et passe au travers.

Cette nuit, tu as posé la main au même endroit."""

NIVEAUX = [
("LE HALL JAUNE", """Des couloirs jaunes. De la moquette mouillée. Et ce bourdonnement.
Quelque part, quelque chose marche. Ses pas sont lourds.

Quelqu'un a gratté sur un mur, avec les ongles :
« IL N'A PAS D'YEUX. IL ENTEND TOUT. NE COURS PAS. »

Trouve trois fusibles. Rallume l'ascenseur.
Méfie-toi de l'eau et du verre : ils parlent pour toi."""),
("LES BUREAUX MORTS", """« Six jours », a dit la voix à la radio.
Le centre a fermé il y a cinq ans.

Une note est punaisée à une porte :
« IL NE BOUGE QUE DANS LE NOIR.
  TANT QUE TU LE REGARDES, IL ATTEND.
  GARDE TA LUMIÈRE SUR LUI. »

Ta lampe ne durera pas éternellement."""),
("LE SOUS-SOL HUMIDE", """L'eau monte jusqu'aux chevilles. Chaque pas résonne.

Cassette de Noé, numéro 3 :
« Elle rampe. Elle a des yeux énormes. Elle adore la lumière.
  J'ai éteint ma lampe et elle est passée à côté de moi.
  Je l'entendais respirer contre ma joue. »

Dans le noir, ses yeux brillent. Les tiens, non."""),
("LES COULOIRS SANS FIN", """La radio grésille. La voix de Varga glisse...
puis devient celle de Noé... puis la tienne.

Ce n'était pas elle. Ça n'a jamais été elle.

Ils sont deux, maintenant. L'un écoute. L'autre cherche ta lumière.
Tais-toi. Et éteins."""),
("LES ARCHIVES", """Des classeurs. Des milliers. Chaque dossier porte un nom.
Tu trouves le tien. Date d'entrée : demain.

« IL NE BOUGE QUE DANS LE NOIR. »
« IL ENTEND TOUT. »

Les deux à la fois. La lumière te sauve de l'un.
Le silence te sauve de l'autre. Rien ne te sauve des deux."""),
("NIVEAU %d", """ON EST TOUJOURS 14.
ON SERA TOUJOURS 14.

Ils sont tous là. Tous les trois.
Ils savent ton nom."""),
]

RADIO = [
("RADIO : ...allô ? Y a quelqu'un ? Je m'appelle Hélène Varga. Ne faites aucun bruit.",
 "RADIO : Bien. Encore deux. Il est près de vous... je l'entends respirer.",
 "RADIO : L'ascenseur a du courant. Courez, maintenant. Il arrive."),
("RADIO : Il est réveillé. Ne le quittez pas des yeux.",
 "RADIO : Votre lampe faiblit. Lui, il ne se fatigue jamais.",
 "RADIO : C'est bien... continuez à descendre."),
("RADIO : Éteignez. Votre. Lampe.",
 "RADIO : Elle vous a senti. Ne bougez plus.",
 "RADIO : Descendez encore. Je vous attends en bas."),
("RADIO : (ta propre voix) ...allô ? Y a quelqu'un ?",
 "RADIO : Tu entends ? Il y en a un qui écoute, et une qui regarde.",
 "RADIO : Plus que toi. Plus que nous."),
("RADIO : Quatorze.", "RADIO : Quinze.", "RADIO : Seize."),
]

# cassettes de Noe : trouvees dans ses campements (niveaux 2 a 6), regardees sur son camescope
CASSETTES = [
("CASSETTE DE NOE - 1", """(l'image tremble, une moquette jaune, la respiration de Noe)

« Jour... deux, je crois. Y a pas de nuit ici.
  Les lumieres s'eteignent jamais. Enfin, si. Parfois.

  Et quand elles s'eteignent...

  (il tourne la camera vers le fond du couloir : quelque chose, tres grand)

  ... faut pas regarder ailleurs. »""",
 "CASSETTE 1 - Noe : « Quand les lumieres s'eteignent... faut pas regarder ailleurs. »"),
("CASSETTE DE NOE - 2", """(un bureau, une radio posee sur une chaise)

« J'ai trouve une radio. Y a une femme dessus. Helene.
  Elle est gentille. Elle m'a dit ou aller.

  (il chuchote)

  ... Mais elle connait mon prenom.
  Je lui ai jamais dit mon prenom. »""",
 "CASSETTE 2 - Noe : « La femme de la radio connait mon prenom. Je lui ai jamais dit. »"),
("CASSETTE DE NOE - 3", """(filme depuis l'interieur d'une armoire, par la fente)

(quelque chose respire contre la porte)

« ... elle m'appelle.
  Avec la voix de maman.

  Me cachez pas sous le lit. Elle regarde toujours sous le lit. »""",
 "CASSETTE 3 - Noe, cache dans une armoire : « Elle m'appelle avec la voix de maman. »"),
("CASSETTE DE NOE - 4", """(une echelle de service, de l'eau qui goutte)

« Si la radio vous dit de descendre, montez.
  Si elle vous dit de rester ensemble...

  (long silence)

  ... elle a raison, pour une fois.
  Je vous attends en haut. La ou tout a commence. »""",
 "CASSETTE 4 - Noe : « Si la radio vous dit de descendre, montez. Je vous attends en haut. »"),
("CASSETTE DE NOE - 5", """(Noe, maigre, barbu, il a vieilli de dix ans)

« Je l'ai depuis le debut. La cle de service.
  Mais elle n'ouvre que de l'exterieur.

  Et de dedans... il faut que quelqu'un tienne la porte.

  Je suis desole. »""",
 "CASSETTE 5 - Noe : « La cle n'ouvre que de dehors. Quelqu'un devra tenir la porte. »"),
]

MORTS = ["IL T'A ENTENDU.", "TU L'AS LAISSÉ DANS LE NOIR.", "ELLE A VU TA LUMIÈRE."]
CONSEILS = ["Il n'entend que le bruit : accroupis-toi (Ctrl), évite l'eau et le verre.",
            "Il n'avance que dans le noir : garde ta lampe ou un néon sur lui.",
            "Elle fonce sur la lumière : éteins ta lampe (F) et ne bouge plus."]

def c(s):
    out, prev_esc = '"', False
    for ch in s:
        b = ch.encode('cp1252')
        for byte in b:
            if byte >= 128:
                out += '\\x%02X' % byte; prev_esc = True; continue
            cc = chr(byte)
            if prev_esc and cc in '0123456789abcdefABCDEF': out += '" "'
            prev_esc = False
            out += {'"': '\\"', '\\': '\\\\'}.get(cc, cc)
    return out + '"'

def tab(nom, lignes):
    return 'static const char *const %s[] = {\n%s    NULL };\n' % (nom, ''.join('    %s,\n' % c(l) for l in lignes))

src = ['/* BOURDONNEMENT - textes du jeu (GENERE par outils/textes.py : modifier le script, pas ce fichier)\n'
       '   Morceau de main.c : inclus dans l\'ordre, ne se compile pas seul (voir main.c). */\n']
src.append(tab('texteIntro', INTRO.split('\n')))
for i, (titre, txt) in enumerate(NIVEAUX):
    src.append(tab('texteNiveau%d' % i, txt.split('\n')))
src.append('static const char *const *const texteNiveaux[%d] = { %s };\n' % (len(NIVEAUX), ', '.join('texteNiveau%d' % i for i in range(len(NIVEAUX)))))
src.append('static const char *const titreNiveaux[%d] = { %s };\n' % (len(NIVEAUX), ', '.join(c(t) for t, _ in NIVEAUX)))
src.append('#define NTEXTES_NIVEAUX %d\n' % len(NIVEAUX))
src.append('static const char *const texteRadio[%d][3] = {\n%s};\n' % (len(RADIO), ''.join('    { %s },\n' % ', '.join(c(x) for x in r) for r in RADIO)))
src.append('#define NTEXTES_RADIO %d\n' % len(RADIO))
for i, (titre, txt, court) in enumerate(CASSETTES):
    src.append(tab('texteCassette%d' % i, txt.split('\n')))
src.append('static const char *const *const texteCassettes[%d] = { %s };\n' % (len(CASSETTES), ', '.join('texteCassette%d' % i for i in range(len(CASSETTES)))))
src.append('static const char *const titreCassettes[%d] = { %s };\n' % (len(CASSETTES), ', '.join(c(t) for t, _, _ in CASSETTES)))
src.append('static const char *const texteCassetteCourt[%d] = { %s };\n' % (len(CASSETTES), ', '.join(c(k) for _, _, k in CASSETTES)))
src.append('#define NTEXTES_CASSETTES %d\n' % len(CASSETTES))
src.append('static const char *const texteMort[3] = { %s };\n' % ', '.join(c(x) for x in MORTS))
src.append('static const char *const texteConseil[3] = { %s };\n' % ', '.join(c(x) for x in CONSEILS))
racine = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(racine, 'src', 'jeu', 'textes.c'), 'w', encoding='ascii').write('\n'.join(src))
print('src/jeu/textes.c ecrit')
