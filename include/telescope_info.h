#ifndef __RTS_TELESCOPE_INFO__
#define __RTS_TELESCOPE_INFO__

struct telescope_info
{
  char type[64];
  char serial_number[64];
  double ra;
  double dec;
  double park_dec;
  // geographic informations
  double longtitude;
  double latitude;
  float altitude;
  // time information
  double siderealtime;		// LOCAL sidereal time!!
  double localtime;
  int correction_mark;
  int flip;
  double axis0_counts;
  double axis1_counts;
};

#endif /* __RTS_TELESCOPE_INFO__ */
