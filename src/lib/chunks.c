#include "chunks.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "util.h"
#include "platform.h"

#define CEIL(a, b) (((a) / (b)) + (((a) % (b)) > 0 ? 1 : 0))

struct chunks_struct
{
  uint8_t space;

  uint8_t *writing;
  uint32_t writelen, writeat;

  uint8_t *reading;
  uint32_t readlen, readat;
};

chunks_t chunks_new(uint8_t size)
{
  chunks_t chunks;
  if(!(chunks = malloc(sizeof (struct chunks_struct)))) return LOG("OOM");
  memset(chunks,0,sizeof (struct chunks_struct));

  if(!size)
  {
    chunks->space = 255;
  }else if(size == 1){
    chunks->space = 1; // minimum
  }else{
    chunks->space = size-1;
  }
  return chunks;
}

chunks_t chunks_free(chunks_t chunks)
{
  if(!chunks) return NULL;
  if(chunks->writing) free(chunks->writing);
  if(chunks->reading) free(chunks->reading);
  free(chunks);
  return NULL;
}

// internal to clean up written data
chunks_t _chunks_gc(chunks_t chunks)
{
  uint8_t len;
  if(!chunks) return NULL;

  // nothing to do
  if(!chunks->writing || !chunks->writeat || !chunks->writelen) return chunks;

  len = chunks->writing[0]+1;
  if(len > chunks->writelen) return LOG("bad chunk write data");

  // the current chunk hasn't beeen written yet
  if(chunks->writeat < len) return chunks;

  // remove the current chunk
  chunks->writelen -= len;
  chunks->writeat -= len;
  memmove(chunks->writing,chunks->writing+len,chunks->writelen);
  chunks->writing = util_reallocf(chunks->writing, chunks->writelen);

  // tail recurse to eat any more chunks
  return _chunks_gc(chunks);
}

// turn this packet into chunks
chunks_t chunks_send(chunks_t chunks, lob_t out)
{
  uint32_t start, len, at;
  uint8_t *raw, size;
  
  // validate and gc first
  if(!_chunks_gc(chunks) || !(len = lob_len(out))) return chunks;

  start = chunks->writelen;
  chunks->writelen += len;
  chunks->writelen += CEIL(len,chunks->space); // include space for per-chunk start byte
  chunks->writelen++; // space for terminating 0
  if(!(chunks->writing = util_reallocf(chunks->writing, chunks->writelen)))
  {
    chunks->writelen = chunks->writeat = 0;
    return LOG("OOM");
  }
  
  raw = lob_raw(out);
  for(at = 0; at < len;)
  {
    size = ((len-at) < chunks->space) ? (len-at) : chunks->space;
    chunks->writing[start] = size;
    start++;
    memcpy(chunks->writing+start,raw+at,size);
    at += size;
    start += size;
  }
  chunks->writing[start] = 0; // end of chunks, full packet
  
  return chunks;
}

// get any packets that have been reassembled from incoming chunks
lob_t chunks_receive(chunks_t chunks)
{
  uint32_t at, len;
  uint8_t *buf, *append;
  lob_t ret;

  if(!chunks || !chunks->reading) return NULL;
  // check for complete packet and get its length
  for(len = at = 0;at < chunks->readlen && chunks->reading[at]; at += chunks->reading[at]+1) len += chunks->reading[at];
  if(!len || at >= chunks->readlen) return NULL;
  
  if(!(buf = malloc(len))) return LOG("OOM %d",len);
  // copy in the body of each chunk
  for(at = 0, append = buf; chunks->reading[at]; append += chunks->reading[at], at += chunks->reading[at]+1)
  {
    memcpy(append, chunks->reading+(at+1), chunks->reading[at]);
  }
  ret = lob_parse(buf,len);
  free(buf);
  
  // advance the reading buffer the whole packet, shrink
  at++;
  chunks->readlen -= at;
  memmove(chunks->reading,chunks->reading+at,chunks->readlen);
  chunks->reading = util_reallocf(chunks->reading,chunks->readlen);

  return ret;
}

// get the next chunk, put its length in len
uint8_t *chunks_out(chunks_t chunks, uint8_t *len)
{
  uint8_t *ret;
  if(!chunks || !len) return NULL;
  if(!_chunks_gc(chunks)) return NULL; // try to clean up any done chunks

  if(chunks->writeat == chunks->writelen)
  {
    len[0] = 0;
    return NULL;
  }
  
  // next chunk body+len
  len[0] = chunks->writing[chunks->writeat] + 1;

  // include any trailing zeros in this chunk if there's space
  while(len[0] < chunks->space && (chunks->writeat+len[0]) < chunks->writelen && chunks->writing[chunks->writeat+len[0]] == 0) len[0]++;

  ret = chunks->writing+chunks->writeat;
  chunks->writeat += len[0];
  return ret;
}

// internal to append read data
chunks_t _chunks_append(chunks_t chunks, uint8_t *block, uint32_t len)
{
  if(!chunks || !block || !len) return chunks;
  if(!chunks->reading) chunks->readlen = chunks->readat = 0; // be paranoid
  chunks->readlen += len;
  if(!(chunks->reading = util_reallocf(chunks->reading, chunks->readlen))) return LOG("OOM"); 
  memcpy(chunks->reading+(chunks->readlen-len),block,len);
  return chunks;
}

// process an incoming individual chunk
chunks_t chunks_in(chunks_t chunks, uint8_t *chunk, uint8_t len)
{
  if(!chunks || !chunk || !len) return chunks;
  if(len < (*chunk + 1)) return LOG("invalid chunk len %d < %d+1",len,*chunk);
  return _chunks_append(chunks,chunk,len);
}

// how many bytes are there in total to be sent
uint32_t chunks_len(chunks_t chunks)
{
  if(!chunks || !chunks->writing || chunks->writeat >= chunks->writelen) return 0;
  return chunks->writelen - chunks->writeat;
}

// return the next block of data to be written to the stream transport
uint8_t *chunks_write(chunks_t chunks)
{
  if(!chunks_len(chunks)) return NULL;
  return chunks->writing+chunks->writeat;
}

// advance the write pointer this far
chunks_t chunks_written(chunks_t chunks, uint32_t len)
{
  if(!chunks || (len+chunks->writeat) > chunks->writelen) return NULL;
  chunks->writeat += len;
  // try a cleanup
  return _chunks_gc(chunks);

}

// process incoming stream data into any packets, returns NULL until a chunk was received and ensures there's data to write
chunks_t chunks_read(chunks_t chunks, uint8_t *block, uint32_t len)
{
  uint32_t at, good;
  if(!_chunks_append(chunks,block,len)) return NULL;
  if(!chunks->reading || !chunks->readlen) return NULL; // paranoid

  // walk through read data to skip whole chunks, stop at incomplete one
  for(at = chunks->readat;at < chunks->readlen; at += chunks->reading[at]+1) good = at;
  if(at == chunks->readlen) good = at;

  // unchanged
  if(good == chunks->readat) return NULL;

  // there's already response data waiting
  if(chunks->writing) return chunks;

  // advance to start of last whole read chunk, add a zero write chunk
  if(!(chunks->writing = util_reallocf(chunks->writing, chunks->writelen+1))) return LOG("OOM");
  memset(chunks->writing+chunks->writelen,0,1); // zeros are acks
  chunks->writelen++;
  chunks->readat = good;

  return chunks;
}
