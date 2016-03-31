/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/AsciiHeader.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdarg>
#include <cstring>
#include <iostream>
#include <stdexcept>

#define STRLEN 4096

using namespace std;

const char * spip::AsciiHeader::whitespace = " \t\n";

spip::AsciiHeader::AsciiHeader()
{
  header_size = DEFAULT_HEADER_SIZE;
  header = (char *) malloc (header_size);
}

spip::AsciiHeader::AsciiHeader(size_t nbytes)
{
  header_size = nbytes;
  header = (char *) malloc (header_size);
}

spip::AsciiHeader::~AsciiHeader()
{
  free (header);
  header = 0;
}

void spip::AsciiHeader::resize (size_t new_size)
{
  if (new_size > header_size)
  {
    if (new_size % DEFAULT_HEADER_SIZE)
      new_size = (((new_size / DEFAULT_HEADER_SIZE) + 1) * DEFAULT_HEADER_SIZE);
    header = (char *) realloc (header, new_size);
    if (!header)
      throw runtime_error ("could not allocate memory for header");
    header_size = new_size;
  }
}

int spip::AsciiHeader::load_from_file (const char * filename)
{
  size_t file_size = spip::AsciiHeader::filesize (filename);
  if (file_size > header_size)
  {
    cerr << "spip::AsciiHeader::load_from_file resizing" << endl;
    resize (file_size);
  }
  if (spip::AsciiHeader::fileread (filename, header, header_size) < 0)
    throw runtime_error ("could not read header from file");
}

int spip::AsciiHeader::load_from_str (const char * string)
{
  size_t str_length = strlen(string) + 1;
  if (str_length > header_size)
    resize (str_length);
  strcpy (header, string);
}

int spip::AsciiHeader::append_from_str (const char * string)
{
  size_t str_length = strlen(string) + strlen(header) + 1;
  if (str_length > header_size)
    resize (str_length);
  strcat (header, string);
}

int spip::AsciiHeader::get (const char* keyword, const char* format, ...) const
{
  va_list arguments;

  char* value = 0;
  int ret = 0;

  /* find the keyword */
  char* key = find (keyword);
  if (!key)
    return -1;

  /* find the value after the keyword */
  value = key + strcspn (key, whitespace);

  /* parse the value */
  va_start (arguments, format);
  ret = vsscanf (value, format, arguments);
  va_end (arguments);

  return ret;
}


int spip::AsciiHeader::get (const char* keyword, const char* format, ...)
{
  va_list arguments;

  char* value = 0;
  int ret = 0;

  /* find the keyword */
  char* key = find (keyword);
  if (!key)
    return -1;

  /* find the value after the keyword */
  value = key + strcspn (key, whitespace);

  /* parse the value */
  va_start (arguments, format);
  ret = vsscanf (value, format, arguments);
  va_end (arguments);

  return ret;
}

int spip::AsciiHeader::set (const char* keyword, const char* format, ...)
{
  va_list arguments;

  char value[STRLEN];
  char* eol = 0;
  char* dup = 0;
  int ret = 0;

  /* find the keyword (also the insertion point) */
  char* key = find (keyword);

  if (key) {
    /* if the keyword is present, find the first '#' or '\n' to follow it */
    eol = key + strcspn (key, "#\n");
  }
  else {
    /* if the keyword is not present, append to the end, before "DATA" */
    eol = strstr (header, "DATA\n");
    if (eol)
      /* insert in front of DATA */
      key = eol;
    else
      /* insert at end of string */
      key = header + strlen (header);
  }

  va_start (arguments, format);
  ret = vsnprintf (value, STRLEN, format, arguments);
  va_end (arguments);

  if (ret < 0) {
    perror ("ascii_header_set: error snprintf\n");
    return -1;
  }

  if (eol)
    /* make a copy */
    dup = strdup (eol);

  /* %Xs dictates only a minumum string length */
  if (sprintf (key, "%-12s %-20s   ", keyword, value) < 0) {
    if (dup)
      free (dup);
    perror ("ascii_header_set: error sprintf\n");
    return -1;
  }

  if (dup) {
    strcat (key, dup);
    free (dup);
  }
  else
    strcat (key, "\n");

  return 0;
}

int spip::AsciiHeader::del (const char * keyword)
{
  /* find the keyword (also the delete from point) */
  char * key = find (keyword);

  /* if the keyword is present, find the first '#' or '\n' to follow it */
  if (key)
  {
    char * eol = key + strcspn (key, "\n") + 1;

    // make a copy of everything after the end of the key we are deleting
    char * dup = strdup (eol);

    if (dup)
    {
      key[0] = '\0';
      strcat (header, dup);
      free (dup);
      return 0;
    }
    else
      return -1;
  }
  else
    return -1;

}

