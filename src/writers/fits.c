/*! 
 * @file Writer routine for fits image
 *
 * @author petr
 */

#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <libnova/libnova.h>
#include <unistd.h>

#include "fitsio.h"
#include "fits.h"
#include "imghdr.h"
#include "config.h"
#include "../utils/config.h"

#include <pthread.h>

pthread_mutex_t image_fits_mutex = PTHREAD_MUTEX_INITIALIZER;

/*!
 * Init fits data.
 *
 * @param receiver 	receiver structure
 * @param filename	filename to store result fits, in fitsio
 * 			notation
 *
 * @return 0 on success, -1 and set errno on error
 */
int
fits_create (struct fits_receiver_data *receiver, char *filename)
{
  int status;
  char *fn;

  fits_clear_errmsg ();

  receiver->offset = 0;
  receiver->size = 0;
  receiver->header_processed = 0;
  receiver->ffile = NULL;

  status = 0;
  printf ("create '%s'\n", filename);

  fn = (char *) malloc (strlen (filename) + 2);
  strcpy (fn + 1, filename);
  fn[0] = '!';

  pthread_mutex_lock (&image_fits_mutex);
  if (fits_create_file (&receiver->ffile, fn, &status))
    {
      receiver->ffile = NULL;
      pthread_mutex_unlock (&image_fits_mutex);
      fits_report_error (stdout, status);
      free (fn);
      errno = EINVAL;
      return -1;
    }
  pthread_mutex_unlock (&image_fits_mutex);
  free (fn);
  return 0;
}

#define write_key(type, key, value, comment)\
	if (fits_update_key (fptr, type, key, value, comment, &status)) \
	{ \
		fits_report_error (stdout, status); \
		errno = EINVAL; \
		return -1; \
	}

int
write_camera (struct fits_receiver_data *receiver,
	      struct camera_info *camera, char *camera_name, int mount_flip)
{
  int status = 0;
  float xplate = 1, yplate = 1;
  float rotang = 0;
  char *filter = "O";
  long flip = 1;
  float cam_xoa, cam_yoa;	//optical axes

  fitsfile *fptr = receiver->ffile;

  write_key (TSTRING, "CAM_NAME", camera_name, "Camera name");

  write_key (TSTRING, "CAM_TYPE", camera->type, "Camera type");
  write_key (TSTRING, "CAM_SRLN", camera->serial_number,
	     "Camera serial number");
  write_key (TFLOAT, "CAM_SETT", &camera->temperature_setpoint,
	     "Camera regulation setpoint");
  write_key (TFLOAT, "CAM_TEMP", &camera->ccd_temperature,
	     "Camera CCD temperature");
  write_key (TFLOAT, "CAM_AIRT", &camera->air_temperature,
	     "Camera air temperature");
  write_key (TINT, "CAM_POWR", &camera->cooling_power,
	     "Camera cooling power");
  write_key (TSTRING, "CAM_FAN", camera->fan ? "on" : "off",
	     "Camera fan status");
  write_key (TINT, "CAM_FLTR", &camera->filter, "Camera filter info");
  rotang = get_device_double_default (camera_name, "rotang", 0);
  filter = get_device_string_default (camera_name, "filter", filter);
  xplate = get_device_double_default (camera_name, "xplate", xplate);
  yplate = get_device_double_default (camera_name, "yplate", yplate);
  flip = get_device_double_default (camera_name, "flip", flip);

  cam_xoa =
    get_device_double_default (camera_name, "cam_xoa",
			       ((struct imghdr *) receiver->data)->sizes[0] /
			       2);
  cam_yoa =
    get_device_double_default (camera_name, "cam_yoa",
			       ((struct imghdr *) receiver->data)->sizes[1] /
			       2);

  write_key (TFLOAT, "CAM_XOA", &cam_xoa, "X optical axe center");
  write_key (TFLOAT, "CAM_YOA", &cam_yoa, "Y optical axe center");

  write_key (TFLOAT, "XPLATE", &xplate, "X plate size");
  write_key (TFLOAT, "YPLATE", &yplate, "Y plate size");
  if (mount_flip)
    rotang =
      ln_range_degrees (rotang +
			get_device_double_default (camera_name,
						   "mount_rotang", 180.0));
  write_key (TFLOAT, "ROTANG", &rotang, "Field rotation");
  write_key (TSTRING, "FILTER", filter, "Filter used");
  write_key (TLONG, "FLIP", &flip, "Image flip");
  return 0;
}

