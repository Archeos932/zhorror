// Tests des salons : node test.js
// Lance un serveur local (dev-server.js, meme logique que Cloudflare) et joue des scenarios avec le client
// WebSocket integre a Node.

import { startDevServer } from './dev-server.js';

const PORT = 8799;
const { server } = await startDevServer(PORT);
let fails = 0;

function check(label, got, want) {
  const ok = JSON.stringify(got) === JSON.stringify(want);
  if (!ok) fails++;
  console.log(`${ok ? 'ok   ' : 'ECHEC'} ${label}${ok ? '' : ` : obtenu ${JSON.stringify(got)}, attendu ${JSON.stringify(want)}`}`);
}

function client() {
  const ws = new WebSocket(`ws://127.0.0.1:${PORT}/`);
  const inbox = [];
  ws.onmessage = (e) => inbox.push(e.data);
  const opened = new Promise((r) => { ws.onopen = r; });
  const next = async (pred = () => true, ms = 1500) => {   // prochaine ligne qui satisfait pred
    const t0 = Date.now();
    for (;;) {
      const i = inbox.findIndex(pred);
      if (i >= 0) return inbox.splice(i, 1)[0];
      if (Date.now() - t0 > ms) return null;
      await new Promise((r) => setTimeout(r, 20));
    }
  };
  return { ws, inbox, opened, next, send: (t) => ws.send(t) };
}
const commence = (p) => (l) => l.startsWith(p);

const a = client(), b = client(), c = client(), d = client();
await Promise.all([a.opened, b.opened, c.opened, d.opened]);
check('bonjour a la connexion', (await a.next(commence('bonjour')))?.split(' ')[0], 'bonjour');
for (const x of [b, c, d]) await x.next(commence('bonjour'));

a.send('nom Alice'); b.send('nom Bob'); c.send('nom Chloe'); d.send('nom Dan');
a.send('creer 3 1');
const cree = await a.next(commence('cree'));
const code = cree?.split(' ')[1];
check('creer : un code de 4 lettres', /^[A-Z]{4}$/.test(code || ''), true);
check('creer : l\'hote est seul', await a.next(commence('membres')), 'membres 0:Alice');

b.send('liste 1');
check('liste : le salon apparait', await b.next(commence('salon')), `salon ${code} 1 3 attente Alice`);
check('liste : fin', await b.next(commence('fin')), 'fin');
b.send('liste 2');
check('liste : autre version, rien', await b.next(), 'fin');

b.send(`rejoindre ${code.toLowerCase()} 1`);
check('rejoindre : place 1', await b.next(commence('rejoint')), `rejoint ${code} 1`);
check('rejoindre : l\'hote voit Bob', await a.next(commence('membres')), 'membres 0:Alice 1:Bob');
c.send(`rejoindre ${code} 2`);
check('rejoindre : mauvaise version', await c.next(commence('refuse')), 'refuse version differente (mets le jeu a jour)');
c.send(`rejoindre ${code} 1`);
check('rejoindre : Chloe place 2', await c.next(commence('rejoint')), `rejoint ${code} 2`);
d.send(`rejoindre ${code} 1`);
check('rejoindre : salon plein (max 3)', await d.next(commence('refuse')), 'refuse salon plein');
d.send('rejoindre ZZZZ 1');
check('rejoindre : salon introuvable', await d.next(commence('refuse')), 'refuse salon introuvable');

b.send('lancer');                                         // un joueur ne peut pas lancer
check('lancer : seul l\'hote peut', await b.next(commence('lance'), 300), null);
a.send('lancer');
check('lancer : tout le monde part (Bob)', await b.next(commence('lance')), 'lance');
check('lancer : tout le monde part (Chloe)', await c.next(commence('lance')), 'lance');
await a.next(commence('lance'));

b.send('d QUJD');                                         // joueur -> hote
check('relais : Bob -> hote', await a.next(commence('d ')), 'd 1 QUJD');
check('relais : Chloe ne recoit pas', await c.next(commence('d '), 300), null);
a.send('d * U05BUA==');                                   // hote -> tous
check('relais : hote -> Bob', await b.next(commence('d ')), 'd U05BUA==');
check('relais : hote -> Chloe', await c.next(commence('d ')), 'd U05BUA==');
a.send('d 2 WFla');                                       // hote -> Chloe seulement
check('relais : hote -> Chloe seule', await c.next(commence('d ')), 'd WFla');
check('relais : ... pas Bob', await b.next(commence('d '), 300), null);
d.send('d QUJD');                                         // pas dans le salon
check('relais : un etranger n\'ecrit a personne', await a.next(commence('d '), 300), null);

d.send(`rejoindre ${code} 1`);
check('rejoindre : toujours plein', await d.next(commence('refuse')), 'refuse salon plein');
a.inbox.length = 0;
b.send('quitter');
check('quitter : l\'hote voit le depart', await a.next(commence('membres')), 'membres 0:Alice 2:Chloe');
d.send(`rejoindre ${code} 1`);
check('rejoindre en cours de partie : place 1 libre', await d.next(commence('rejoint')), `rejoint ${code} 1`);
check('... et la partie commence pour lui', await d.next(commence('lance')), 'lance');

a.ws.close();                                             // l'hote part : le salon ferme
check('hote parti : Chloe recoit ferme', await c.next(commence('ferme')), 'ferme');
check('hote parti : Dan recoit ferme', await d.next(commence('ferme')), 'ferme');
c.send('liste 1');
check('hote parti : le salon a disparu', await c.next((l) => l === 'fin' || l.startsWith('salon')), 'fin');

for (const x of [b, c, d]) x.ws.close();
server.close();
console.log(fails ? `\n${fails} ECHEC(S)` : '\ntous les tests passent');
process.exit(fails ? 1 : 0);
