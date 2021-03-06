#ifndef hashname_h
#define hashname_h

#include "base32.h"
#include "lob.h"

// public struct
typedef struct hashname_struct
{
  char hashname[53];
  uint8_t bin[32];
} *hashname_t;


// bin must be 32 bytes if given
hashname_t hashname_new(uint8_t *bin);
hashname_t hashname_free(hashname_t hn);

// these all create a new hashname
hashname_t hashname_str(char *str); // from a string
hashname_t hashname_keys(lob_t keys);
hashname_t hashname_key(lob_t key); // key is body, intermediates in json

// utilities related to hashnames
uint8_t hashname_valid(char *str); // validate a str is a hashname
uint8_t hashname_id(lob_t a, lob_t b); // best matching id (single byte)
lob_t hashname_im(lob_t keys, uint8_t id); // intermediate hashes in the json, optional id to set that as body

/*
#include "crypt.h"
#include "path.h"
#include "xht.h"
#include "lob.h"

typedef struct hashname_struct
{
  unsigned char hashname[32];
  char csid, hexid[3], hexname[65]; // for convenience
  unsigned long chanOut, chanMax; // for channel id tracking
  crypt_t c;
  path_t *paths, last;
  xht_t chans;
  lob_t parts;
} *hashname_t;

// fetch/create matching hn (binary or hex)
hashname_t hashname_get(xht_t index, unsigned char *hn);
hashname_t hashname_gethex(xht_t index, char *hex);

// loads from raw parts object, sets ->parts and ->csid
hashname_t hashname_getparts(xht_t index, struct lob_struct *p);

// derive a hn from packet ("from" and body), loads matching CSID
hashname_t hashname_frompacket(xht_t index, struct lob_struct *p);

// derive a hn from json format
hashname_t hashname_fromjson(xht_t index, struct lob_struct *p);

// return a matching path, or add it if none, valid=1 updates hn->last and path->tin
path_t hashname_path(hashname_t hn, path_t p, int flag);

unsigned char hashname_distance(hashname_t a, hashname_t b);

*/

#endif