#include "hashname.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "base32.h"
#include "../platform.h"
#include "util.h"
#include "../e3x/e3x.h" // for sha256 e3x_hash()

// how many csids can be used to make a hashname
#define MAX_CSIDS 8

// validate a str is a base32 hashname
uint8_t hashname_valid(char *str)
{
  static uint8_t buf[32];
  if(!str) return 0;
  if(strlen(str) != 52) return 0;
  if(base32_decode_into(str,52,buf) != 32) return 0;
  return 1;
}

// bin must be 32 bytes
hashname_t hashname_new(uint8_t *bin)
{
  hashname_t hn;
  if(!(hn = malloc(sizeof (struct hashname_struct)))) return NULL;
  memset(hn,0,sizeof (struct hashname_struct));
  if(bin)
  {
    memcpy(hn->bin, bin, 32);
    base32_encode_into(hn->bin,32,hn->hashname);
  }
  return hn;
}

// these all create a new hashname
hashname_t hashname_str(char *str)
{
  hashname_t hn;
  if(!hashname_valid(str)) return NULL;
  hn = hashname_new(NULL);
  base32_decode_into(str,52,hn->bin);
  base32_encode_into(hn->bin,32,hn->hashname);
  return hn;
}

// create hashname from intermediate values as hex/base32 key/value pairs
hashname_t hashname_key(lob_t key)
{
  int i, start;
  uint8_t hash[64];
  char *id, *value;
  hashname_t hn = NULL;
  if(!key) return LOG("invalid args");

  // get in sorted order
  lob_sort(key);

  // loop through all keys rolling up
  for(i=0;(id = lob_get_index(key,i));i+=2)
  {
    value = lob_get_index(key,i+1);
    if(strlen(id) != 2 || !util_ishex(id,2) || !value) continue; // skip non-id keys
    
    // hash the id
    util_unhex(id,2,hash+32);
    start = (i == 0) ? 32 : 0; // only first one excludes previous rollup
    e3x_hash(hash+start,(32-start)+1,hash); // hash in place

    // get the value from the body if bool
    if(util_cmp("true",value) == 0)
    {
      if(key->body_len == 0) return LOG("missing key body");
      // hash the body
      e3x_hash(key->body,key->body_len,hash+32);
    }else{
      if(strlen(value) != 52) return LOG("invalid value %s %d",value,strlen(value));
      base32_decode_into(value,52,hash+32);
    }
    e3x_hash(hash,64,hash);
  }
  if(!i || i % 2 != 0) return LOG("invalid keys %d",i);
  
  hn = hashname_new(hash);
  return hn;
}

hashname_t hashname_keys(lob_t keys)
{
  hashname_t hn;
  lob_t im;

  if(!keys) return LOG("bad args");
  im = hashname_im(keys,0);
  hn = hashname_key(im);
  lob_free(im);
  return hn;
}

hashname_t hashname_free(hashname_t hn)
{
  if(!hn) return NULL;
  free(hn);
  return NULL;
}

uint8_t hashname_id(lob_t a, lob_t b)
{
  uint8_t id, best;
  int i;
  char *key;

  if(!a || !b) return 0;

  best = 0;
  for(i=0;(key = lob_get_index(a,i));i+=2)
  {
    if(strlen(key) != 2) continue;
    if(!lob_get(b,key)) continue;
    id = 0;
    util_unhex(key,2,&id);
    if(id > best) best = id;
  }
  
  return best;
}

// packet-format w/ intermediate hashes in the json
lob_t hashname_im(lob_t keys, uint8_t id)
{
  int i,len;
  uint8_t *buf, hash[32];
  char *key, *value, hex[3];
  lob_t im;

  if(!keys) return LOG("bad args");

  // loop through all keys and create intermediates
  im = lob_new();
  buf = NULL;
  util_hex(&id,1,hex);
  for(i=0;(key = lob_get_index(keys,i));i+=2)
  {
    value = lob_get_index(keys,i+1);
    if(strlen(key) != 2 || !value) continue; // skip non-csid keys
    len = base32_decode_length(strlen(value));
    // save to body raw or as a base32 intermediate value
    if(id && util_cmp(hex,key) == 0)
    {
      lob_body(im,NULL,len);
      if(base32_decode_into(value,strlen(value),im->body) != len) continue;
      lob_set_raw(im,key,"true",4);
    }else{
      buf = util_reallocf(buf,len);
      if(!buf) return lob_free(im);
      if(base32_decode_into(value,strlen(value),buf) != len) continue;
      // store the hash intermediate value
      e3x_hash(buf,len,hash);
      lob_set_base32(im,key,hash,32);
    }
  }
  if(buf) free(buf);
  return im;
}

