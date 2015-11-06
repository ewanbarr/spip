/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DataBlock.h"

#include <sstream>
#include <iostream>
#include <cstdlib>
#include <stdexcept>

using namespace std;

spip::DataBlock::DataBlock (const char * key_string)
{
  key_t key;

  stringstream ss;
  ss << std::hex << key_string;
  ss >> key;

  // parse key_string into a dada key
  //if (sscanf (key_string, "%x", &key) != 1) 
  //{
  //  cerr << "spip::DataBlock::DataBlock could not parse " << key_string 
  //       << " as PSRDada key";
  //  throw runtime_error ("Bad PSRDada key");
  //}

  // keys for the header + data unit
  data_block_key = key;
  header_block_key = key + 1;

  ipcbuf_t ipcbuf_init = IPCBUF_INIT;
  ipcio_t ipcio_init = IPCIO_INIT;

  header_block = (ipcbuf_t *) malloc (sizeof(ipcbuf_t));
  *header_block = ipcbuf_init;

  data_block = (ipcio_t *) malloc (sizeof(ipcio_t));
  *data_block = ipcio_init;
  
  connected = false;
  locked = false;
  block_open = false;
  curr_buf = 0;
  curr_buf_bytes = 0;
  curr_buf_id = 0;
  header = 0;
  header_bufsz = 0;
  data_bufsz = 0;
}

spip::DataBlock::~DataBlock ()
{
  if (header_block)  
    free (header_block);
  if (data_block)
    free(data_block);
}

void spip::DataBlock::connect ()
{
  if (connected)
    throw runtime_error ("already connected to data block");

  if (ipcbuf_connect (header_block, header_block_key) < 0)
    throw runtime_error ("failed to connect to header block");
  if (ipcio_connect (data_block, data_block_key) < 0)
    throw runtime_error ("failed to connect to data block");

  header_bufsz = ipcbuf_get_bufsz (header_block);
  data_bufsz   = ipcbuf_get_bufsz ((ipcbuf_t *) data_block);

  header = (char *) malloc (header_bufsz);

  connected = true;
}

void spip::DataBlock::disconnect ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (ipcio_disconnect (data_block) < 0) 
    throw runtime_error ("failed to disconnect from data block");
  if (ipcbuf_disconnect (header_block) < 0)
    throw runtime_error ("failed to disconnect from header block");

  connected = false;
  locked = false;
  block_open = false;
  header_bufsz = 0;
  data_bufsz = 0;
  curr_buf = 0;
  curr_buf_bytes = 0;
  curr_buf_id = 0;
  if (header)
    free(header);
  header = 0;
}