char* spip::AsciiHeader::find (const char* keyword)
{
  return AsciiHeader::header_find (header, keyword);
}

char* spip::AsciiHeader::find (const char* keyword) const
{
  return AsciiHeader::header_find (header, keyword);
}


size_t spip::AsciiHeader::get_size (char * filename)
{
    size_t hdr_size = -1;
  int fd = open (filename, O_RDONLY);
  if (!fd)
  {
    fprintf (stderr, "spip::AsciiHeader::get_size: failed to open %s for reading\n", filename);
  }
  else
  {
    hdr_size = AsciiHeader::get_size_fd (fd);
    close (fd);
  }
  return hdr_size;

}

size_t spip::AsciiHeader::get_size_fd (int fd)
{
  size_t hdr_size = -1;
  char * header = (char *) malloc (DEFAULT_HEADER_SIZE+1);
  if (!header)
  {
    fprintf (stderr, "spip::AsciiHeader::get_size_fd failed to allocate %d bytes\n", DEFAULT_HEADER_SIZE+1);
  }
  else
  {
    // seek to start of file
    lseek (fd, 0, SEEK_SET);

    // read the header 
    ssize_t ret = read (fd, header, DEFAULT_HEADER_SIZE);
    if (ret != DEFAULT_HEADER_SIZE)
    {
      fprintf (stderr, "spip::AsciiHeader::get_size_fd failed to read %d bytes from file\n", DEFAULT_HEADER_SIZE);
    }
    else
    {
      // check the actual HDR_SIZE in the header
      if (AsciiHeader::header_get (header, "HDR_SIZE", "%ld", &hdr_size) != 1)
      {
        fprintf (stderr, "spip::AsciiHeader::get_size_fd failed to read HDR_SIZE from header\n");
        hdr_size = -1;
      }
    }
    // seek back to start of file
    lseek (fd, 0, SEEK_SET);
    free (header);
  }
  return hdr_size;
}

int spip::AsciiHeader::header_get (const char* static_hdr, const char* keyword, const char* format, ...)
{
  va_list arguments;

  char* value = 0;
  int ret = 0;

  /* find the keyword */
  char* key = AsciiHeader::header_find (static_hdr, keyword);
  if (!key)
    return -1;

  /* find the value after the keyword */
  value = key + strcspn (key, whitespace);

  /* parse the value */
  va_start (arguments, format);
  ret = vsscanf (value, format, arguments);
  va_end (arguments);

  return ret;
}

char * spip::AsciiHeader::header_find (const char* hdr, const char* keyword)
{
  const char* key = strstr (hdr, keyword);

  // keyword might be the very first word in header
  while (key > hdr)
  {
    // if preceded by a new line, return the found key
    if ( ((*(key-1) == '\n') || (*(key-1) == '\\')) &&
         ((*(key+strlen(keyword)) == '\t') || (*(key+strlen(keyword)) == ' ')))
      break;

    // otherwise, search again, starting one byte later
    key = strstr (key+1, keyword);
  }
  return (char *) key;
}

long spip::AsciiHeader::filesize (const char* filename)
{
  struct stat statistics;
  if (stat (filename, &statistics) < 0) {
    fprintf (stderr, "filesize() error stat (%s)", filename);
    perror ("");
    return -1;
  }

  return (long) statistics.st_size;
}

long spip::AsciiHeader::fileread (const char* filename, char* buffer, unsigned bufsz)
{
  FILE* fptr = 0;
  long fsize = filesize (filename);

  if (fsize < 0) {
    fprintf (stderr, "fileread: filesize(%s) %s\n", filename, strerror(errno));
    return -1;
  }

  if (fsize > bufsz) {
    fprintf (stderr, "fileread: filesize=%ld > bufsize=%u\n", fsize, bufsz);
    return -1;
  }

  fptr = fopen (filename, "r");
  if (!fptr) {
    fprintf (stderr, "fileread: fopen(%s) %s\n", filename, strerror(errno));
    return -1;
  }

  if (fread (buffer, fsize, 1, fptr) != 1) {
    perror ("fileread: fread");
    fclose (fptr);
    return -1;
  }

  fclose (fptr);

  memset (buffer + fsize, '\0', bufsz - fsize);

  return 0;
}