int
write_telescope (struct fits_receiver_data *receiver,
		 struct telescope_info *telescope, double jd)
{
  int status = 0;
  struct ln_equ_posn tel;
  struct ln_lnlat_posn observer;
  struct ln_hrz_posn hrz;

  fitsfile *fptr = receiver->ffile;
  tel.ra = telescope->ra;
  tel.dec = telescope->dec;
  observer.lat = telescope->latitude;
  observer.lng = telescope->longtitude;
  ln_get_hrz_from_equ (&tel, &observer, jd, &hrz);

  write_key (TSTRING, "TEL_TYPE", telescope->type, "Telescope type");
  write_key (TSTRING, "TEL_SRLN", telescope->serial_number,
	     "Telescope serial number");
  write_key (TDOUBLE, "RASC", &telescope->ra, "Telescope ra");
  write_key (TDOUBLE, "DECL", &telescope->dec, "Telescope dec");
  write_key (TDOUBLE, "AZ", &hrz.az,
	     "Calculated telescope azimut [0=S,90=W]");
  write_key (TDOUBLE, "ALT", &hrz.alt, "Calculated telescope altitude");
  write_key (TDOUBLE, "TEL_LONG", &telescope->longtitude,
	     "Telescope longtitude");
  write_key (TDOUBLE, "TEL_LAT", &telescope->latitude, "Telescope latitude");
  write_key (TFLOAT, "TEL_ALT", &telescope->altitude,
	     "Telescope altitude in m");
  write_key (TDOUBLE, "TEL_SDTM", &telescope->siderealtime,
	     "Telescope sidereailtime");
  write_key (TDOUBLE, "TEL_LOC", &telescope->localtime,
	     "Telescope localtime");
  write_key (TINT, "TEL_FLIP", &telescope->flip, "Telescope flip");
  write_key (TDOUBLE, "TEL_CNT0", &telescope->axis0_counts,
	     "Telescope axis 0 counts");
  write_key (TDOUBLE, "TEL_CNT1", &telescope->axis0_counts,
	     "Telescope axis 1 counts");
  return 0;
  return 0;
}

int
write_weather (struct fits_receiver_data *receiver, struct dome_info *info)
{
  int status = 0;
  fitsfile *fptr = receiver->ffile;
  char *undef = "UNKNOW";

  write_key (TSTRING, "TEMP", undef, "Site temperature");
  write_key (TSTRING, "ATM_PRES", undef, "Athmospheric pressure");
  write_key (TSTRING, "WIND_SPD", undef, "Wind speed");
  write_key (TSTRING, "WIND_DIR", undef, "Wind direction");
  write_key (TSTRING, "HUMIDITY", undef, "Humidity");
  write_key (TSTRING, "DOME", undef, "Dome status");
  return 0;
}

#define write_key_unlock(type, key, value, comment)\
	if (fits_update_key (fptr, type, key, value, comment, &status)) \
	{ \
		goto err; \
	}

int
fits_write_image_info (struct fits_receiver_data *receiver,
		       struct image_info *info, char *dark_name)
{
  int status = 0;
  double jd;
  fitsfile *fptr = receiver->ffile;
  char *image_type;

  pthread_mutex_lock (&image_fits_mutex);
  jd = ln_get_julian_from_timet (&info->exposure_time);
  if (*info->telescope.type)
    {
      write_key_unlock (TSTRING, "TEL_NAME", info->telescope_name,
			"Telescope name");
      write_telescope (receiver, &info->telescope, jd);
    }
  if (*info->camera.type)
    write_camera (receiver, &info->camera, info->camera_name,
		  info->telescope.flip);
  if (*info->dome.type)
    {
//     write_key_unlock (TSTRING, "DOME_NAME", info->dome_name, "Dome name");
      write_weather (receiver, &info->dome);
    }
  write_key_unlock (TFLOAT, "EXPOSURE", &info->exposure_length,
		    "Camera exposure time in msec");

  if (dark_name)
    write_key_unlock (TSTRING, "DARK", dark_name, "Dark image path");

