#include "ext.h"

// handle incoming packets for the built-in link channel
void link_chan_handler(link_t link, channel3_t chan, void *arg)
{
  lob_t packet;
  if(!link || link->chan != chan) return;
  while((packet = channel3_receiving(chan)))
  {
    link->up = lob_free(link->up);
    // TODO if packet has err, set link->status instead
    link->up = packet;
    // send out link update/change signal
    mesh_link(link->mesh, link);
  }

  // if no status is set, default one back
  if(!link->status)
  {
    link->status = lob_new();
    channel3_send(chan,link->status);
  }
}

// new incoming link channel, set up handler
void link_chan_open(link_t link, lob_t open)
{
  if(!link || !open) return;
  if(link->chan) LOG("TODO remove end/remove existing channel, leaking it!");
  link->chan = channel3_new(open);
  link_handle(link, link->chan, link_chan_handler, NULL);
  channel3_receive(link->chan, open);
  link_chan_handler(link, link->chan, NULL);
}

mesh_t ext_link(mesh_t mesh)
{
  // set up built-in link channel handler
  mesh_on_open(mesh, "link", link_chan_open);
  return mesh;
}

/*
#define LINK_TPING 29
#define LINK_TOUT 65

typedef struct link_struct 
{
  int meshing;
  bucket_t links;
  // only used when seeding
  int seeding;
  bucket_t *buckets;
} *link_t;

link_t link_new(switch_t s)
{
  link_t l;
  l = malloc(sizeof (struct link_struct));
  memset(l,0,sizeof (struct link_struct));
  xht_set(s->index,"link",l);
  l->links = bucket_new();
  return l;
}

link_t link_get(switch_t s)
{
  link_t l;
  l = xht_get(s->index,"link");
  return l ? l : link_new(s);
}

void link_free(switch_t s)
{
  int i;
  link_t l = link_get(s);
  if(l->seeding)
  {
    for(i=0;i<=255;i++) if(l->buckets[i]) bucket_free(l->buckets[i]);
  }
  bucket_free(l->links);
  free(l);
}

// automatically mesh any links
void link_mesh(switch_t s, int max)
{
  link_t l = link_get(s);
  l->meshing = max;
  if(!max) return;
  // TODO check s->seeds
}

// enable acting as a seed
void link_seed(switch_t s, int max)
{
  link_t l = link_get(s);
  l->seeding = max;
  if(!max) return;
  // create all the buckets
  l->buckets = malloc(256 * sizeof(bucket_t));
  memset(l->buckets, 0, 256 * sizeof(bucket_t));
}


// adds regular ping data and sends
void link_ping(link_t l, chan_t c, lob_t p)
{
  if(!l || !c || !p) return;
  if(l->seeding) lob_set(p,"seed","true",4);
  chan_send(c, p);
}

// create/fetch/maintain a link to this hn, sends note on UP and DOWN change events
chan_t link_hn(switch_t s, hashname_t hn, lob_t note)
{
  chan_t c;
  link_t l = link_get(s);
  if(!s || !hn) return NULL;
  
  c = (chan_t)bucket_arg(l->links,hn);
  if(c)
  {
    lob_free((lob_t)c->arg);
  }else{
    c = chan_new(s, hn, "link", 0);
    chan_timeout(c,60);
    bucket_set(l->links,hn,(void*)c);
  }

  c->arg = note;
  // TODO send see array
  // TODO bridging signals
  link_ping(l, c, chan_packet(c));
  return c;
}

void ext_link(chan_t c)
{
  chan_t cur;
  lob_t p, note = (lob_t)c->arg;
  link_t l = link_get(c->s);

  // check for existing links to update
  cur = (chan_t)bucket_arg(l->links,c->to);
  if(cur != c && !c->ended)
  {
    // move any note over to new one
    if(cur)
    {
      c->arg = cur->arg;
      cur->arg = NULL;
      note = (lob_t)c->arg;
    }
    bucket_set(l->links,c->to,(void*)c);
  }

  while((p = chan_pop(c)))
  {
    DEBUG_PRINTF("TODO link packet %.*s\n", p->json_len, p->json);      
    lob_free(p);
  }
  // respond/ack if we haven't recently
  if(c->s->tick - c->tsent > (LINK_TPING/2)) link_ping(l, c, chan_packet(c));

  if(c->ended)
  {
    bucket_rem(l->links,c->to);
    // TODO remove from seeding
  }
  
  // if no note, nothing else to do
  if(!note) return;

  // handle note UP/DOWN change based on channel state
  if(!c->ended)
  {
    if(util_cmp(lob_get_str(note,"link"),"up") != 0)
    {
      lob_set_str(note,"link","up");
      chan_reply(c,lob_copy(note));
    }
  }else{
    if(util_cmp(lob_get_str(note,"link"),"down") != 0)
    {
      lob_set_str(note,"link","down");
      chan_reply(c,lob_copy(note));
    }
    c->arg = NULL;
    // keep trying to start over
    link_hn(c->s,c->to,note);
  }
  
}

void ext_seek(chan_t c)
{
  printf("TODO handle seek channel\n");
}
*/