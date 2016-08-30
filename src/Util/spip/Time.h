
#ifndef __Time_h
#define __Time_h

#include <cstddef>
#include <string>

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE /* glibc2 needs this for strptime  */
#endif
#include <time.h>

#define MJD_1970_01_01 40587

#define UNIX_TIME_TO_MJD(t) ( MJD_1970_01_01 + ((t) / 86400.0) )
#define MJD_TO_UNIX_TIME(m) ( (long) (((m) - MJD_1970_01_01) * 86400.0) )

namespace spip {

  class Time {

    public:

      Time ();

      Time (const char * str);

      Time (time_t now);

      ~Time ();
 
      void set_time (const char * str);

      void set_time (time_t now) { epoch = now; };

      time_t get_time() { return epoch; };

      int get_mjd_day() { return (int) UNIX_TIME_TO_MJD (epoch); };

      int get_gm_year ();

      int get_gm_month ();

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
