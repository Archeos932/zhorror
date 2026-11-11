// Worker + Durable Object des salons de BOURDONNEMENT.
// Une seule instance du DO ("global") tient tous les salons ; ses WebSockets sont hibernables : rien n'est facture
// tant que personne n'envoie de message. La logique est dans salons.js (testee par test.js).

import { handle, closed, newMeta, relais } from './salons.js';

export class Salons {
  constructor(ctx) {
    this.ctx = ctx;
  }

  // les connexions sont reconstruites depuis les WebSockets vivants (l'etat survit a l'hibernation)
  conns() {
    return this.ctx.getWebSockets().map((ws) => ({
      ws,
      meta: ws.deserializeAttachment() || newMeta('?'),
      send: (t) => { try { ws.send(t); } catch (e) { /* deja ferme */ } },
    }));
  }

  fetch(request) {
    if (request.headers.get('Upgrade') !== 'websocket') return new Response('BOURDONNEMENT - salons', { status: 200 });
    const pair = new WebSocketPair();
    const id = crypto.randomUUID().slice(0, 6);
    this.ctx.acceptWebSocket(pair[1], [id]);
    pair[1].serializeAttachment(newMeta(id));
    pair[1].send(`bonjour ${id}`);
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  webSocketMessage(ws, message) {
    if (typeof message !== 'string') return;
    const conns = this.conns();
    const c = conns.find((x) => x.ws === ws);
    if (!c) return;
    if (message.startsWith('d ')) {                       // relais du jeu : chemin court, rien a sauver
      for (const [o, l] of relais(conns, c, message)) o.send(l);
      return;
    }
    handle(conns, c, message);
    for (const x of conns) x.ws.serializeAttachment(x.meta);   // les metas des autres ont pu changer
  }

  webSocketClose(ws) {
    const conns = this.conns();
    const c = conns.find((x) => x.ws === ws);
    if (c) {
      const autres = conns.filter((x) => x !== c);
      closed(autres, c);
      for (const x of autres) x.ws.serializeAttachment(x.meta);
    }
    try { ws.close(); } catch (e) { /* deja ferme */ }
  }

  webSocketError(ws) {
    this.webSocketClose(ws);
  }
}

export default {
  fetch(request, env) {
    return env.SALONS.get(env.SALONS.idFromName('global')).fetch(request);
  },
};