  switch (info->target_type)
    {
    case TARGET_LIGHT:
      image_type = "light";
      break;
    case TARGET_DARK:
      image_type = "dark";
      break;
    case TARGET_FLAT:
      image_type = "flat";
      break;
    case TARGET_FLAT_DARK:
      image_type = "flat dark";
      break;
    default:
      image_type = "Unknow";

    }
  write_key_unlock (TSTRING, "IMGTYPE", image_type, "Image type");
  write_key_unlock (TSTRING, "DARK", dark_name, "Dark image path");
  write_key_unlock (TSTRING, "FLAT", "undef", "Flat image path");
  write_key_unlock (TSTRING, "ISPROC", "", "Processed? [bdf]");
  write_key_unlock (TSTRING, "SERNUM", "1", "Number of images in the series");
  write_key_unlock (TINT, "TARGET", &info->target_id, "Target id");
  write_key_unlock (TINT, "OBSERVAT", &info->observation_id,
		    "Observation id");
  write_key_unlock (TSTRING, "OBSERVER", PACKAGE " " VERSION, "Observer");

  write_key_unlock (TLONG, "SEC", &info->exposure_time,
		    "Camera exposure start (sec 1.1.1970)");
  write_key_unlock (TLONG, "CTIME", &info->exposure_time,
		    "Camera exposure start (sec 1.1.1970)");
  write_key_unlock (TDOUBLE, "JD", &jd, "Camera exposure Julian date");

  pthread_mutex_unlock (&image_fits_mutex);
  return 0;
err:
  fits_report_error (stdout, status);
  pthread_mutex_unlock (&image_fits_mutex);
  return -1;
}

#undef write_key

/*!
 * @param expected_size	expected data size, including header
 */
int
fits_init (struct fits_receiver_data *receiver, size_t expected_size)
{
  if (expected_size <= 0)
    {
      errno = EINVAL;
      return -1;
    }
  if (!(receiver->data = malloc (expected_size)))
    {
      errno = ENOMEM;
      return -1;
    }
  receiver->size = expected_size;

  return 0;
}

/*!
 * Receive callback function.
 * 
 * @param data		received data
 * @param size		size of receive data
 * @param receiver	holds persistent data specific informations
 *
 * @return 0 if we can continue receiving data, < 0 if we don't like to see more data.
 */
int
fits_handler (void *data, size_t size, struct fits_receiver_data *receiver)
{
  memcpy (&(receiver->data[receiver->offset]), data, size);
  receiver->offset += size;
#ifdef DEBUG
  if (isatty (1))
    {
      printf (".");
    };
#endif /* DEBUG */
  if (receiver->offset > sizeof (struct imghdr))
    {
      if (!(receiver->header_processed))	// we receive full image header
	{
	  int status;
	  status = 0;
//#ifdef DEBUG
	  printf ("naxes: %i image size: %li x %li                    \n",
		  ((struct imghdr *) receiver->data)->naxes,
		  ((struct imghdr *) receiver->data)->sizes[0],
		  ((struct imghdr *) receiver->data)->sizes[1]);
//#endif /* DEBUG */
	  if (((struct imghdr *) receiver->data)->naxes > 0
	      && ((struct imghdr *) receiver->data)->naxes < 5)
	    {
	      if (receiver->ffile)
		{
		  if (fits_create_img
		      (receiver->ffile, USHORT_IMG, 2,
		       ((struct imghdr *) receiver->data)->sizes, &status))
		    fits_report_error (stdout, status);
		}
	      else
		{
		  printf ("null receiver->ffile!!\n");
		  fflush (stdout);
		}
	    }
	  else
	    {
	      printf ("bad naxes: %i\n",
		      ((struct imghdr *) receiver->data)->naxes);
	    }
	  receiver->header_processed = 1;
	}
      if (receiver->offset == receiver->size)
	{
	  int status = 0;
	  fits_clear_errmsg ();
	  pthread_mutex_lock (&image_fits_mutex);
	  if (fits_write_img
	      (receiver->ffile, TUSHORT, 1, receiver->size / 2,
	       ((receiver->data) + sizeof (struct imghdr)), &status));
	  fits_report_error (stdout, status);
	  pthread_mutex_unlock (&image_fits_mutex);
	  free (receiver->data);
#ifdef DEBUG
	  printf ("readed:%i bytes\n", receiver->offset);
#endif /* DEBUG */
	  return 1;
	}
    }
  return 0;
}

int
fits_close (struct fits_receiver_data *receiver)
{
  int status;
  pthread_mutex_lock (&image_fits_mutex);
  if (fits_close_file (receiver->ffile, &status))
    fits_report_error (stdout, status);
  pthread_mutex_unlock (&image_fits_mutex);
  return 0;
}
