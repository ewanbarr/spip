/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DataBlockView.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

#define IPCBUF_DISCON  0  /* disconnected */
#define IPCBUF_VIEWER  1  /* connected */

#define IPCBUF_WRITER  2  /* one process that writes to the buffer */
#define IPCBUF_WRITING 3  /* start-of-data flag has been raised */
#define IPCBUF_WCHANGE 4  /* next operation will change writing state */

#define IPCBUF_READER  5  /* one process that reads from the buffer */
#define IPCBUF_READING 6  /* start-of-data flag has been raised */
#define IPCBUF_RSTOP   7  /* end-of-data flag has been raised */

#define IPCBUF_VIEWING 8  /* currently viewing */
#define IPCBUF_VSTOP   9  /* end-of-data while viewer */


using namespace std;

spip::DataBlockView::DataBlockView (const char * key_string) : spip::DataBlock(key_string)
{

}

spip::DataBlockView::~DataBlockView ()
{
}

void spip::DataBlockView::lock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (locked)
    throw runtime_error ("data block already locked");

  if (ipcio_open (data_block, 'r') < 0)
   throw runtime_error ("could not lock header block for viewing");

  locked = true;
}

void spip::DataBlockView::unlock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("data block was not locked");

    // I dont think this is necessary!
  //if (ipcio_close (data_block) < 0)
  //  throw runtime_error ("could not close data block from viewing");

  locked = false;
}

void spip::DataBlockView::read_header ()
{
  header_size = 0;
  char * header_ptr;

  header_ptr = ipcbuf_get_next_read (header_block, &header_size);

  while (!header_size)
  {
    // wait for 
    header_ptr = ipcbuf_get_next_read (header_block, &header_size);

    // note that a viewer does not mark the header buffer as cleared
    if (ipcbuf_eod (header_block))
    {
      cerr << "spip::DataBlockView::read_header End of data on header block" << endl;
      if (ipcbuf_is_reader (header_block))
      {
        ipcbuf_reset (header_block);
      }
    }
    else
    {
      throw runtime_error ("empty header block");
    }
    sleep(1);
  }

  memcpy (header, header_ptr, header_size);

}

void * spip::DataBlockView::open_block ()
{
  curr_buf = (void *) ipcio_open_block_read (data_block, &curr_buf_bytes, &curr_buf_id);
  return curr_buf;
}

int64_t spip::DataBlockView::read (void * buffer, uint64_t bytes_to_read)
{
  return ipcio_read (data_block, (char*) buffer, bytes_to_read);
}

ssize_t spip::DataBlockView::close_block (uint64_t bytes)
{
  throw runtime_error ("not implemened");
}

int spip::DataBlockView::eod ()
{
  return ipcbuf_eod ( &(data_block->buf) );
}

int spip::DataBlockView::view_eod (uint64_t byte_resolution)
{
  ipcbuf_t * buf = &(data_block->buf);

#ifdef _DEBUG
  cerr << "spip::DataBlockView::view_eod: write_buf=" << ipcbuf_get_write_count( buf ) << endl;
#endif

#ifdef _DEBUG
  cerr << "spip::DataBlockView::view_eod incrementing buf->viewbuf=" << buf->viewbuf << endl;
#endif
  buf->viewbuf ++;

  if (ipcbuf_get_write_count( buf ) > buf->viewbuf)
    buf->viewbuf = ipcbuf_get_write_count( buf ) + 1;

#ifdef _DEBUG
  cerr << "spip::DataBlockView::view_eod buf->viewbuf=" << buf->viewbuf << endl;
#endif

  data_block->bytes = 0;
  data_block->curbuf = 0;

  uint64_t current = ipcio_tell (data_block);
  uint64_t too_far = current % byte_resolution;
  if (too_far)
  {
    int64_t absolute_bytes = ipcio_seek (data_block,
           current + byte_resolution - too_far,
           SEEK_SET);
    if (absolute_bytes < 0)
      return -1;
  }
  return 0;
}

void * spip::DataBlockView::get_curr_buf ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as reader");

  return (void *) data_block->curbuf;
}
