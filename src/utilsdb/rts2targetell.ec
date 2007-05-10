#include "rts2targetell.h"

#include "../utils/infoval.h"
#include "../utils/libnova_cpp.h"
#include "../writers/rts2image.h"

// EllTarget - good for commets and so on
EllTarget::EllTarget (int in_tar_id, struct ln_lnlat_posn *in_obs):Target (in_tar_id, in_obs)
{
}

int
EllTarget::load ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  double ell_minpause;
  double ell_a;
  double ell_e;
  double ell_i;
  double ell_w;
  double ell_omega;
  double ell_n;
  double ell_JD;
//  double min_m;			// minimal magnitude
  int db_tar_id = getTargetID ();
  EXEC SQL END DECLARE SECTION;

  EXEC SQL SELECT
    EXTRACT(EPOCH FROM ell_minpause),
    ell_a,
    ell_e,
    ell_i,
    ell_w,
    ell_omega,
    ell_n,
    ell_JD
  INTO
    :ell_minpause,
    :ell_a,
    :ell_e,
    :ell_i,
    :ell_w,
    :ell_omega,
    :ell_n,
    :ell_JD
  FROM
    ell
  WHERE
    ell.tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    logMsgDb ("EllTarget::load", MESSAGE_ERROR);
    return -1;
  }
  orbit.a = ell_a;
  orbit.e = ell_e;
  orbit.i = ell_i;
  orbit.w = ell_w;
  orbit.omega = ell_omega;
  orbit.n = ell_n;
  orbit.JD = ell_JD;
  return Target::load ();
}

int
EllTarget::getPosition (struct ln_equ_posn *pos, double JD, struct ln_equ_posn *parallax)
{
  if (orbit.e == 1.0)
    {
      struct ln_par_orbit par_orbit;
      par_orbit.q = orbit.a;
      par_orbit.i = orbit.i;
      par_orbit.w = orbit.w;
      par_orbit.omega = orbit.omega;
      par_orbit.JD = orbit.JD;
      ln_get_par_body_equ_coords (JD, &par_orbit, pos);
    }
  else if (orbit.e > 1.0)
    {
      struct ln_hyp_orbit hyp_orbit;
      hyp_orbit.q = orbit.a;
      hyp_orbit.e = orbit.e;
      hyp_orbit.i = orbit.i;
      hyp_orbit.w = orbit.w;
      hyp_orbit.omega = orbit.omega;
      hyp_orbit.JD = orbit.JD;
      ln_get_hyp_body_equ_coords (JD, &hyp_orbit, pos);
    }
  else
    {
      ln_get_ell_body_equ_coords (JD, &orbit, pos);
    }

  ln_get_parallax (pos, getEarthDistance (JD), observer, 1706, JD, parallax);

  pos->ra += parallax->ra;
  pos->dec += parallax->dec;
 
  return 0;
}

int
EllTarget::getPosition (struct ln_equ_posn *pos, double JD)
{
  struct ln_equ_posn parallax;
  return getPosition (pos, JD, &parallax);
}

int
EllTarget::getRST (struct ln_rst_time *rst, double JD, double horizon)
{
  if (orbit.e == 1.0)
    {
      struct ln_par_orbit par_orbit;
      par_orbit.q = orbit.a;
      par_orbit.i = orbit.i;
      par_orbit.w = orbit.w;
      par_orbit.omega = orbit.omega;
      par_orbit.JD = orbit.JD;
      return ln_get_par_body_next_rst_horizon (JD, observer, &par_orbit, horizon, rst);
    }
  else if (orbit.e > 1.0)
    {
      struct ln_hyp_orbit hyp_orbit;
      hyp_orbit.q = orbit.a;
      hyp_orbit.e = orbit.e;
      hyp_orbit.i = orbit.i;
      hyp_orbit.w = orbit.w;
      hyp_orbit.omega = orbit.omega;
      hyp_orbit.JD = orbit.JD;
      return ln_get_hyp_body_next_rst_horizon (JD, observer, &hyp_orbit, horizon, rst);
    }
  return ln_get_ell_body_next_rst_horizon (JD, observer, &orbit, horizon, rst);
}

