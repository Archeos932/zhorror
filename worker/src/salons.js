// Logique des salons de BOURDONNEMENT, sans aucune dependance : utilisee telle quelle par le Durable Object
// (Cloudflare, index.js) et par dev-server.js (tests en local).
//
// Une "connexion" est un objet { send(texte), meta } ; meta est un objet simple que le Durable Object garde dans
// l'attachement du WebSocket (il survit a l'hibernation) :
//   meta = { id, nom, code: salon|null, place: 0..max-1, hote: bool, max, ver, enJeu }
// Les informations du salon (max, ver, enJeu) sont dans la meta de l'hote (place 0).
//
// Lignes recues (client -> serveur) :
//   nom <pseudo>                    liste <ver>                   creer <max> <ver>
//   rejoindre <CODE> <ver>          quitter                       lancer (hote)          ping
//   d <b64>          (joueur -> hote : il recoit "d <place> <b64>")
//   d <place|*> <b64> (hote -> un joueur ou tous : ils recoivent "d <b64>")
// Lignes envoyees : bonjour <id> | salon <CODE> <n> <max> <attente|jeu> <hote> ... fin | cree <CODE> |
//   rejoint <CODE> <place> | membres <place>:<nom> ... | lance | refuse <raison> | ferme | pong

export const MAX_LIGNE = 200;
export const MAX_RELAIS = 2048;
const B64_RE = /^[A-Za-z0-9+/=]{1,2000}$/;
const NOM_RE = /^[\w.\-]{1,15}$/;
const CODE_RE = /^[A-Z]{4}$/;
const LETTRES = 'ABCDEFGHJKLMNPQRSTUVWXYZ';                // sans I ni O (lus 1 et 0)
const MAXP = 4;

export function newMeta(id) {
  return { id, nom: 'joueur', code: null, place: 0, hote: false, max: MAXP, ver: 0, enJeu: false };
}

function membres(conns, code) {
  return conns.filter((c) => c.meta.code === code).sort((a, b) => a.meta.place - b.meta.place);
}
function hoteDe(conns, code) {
  return conns.find((c) => c.meta.code === code && c.meta.hote);
}
function annoncerMembres(conns, code) {
  const ms = membres(conns, code);
  const ligne = 'membres ' + ms.map((c) => `${c.meta.place}:${c.meta.nom}`).join(' ');
  for (const c of ms) c.send(ligne);
}
function nouveauCode(conns, hasard) {
  for (let essai = 0; essai < 50; essai++) {
    let code = '';
    for (let i = 0; i < 4; i++) code += LETTRES[Math.floor(hasard() * LETTRES.length)];
    if (!hoteDe(conns, code)) return code;
  }
  return null;
}

// quitte le salon courant (sans fermer la connexion)
function quitter(conns, c) {
  const m = c.meta, code = m.code;
  if (!code) return;
  const etaitHote = m.hote;
  m.code = null; m.hote = false; m.place = 0; m.enJeu = false;
  if (etaitHote) {                                        // l'hote part : le salon ferme
    for (const o of membres(conns, code)) {
      if (o === c) continue;
      o.meta.code = null; o.meta.place = 0;
      o.send('ferme');
    }
  } else annoncerMembres(conns, code);
}

// Paquet du jeu ("d ...") de c -> [[destinataire, ligne], ...]
export function relais(conns, c, raw) {
  if (typeof raw !== 'string' || raw.length > MAX_RELAIS) return [];
  const m = c.meta, w = raw.split(' ');
  if (w[0] !== 'd' || !m.code) return [];
  if (m.hote && w.length === 3 && B64_RE.test(w[2])) {   // hote -> un joueur, ou tous
    const ligne = `d ${w[2]}`;
    return membres(conns, m.code).filter((o) => o !== c && (w[1] === '*' || String(o.meta.place) === w[1])).map((o) => [o, ligne]);
  }
  if (!m.hote && w.length === 2 && B64_RE.test(w[1])) {  // joueur -> hote
    const h = hoteDe(conns, m.code);
    return h ? [[h, `d ${m.place} ${w[1]}`]] : [];
  }
  return [];
}

// Traite une ligne recue de c. conns = toutes les connexions ouvertes (c comprise). hasard() : Math.random par defaut.
export function handle(conns, c, raw, hasard = Math.random) {
  if (typeof raw === 'string' && raw.startsWith('d ')) {
    for (const [o, l] of relais(conns, c, raw)) o.send(l);
    return;
  }
  if (typeof raw !== 'string' || raw.length > MAX_LIGNE) return;
  const w = raw.trim().split(/\s+/);
  const m = c.meta;
  switch (w[0]) {
    case 'ping': c.send('pong'); break;
    case 'nom':
      if (NOM_RE.test(w[1] || '')) {
        m.nom = w[1];
        if (m.code) annoncerMembres(conns, m.code);
      }
      break;
    case 'liste': {
      const ver = parseInt(w[1], 10) || 0;
      for (const h of conns) {
        const r = h.meta;
        if (!r.hote || r.ver !== ver) continue;
        const n = membres(conns, r.code).length;
        if (n >= r.max) continue;
        c.send(`salon ${r.code} ${n} ${r.max} ${r.enJeu ? 'jeu' : 'attente'} ${r.nom}`);
      }
      c.send('fin');
      break;
    }
    case 'creer': {
      quitter(conns, c);
      const code = nouveauCode(conns, hasard);
      if (!code) { c.send('refuse serveur plein'); break; }
      const max = Math.max(1, Math.min(MAXP, parseInt(w[1], 10) || MAXP));
      Object.assign(m, { code, place: 0, hote: true, max, ver: parseInt(w[2], 10) || 0, enJeu: false });
      c.send(`cree ${code}`);
      annoncerMembres(conns, code);
      break;
    }
    case 'rejoindre': {
      const code = (w[1] || '').toUpperCase(), ver = parseInt(w[2], 10) || 0;
      if (!CODE_RE.test(code)) { c.send('refuse code invalide'); break; }
      const h = hoteDe(conns, code);
      if (!h) { c.send('refuse salon introuvable'); break; }
      if (h.meta.ver !== ver) { c.send('refuse version differente (mets le jeu a jour)'); break; }
      if (m.code === code) break;
      const ms = membres(conns, code);
      if (ms.length >= h.meta.max) { c.send('refuse salon plein'); break; }
      quitter(conns, c);
      let place = 1;
      while (ms.some((o) => o.meta.place === place)) place++;
      Object.assign(m, { code, place, hote: false, max: h.meta.max, ver, enJeu: h.meta.enJeu });
      c.send(`rejoint ${code} ${place}`);
      annoncerMembres(conns, code);
      if (h.meta.enJeu) c.send('lance');                  // la partie a deja commence : il la rejoint
      break;
    }
    case 'quitter': quitter(conns, c); break;
    case 'lancer':
      if (!m.hote || m.enJeu) break;
      m.enJeu = true;
      for (const o of membres(conns, m.code)) { o.meta.enJeu = true; o.send('lance'); }
      break;
    default: break;
  }
}

// la connexion c vient de se fermer (conns ne la contient plus)
export function closed(conns, c) {
  quitter(conns.concat([c]), c);
}
