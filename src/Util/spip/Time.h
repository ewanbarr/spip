
#ifndef __Time_h
#define __Time_h

#include <cstddef>
#include <string>

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE /* glibc2 needs this for strptime  */
#endif
#include <time.h>


namespace spip {

  class Time {

    public:

      Time (const char * str);

      Time (time_t now);

      ~Time ();
         
      time_t get_time() { return epoch; };

      static time_t mjd2utctm (double mjd);

      void add_seconds (unsigned n) { epoch += n; }; 

      void sub_seconds (unsigned n) { epoch -= n; }; 

      std::string get_localtime ();
      std::string get_gmtime ();

      static std::string format_localtime (time_t e);
      static std::string format_gmtime (time_t e);

    private:

      time_t epoch;
  };
}
            
#endif // _Time_h_