void
EllTarget::printExtra (std::ostream & _os, double JD)
{
  struct ln_equ_posn pos, parallax;
  getPosition (&pos, JD, &parallax);
  _os
    << InfoVal<TimeJD> ("EPOCH", TimeJD (orbit.JD));
  if (orbit.e < 1.0)
  {
    _os 
      << InfoVal<double> ("n", orbit.n)
      << InfoVal<double> ("a", orbit.a);
  }
  else if (orbit.e > 1.0)
  {
    _os
      << InfoVal<double> ("q", orbit.a);
  }  
  _os  
    << InfoVal<double> ("e", orbit.e)
    << InfoVal<double> ("Peri.", orbit.w)
    << InfoVal<double> ("Node", orbit.omega)
    << InfoVal<double> ("Incl.", orbit.i)
    << std::endl
    << InfoVal<double> ("EARTH DISTANCE", getEarthDistance (JD))
    << InfoVal<double> ("SOLAR DISTANCE", getSolarDistance (JD))
    << InfoVal<LibnovaDegDist> ("PARALLAX RA", LibnovaDegDist (parallax.ra))
    << InfoVal<LibnovaDegDist> ("PARALLAX DEC", LibnovaDegDist (parallax.dec))
    << std::endl;
  Target::printExtra (_os, JD);
}

void
EllTarget::writeToImage (Rts2Image * image)
{
  Target::writeToImage (image);
  image->setValue ("ELL_EPO", orbit.JD, "epoch of the orbit");
  if (orbit.e < 1.0)
  {
    image->setValue ("ELL_N", orbit.n, "n parameter of the orbit");
    image->setValue ("ELL_A", orbit.a, "a parameter of the orbit");
  }
  else if (orbit.e > 1.0)
  {
    image->setValue ("ELL_Q", orbit.a, "q parameter of the orbit");
  }
  image->setValue ("ELL_E", orbit.e, "orbit eccentricity");
  image->setValue ("ELL_PERI", orbit.w, "perihelium parameter");
  image->setValue ("ELL_NODE", orbit.omega, "node angle");
  image->setValue ("ELL_INCL", orbit.i, "orbit inclination");
}

double
EllTarget::getEarthDistance (double JD)
{
  if (orbit.e == 1.0)
    {
      struct ln_par_orbit par_orbit;
      par_orbit.q = orbit.a;
      par_orbit.i = orbit.i;
      par_orbit.w = orbit.w;
      par_orbit.omega = orbit.omega;
      par_orbit.JD = orbit.JD;
      return ln_get_par_body_earth_dist (JD, &par_orbit);
    }
  else if (orbit.e > 1.0)
    {
      struct ln_hyp_orbit hyp_orbit;
      hyp_orbit.q = orbit.a;
      hyp_orbit.e = orbit.e;
      hyp_orbit.i = orbit.i;
      hyp_orbit.w = orbit.w;
      hyp_orbit.omega = orbit.omega;
      hyp_orbit.JD = orbit.JD;
      return ln_get_hyp_body_earth_dist (JD, &hyp_orbit);
    }
  return ln_get_ell_body_earth_dist (JD, &orbit);
}

double
EllTarget::getSolarDistance (double JD)
{
  if (orbit.e == 1.0)
    {
      struct ln_par_orbit par_orbit;
      par_orbit.q = orbit.a;
      par_orbit.i = orbit.i;
      par_orbit.w = orbit.w;
      par_orbit.omega = orbit.omega;
      par_orbit.JD = orbit.JD;
      return ln_get_par_body_solar_dist (JD, &par_orbit);
    }
  else if (orbit.e > 1.0)
    {
      struct ln_hyp_orbit hyp_orbit;
      hyp_orbit.q = orbit.a;
      hyp_orbit.e = orbit.e;
      hyp_orbit.i = orbit.i;
      hyp_orbit.w = orbit.w;
      hyp_orbit.omega = orbit.omega;
      hyp_orbit.JD = orbit.JD;
      return ln_get_hyp_body_solar_dist (JD, &hyp_orbit);
    }
  return ln_get_ell_body_solar_dist (JD, &orbit);

}
