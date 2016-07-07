
#ifndef __AsciiHeader_h
#define __AsciiHeader_h

#include <cstddef>

#define DEFAULT_HEADER_SIZE 4096

namespace spip {

  class AsciiHeader {

    public:

      AsciiHeader ();

      AsciiHeader (size_t header_size);

      AsciiHeader (const AsciiHeader &obj);

      ~AsciiHeader ();

      char * raw () const { return header; };

      void resize (size_t new_size);

      size_t get_header_size () const;

      size_t get_header_length() const;

      void reset () { header[0] = '\0'; };

      int load_from_file (const char* filename);

      int load_from_str (const char* header_str);

      int append_from_str (const char* header_str);

      int get (const char* keyword, const char* format, ...);

      int get (const char* keyword, const char* format, ...) const;

      int set (const char* keyword, const char* code, ...);

      int del (const char * keyword);

      static int header_get (const char *hdr, const char* keyword, const char* format, ...);

      static char * header_find (const char *hdr, const char* keyword);

      static size_t get_size (char * filename);

      static size_t get_size_fd (int fd);

      static long fileread (const char* filename, char* buffer, unsigned bufsz);

      static long filesize (const char* filename);

      static const char* whitespace;

    private:

      char * header;

      size_t header_size;

      char* find (const char* keyword);

      char* find (const char* keyword) const;

  };
}
            
#endif // __Ascii_header_h