/*
hashname_t hashname_free(hashname_t hn)
{
  if(!hn) return NULL;
  if(hn->chans) xht_free(hn->chans);
  if(hn->c) crypt_free(hn->c);
  if(hn->parts) lob_free(hn->parts);
  if(hn->paths) free(hn->paths);
  free(hn);
  return NULL;
}

hashname_t hashname_get(xht_t index, unsigned char *bin)
{
  hashname_t hn;
  unsigned char hex[65];
  
  if(!bin) return NULL;
  util_hex(bin,32,hex);
  hn = xht_get(index, (const char*)hex);
  if(hn) return hn;

  // init new hashname container
  if(!(hn = malloc(sizeof (struct hashname_struct)))) return NULL;
  memset(hn,0,sizeof (struct hashname_struct));
  memcpy(hn->hashname, bin, 32);
  memcpy(hn->hexname, hex, 65);
  xht_set(index, (const char*)hn->hexname, (void*)hn);
  if(!(hn->paths = malloc(sizeof (path_t)))) return hashname_free(hn);
  hn->paths[0] = NULL;
  return hn;
}

hashname_t hashname_gethex(xht_t index, char *hex)
{
  hashname_t hn;
  unsigned char bin[32];
  if(!hex || strlen(hex) < 64) return NULL;
  if((hn = xht_get(index,hex))) return hn;
  util_unhex((unsigned char*)hex,64,bin);
  return hashname_get(index,bin);
}

int csidcmp(void *arg, const void *a, const void *b)
{
  if(*(char*)a == *(char*)b) return *(char*)(a+1) - *(char*)(b+1);
  return *(char*)a - *(char*)b;
}

hashname_t hashname_getparts(xht_t index, lob_t p)
{
  char *part, csid, csids[16], hex[3]; // max parts of 8
  int i,ids,ri,len;
  unsigned char *rollup, hnbin[32];
  char best = 0;
  hashname_t hn;

  if(!p) return NULL;
  hex[2] = 0;

  for(ids=i=0;ids<8 && p->js[i];i+=4)
  {
    if(p->js[i+1] != 2) continue; // csid must be 2 char only
    memcpy(hex,p->json+p->js[i],2);
    memcpy(csids+(ids*2),hex,2);
    util_unhex((unsigned char*)hex,2,(unsigned char*)&csid);
    if(csid > best && xht_get(index,hex)) best = csid; // matches if we have the same csid in index (for our own keys)
    ids++;
  }
  
  if(!best) return NULL; // we must match at least one
  util_sort(csids,ids,2,csidcmp,NULL);

  rollup = NULL;
  ri = 0;
  for(i=0;i<ids;i++)
  {
    len = 2;
    if(!(rollup = util_reallocf(rollup,ri+len))) return NULL;
    memcpy(rollup+ri,csids+(i*2),len);
    crypt_hash(rollup,ri+len,hnbin);
    ri = 32;
    if(!(rollup = util_reallocf(rollup,ri))) return NULL;
    memcpy(rollup,hnbin,ri);

    memcpy(hex,csids+(i*2),2);
    part = lob_get_str(p, hex);
    if(!part) continue; // garbage safety
    len = strlen(part);
    if(!(rollup = util_reallocf(rollup,ri+len))) return NULL;
    memcpy(rollup+ri,part,len);
    crypt_hash(rollup,ri+len,hnbin);
    memcpy(rollup,hnbin,32);
  }
  memcpy(hnbin,rollup,32);
  free(rollup);
  hn = hashname_get(index, hnbin);
  if(!hn) return NULL;

  if(!hn->parts) hn->parts = p;
  else lob_free(p);
  
  hn->csid = best;
  util_hex((unsigned char*)&best,1,(unsigned char*)hn->hexid);

  return hn;
}

hashname_t hashname_frompacket(xht_t index, lob_t p)
{
  hashname_t hn = NULL;
  if(!p) return NULL;
  
  // get/gen the hashname
  hn = hashname_getparts(index, lob_get_packet(p, "from"));
  if(!hn) return NULL;

  // load key from packet body
  if(!hn->c && p->body)
  {
    hn->c = crypt_new(hn->csid, p->body, p->body_len);
    if(!hn->c) return NULL;
  }
  return hn;
}

// derive a hn from json seed or connect format
hashname_t hashname_fromjson(xht_t index, lob_t p)
{
  char *key;
  hashname_t hn = NULL;
  lob_t pp, next;
  path_t path;

  if(!p) return NULL;
  
  // get/gen the hashname
  pp = lob_get_packet(p,"from");
  if(!pp) pp = lob_get_packet(p,"parts");
  hn = hashname_getparts(index, pp); // frees pp
  if(!hn) return NULL;

  // if any paths are stored, associte them
  pp = lob_get_packets(p, "paths");
  while(pp)
  {
    path = hashname_path(hn, path_parse((char*)pp->json, pp->json_len), 0);
    next = pp->next;
    lob_free(pp);
    pp = next;
  }

  // already have crypto
  if(hn->c) return hn;

  if(p->body_len)
  {
    hn->c = crypt_new(hn->csid, p->body, p->body_len);
  }else{
    pp = lob_get_packet(p, "keys");
    key = lob_get_str(pp,hn->hexid);
    if(key) hn->c = crypt_new(hn->csid, (unsigned char*)key, strlen(key));
    lob_free(pp);
  }
  
  return (hn->c) ? hn : NULL;  
}

path_t hashname_path(hashname_t hn, path_t p, int valid)
{
  path_t ret = NULL;
  int i;

  if(!p) return NULL;

  // find existing matching path
  for(i=0;hn->paths[i];i++)
  {
    if(path_match(hn->paths[i], p)) ret = hn->paths[i];
  }
  if(!ret && (ret = path_copy(p)))
  {
    // add new path, i is the end of the list from above
    if(!(hn->paths = util_reallocf(hn->paths, (i+2) * (sizeof (path_t))))) return NULL;
    hn->paths[i] = ret;
    hn->paths[i+1] = 0; // null term
  }

  // update state tracking
  if(ret && valid)
  {
    hn->last = ret;
    ret->tin = platform_seconds();    
  }

  return ret;
}

unsigned char hashname_distance(hashname_t a, hashname_t b)
{
  return 0;
}

*/
