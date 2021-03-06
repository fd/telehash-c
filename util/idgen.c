#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "e3x.h"
#include "lob.h"
#include "hashname.h"

#include "platform.h"

int main(int argc, char *argv[])
{
  lob_t keys, secrets, options, json;
  hashname_t id;
  FILE *fdout;

  if(argc > 2)
  {
    printf("Usage: idgen [idfile.json]\n");
    return -1;
  }

  options = lob_new();
  if(e3x_init(options))
  {
    printf("e3x init failed: %s\n",e3x_err());
    return -1;
  }

  LOG("*** generating keys ***");
  secrets = e3x_generate();
  keys = lob_linked(secrets);
  if(!keys)
  {
    LOG("keygen failed: %s",e3x_err());
    return -1;
  }
  id = hashname_keys(keys);
  json = lob_new();
  lob_set(json,"hashname",id->hashname);
  lob_set_raw(json,"keys",(char*)keys->head,keys->head_len);
  lob_set_raw(json,"secrets",(char*)secrets->head,secrets->head_len);
  
  if(argc==1)
  {
    fdout = fopen("id.json","wb");
    LOG("Writing keys to id.json");
  } else {
    fdout = fopen(argv[1],"wb");
    LOG("Writing keys to %s", argv[1]);
  }


  if(fdout == NULL)
  {
    LOG("Error opening file: %s",strerror(errno));
    return -1;
  }

  fwrite(json->head,1,json->head_len,fdout);
  fclose(fdout);
  return 0;
}
