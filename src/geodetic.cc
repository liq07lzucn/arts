/* Copyright (C) 2012
   Patrick Eriksson <Patrick.Eriksson@chalmers.se>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA. */




/*===========================================================================
  === File description 
  ===========================================================================*/

/*!
   \file   geodetic.cc
   \author Patrick Eriksson <Patrick.Eriksson@chalmers.se>
   \date   2012-02-06 

   This file contains functions associated with the reference ellipsoid,
   conversion between latitudes and similar stuff.
*/



/*===========================================================================
  === External declarations
  ===========================================================================*/

#include <cmath>
#include <stdexcept>
#include "geodetic.h"
#include "math_funcs.h"
#include "ppath.h"

extern const Numeric DEG2RAD;
extern const Numeric RAD2DEG;



/*===========================================================================
  === 2D functions
  ===========================================================================*/

// The 2D case is treated as being the 3D x/z-plane. That is, y-coordinate is
// skipped. For simplicity, the angle coordinate is denoted as latitude.
// However, the latitude is here not limited to [-90,90]. It is cyclic and can
// have any value. The input *lat0* is used to shift the output from atan2 with
// n*360 to return what should be the expected latitude. That is, it is assumed
// that no operation moves the latitude more than 180 degrees from the initial
// value *lat0*.


//! cart2pol
/*! 
   The inverse of *pol2cart*. 
   
   A 2D version of cart2sph 

   \param   r     Out: Radius of position.
   \param   lat   Out: Latitude of position.
   \param   x     x-coordinate of position.
   \param   z     z-coordinate of position.
   \param   lat0  Original latitude.
   \param   za0   Original zenith angle.

   \author Patrick Eriksson
   \date   2012-03-20
*/
void cart2pol(
            double&   r,
            double&   lat,
      const double&   x,
      const double&   z,
      const double&   lat0,
      const double&   za0 )
{
  r   = sqrt( x*x + z*z );

  // Zenith and nadir cases
  if( za0 < ANGTOL  ||  za0 > 180-ANGTOL  )
    { lat = lat0; }

  else
    { // Latitude inside [0,360]
      lat = RAD2DEG * atan2( z, x );
      // Shift with n*360 to get as close as possible to lat0
      lat = lat - 360.0 * Numeric( round( ( lat -lat0 ) / 360.0 ) );
    }
}



//! cart2poslos
/*! 
   2D version of the 3D *cart2poslos*.

   \param   r     Out: Radius of observation position.
   \param   lat   Out: Latitude of observation position.
   \param   za    Out: LOS zenith angle at observation position.
   \param   x     x-coordinate of observation position.
   \param   z     z-coordinate of observation position.
   \param   dx    x-part of LOS unit vector.
   \param   dz    z-part of LOS unit vector.
   \param   ppc   Propagation path constant (r*sin(za))
   \param   lat0  Original latitude.
   \param   za0   Original zenith angle.

   \author Patrick Eriksson
   \date   2012-03-21
*/
void cart2poslos(
             double&   r,
             double&   lat,
             double&   za,
       const double&   x,
       const double&   z,
       const double&   dx,
       const double&   dz,
       const double&   ppc,
       const double&   lat0,
       const double&   za0 )
{
  r   = sqrt( x*x + z*z );

  // Zenith and nadir cases
  if( za0 < ANGTOL  ||  za0 > 180-ANGTOL  )
    { 
      lat = lat0;
      za  = za0; 
    }

  else
    {
      lat = RAD2DEG * atan2( z, x );

      const double   latrad = DEG2RAD * lat;
      const double   coslat = cos( latrad );
      const double   sinlat = sin( latrad );
      const double   dr     = coslat*dx + sinlat*dz;

      // Use ppc for max accuracy, but dr required to resolve if up- 
      // and downward cases
      za = RAD2DEG * asin( ppc / r );
      if( isnan( za ) )
        { za = 90; }
      if( dr < 0 )
        {
          za = 180.0 - za;
          if( za0 < 0 )
            { za = - za; }
        }

      // The difference below can at least be 3e-6 for tangent points 
      assert( abs( za - RAD2DEG*acos(dr) ) < 1e-4 );
    }
}



//! distance2D
/*! 
   The distance between two 2D points.
   
   The two latitudes can deviate with max 180 degrees.

   \param   l     Out: The distance
   \param   r1    Radius of position 1
   \param   lat1  Latitude of position 1
   \param   r2    Radius of position 2
   \param   lat2  Latitude of position 2

   \author Patrick Eriksson
   \date   2012-03-20
*/
void distance2D(
            double&   l,
      const double&   r1,
      const double&   lat1,
      const double&   r2,
      const double&   lat2 )
{
  assert( abs( lat2 - lat1 ) <= 180 );

  Numeric x1, z1, x2, z2;
  pol2cart( x1, z1, r1, lat1 );
  pol2cart( x2, z2, r2, lat2 );

  l = sqrt( pow( x2-x1, 2.0 ) + pow( x2-x1, 2.0 ) ); 
}



//! geomtanpoint2d
/*! 
   Position of the tangent point for 3D cases.

   Calculates the 3D geometrical tangent point for arbitrary reference
   ellipsiod. That is, a spherical planet is not assumed. The tangent
   point is thus defined as the point closest to the ellipsoid (not as the
   ppoint with za=90).
  
   Geocentric coordinates are used for both sensor and tangent point
   positions.

   The algorithm used for non-spherical cases is derived by Nick Lloyd at
   University of Saskatchewan, Canada (nick.lloyd@usask.ca), and is part of
   the operational code for both OSIRIS and SMR on-board- the Odin
   satellite.

   The zenith angle must be >= 90.

   \param   r_tan       Out: Radius of tangent point.
   \param   lat_tan     Out: Latitude of tangent point.
   \param   lon_tan     Out: Longitude of tangent point.
   \param   r           Radius of observation position.
   \param   lat         Latitude of observation position.
   \param   lon         Longitude of observation position.
   \param   za          LOS zenith angle at observation position.
   \param   aa          LOS azimuth angle at observation position.

   \author Patrick Eriksson
   \date   2012-02-12
*/
/*
void geomtanpoint2d( 
             double&    r_tan,
             double&    lat_tan,
     ConstVectorView    refellipsoid,
       const double&    r,
       const double&    lat,
       const double&    za )
{
  assert( refellipsoid.nelem() == 2 );
  assert( refellipsoid[0] > 0 );
  assert( refellipsoid[1] >= 0 );
  assert( refellipsoid[1] < 1 );
  assert( r > 0 );
  assert( za >= 90 );
                                // e=1e-7 corresponds to that polar radius
  if( refellipsoid[1] < 1e-7 )  // less than 1 um smaller than equatorial 
    {                           // one for the Earth
      r_tan = geometrical_ppc( r, za );
      if( za > 0 )
        { lat_tan = geompath_lat_at_za( za, lat, 90 ); }
      else
        { lat_tan = geompath_lat_at_za( za, lat, -90 ); }
    }

  else
    {
      assert( 0 );  // To be implemented
    }  
}  
*/


//! pol2cart
/*! 
   Conversion from polar to cartesian coordinates.

   The cartesian coordinate system is defined such as the x-axis goes along
   lat=0 and lon=0 and z-axis goes along lat=90.

   \param   x     Out: x position.
   \param   z     Out: z position.
   \param   r     Radius.
   \param   lat   Latitude.

   \author Patrick Eriksson
   \date   2012-03-20
*/
void pol2cart(
            double&   x,
            double&   z,
      const double&   r,
      const double&   lat )
{
  assert( r > 0 );

  const double   latrad = DEG2RAD * lat;

  x = r * cos( latrad );  
  z = r * sin( latrad );
}



//! poslos2cart
/*! 
   2D version of poslos2cart

   \param   x     Out: x-coordinate of observation position.
   \param   z     Out: z-coordinate of observation position.
   \param   dx    Out: x-part of LOS unit vector.
   \param   dz    Out: z-part of LOS unit vector.
   \param   r     Radius of observation position.
   \param   lat   Latitude of observation position.
   \param   za    LOS zenith angle at observation position.

   \author Patrick Eriksson
   \date   2012-03-20
*/
void poslos2cart(
             double&   x,
             double&   z,
             double&   dx,
             double&   dz,
       const double&   r,
       const double&   lat,
       const double&   za )
{
  assert( r > 0 );
  assert( za >= -180 && za<=180 );

  const double   latrad = DEG2RAD * lat;
  const double   zarad  = DEG2RAD * za;

  const double   coslat = cos( latrad );
  const double   sinlat = sin( latrad );
  const double   cosza  = cos( zarad );
  const double   sinza  = sin( zarad );

  // This part as pol2cart but uses local variables
  x = r * coslat;  
  z = r * sinlat;

  const double   dr   = cosza;
  const double   dlat = sinza;         // r-term cancel out below

  dx = coslat * dr - sinlat * dlat;
  dz = sinlat * dr + coslat * dlat;
}





/*===========================================================================
  === 3D functions
  ===========================================================================*/

//! cart2poslos
/*! 
   The inverse of *poslos2cart*.

   Beside the cartesian coordinates (x,y,z,dx,dy,dz), the function takes as
   input information about the original position and LOS. The later data are
   used to improve the accuracy. For example, for zenith and nadir cases it is
   ensured that the latitude and longitude are not changed. This makes the
   function slower, but accuarcy is favoured as zenith, nadir, north and south
   line-of-sights are especially tricky as they can go along a grid box
   boundary and the smallest rounding error can move the path from one grid box
   to the neighbouring one.

   \param   r     Out: Radius of observation position.
   \param   lat   Out: Latitude of observation position.
   \param   lon   Out: Longitude of observation position.
   \param   za    Out: LOS zenith angle at observation position.
   \param   aa    Out: LOS azimuth angle at observation position.
   \param   x     x-coordinate of observation position.
   \param   y     y-coordinate of observation position.
   \param   z     z-coordinate of observation position.
   \param   dx    x-part of LOS unit vector.
   \param   dy    y-part of LOS unit vector.
   \param   dz    z-part of LOS unit vector.
   \param   ppc   Propagation path constant (r*sin(za))
   \param   lat0  Original latitude.
   \param   lon0  Original longitude.
   \param   za0   Original zenith angle.
   \param   aa0   Original azimuth angle.

   \author Patrick Eriksson
   \date   2002-12-30
*/
void cart2poslos(
             double&   r,
             double&   lat,
             double&   lon,
             double&   za,
             double&   aa,
       const double&   x,
       const double&   y,
       const double&   z,
       const double&   dx,
       const double&   dy,
       const double&   dz,
       const double&   ppc,
       const double&   lat0,
       const double&   lon0,
       const double&   za0,
       const double&   aa0 )
{
  r   = sqrt( x*x + y*y + z*z );

  // Zenith and nadir cases
  if( za0 < ANGTOL  ||  za0 > 180-ANGTOL  )
    { 
      lat = lat0;
      lon = lon0;
      za  = za0; 
      aa  = aa0; 
    }

  else
    {
      lat = RAD2DEG * asin( z / r );
      lon = RAD2DEG * atan2( y, x );

      bool ns_case = false;
      bool lon_flip = false;

      // Make sure that lon is maintained for N-S cases (if not 
      // starting on a pole)
      if( ( abs(aa0) < ANGTOL  ||  abs(180-aa0) < ANGTOL )  && 
                                             abs( lat0 ) <= POLELAT )
        {
          ns_case = true;
          // Check that not lon changed with 180 deg
          if( abs(lon-lon0) < 1 )
            { lon = lon0; }
          else
            {
              lon_flip = true;
              if( lon0 > 0 )
                { lon = lon0 - 180; }
              else
                { lon = lon0 + 180; }
            }
        }

      const double   latrad = DEG2RAD * lat;
      const double   lonrad = DEG2RAD * lon;
      const double   coslat = cos( latrad );
      const double   sinlat = sin( latrad );
      const double   coslon = cos( lonrad );
      const double   sinlon = sin( lonrad );
      const double   dr     = coslat*coslon*dx + coslat*sinlon*dy + sinlat*dz;

      // Use ppc for max accuracy, but dr required to resolve if up- 
      // and downward cases
      za = RAD2DEG * asin( ppc / r );
      if( isnan( za ) )
        { za = 90; }
      if( dr < 0 )
        { za = 180.0 - za; }

      // The difference below can at least be 3e-6 for tangent points 
      assert( abs( za - RAD2DEG*acos(dr) ) < 1e-4 );

      // For lat = +- 90 the azimuth angle gives the longitude along which 
      // the LOS goes
      if( abs( lat ) >= POLELAT )      
        { aa = RAD2DEG * atan2( dy, dx ); }

      // N-S cases, not starting at a pole
      else if( ns_case )
        { 
          if( !lon_flip )
            { aa = aa0; }
          else
            {
              if( abs(aa0) < ANGTOL )
                { aa = 180; }
              else
                { aa = 0; }
            }
        }

      else
        {
          const double   dlat = -sinlat*coslon/r*dx - sinlat*sinlon/r*dy + 
                                                             coslat/r*dz;
          const double   dlon = -sinlon/coslat/r*dx + coslon/coslat/r*dy;

          aa = RAD2DEG * acos( r * dlat / sin( DEG2RAD * za ) );

          if( isnan( aa ) )
            {
              if( dlat >= 0 )
                { aa = 0; }
              else
                { aa = 180; }
            }
          else if( dlon < 0 )
            { aa = -aa; }
        }
    }
}



//! cart2sph
/*! 
   The inverse of *sph2cart*.

   For definition of lat0, lon0, za0 and aa0, see *cart2poslos*.

   \param   r     Out: Radius of observation position.
   \param   lat   Out: Latitude of observation position.
   \param   lon   Out: Longitude of observation position.
   \param   x     x-coordinate of observation position.
   \param   y     y-coordinate of observation position.
   \param   z     z-coordinate of observation position.
   \param   lat0  Original latitude.
   \param   lon0  Original longitude.
   \param   za0   Original zenith angle.
   \param   aa0   Original azimuth angle.

   \author Patrick Eriksson
   \date   2002-12-30
*/
void cart2sph(
             double&   r,
             double&   lat,
             double&   lon,
       const double&   x,
       const double&   y,
       const double&   z,
       const double&   lat0,
       const double&   lon0,
       const double&   za0,
       const double&   aa0 )
{
  r   = sqrt( x*x + y*y + z*z );

  // Zenith and nadir cases
  if( za0 < ANGTOL  ||  za0 > 180-ANGTOL  )
    { 
      lat = lat0;
      lon = lon0;
    }

  else
    {
      lat = RAD2DEG * asin( z / r );
      lon = RAD2DEG * atan2( y, x );

      // Make sure that lon is maintained for N-S cases (if not 
      // starting on a pole)
      if( ( abs(aa0) < ANGTOL  ||  abs(180-aa0) < ANGTOL )  && 
                                             abs( lat0 ) <= POLELAT )
        {
          // Check that not lon changed with 180 deg
          if( abs(lon-lon0) < 1 )
            { lon = lon0; }
          else
            {
              if( lon0 > 0 )
                { lon = lon0 - 180; }
              else
                { lon = lon0 + 180; }
            }
        }
    }
}



//! distance3D
/*! 
   The distance between two 3D points.
   
   \param   l     Out: The distance
   \param   r1    Radius of position 1
   \param   lat1  Latitude of position 1
   \param   lon1  Longitude of position 1
   \param   r2    Radius of position 2
   \param   lat2  Latitude of position 2
   \param   lon2  Longitude of position 2

   \author Patrick Eriksson
   \date   2012-03-20
*/
void distance3D(
            double&   l,
      const double&   r1,
      const double&   lat1,
      const double&   lon1,
      const double&   r2,
      const double&   lat2,
      const double&   lon2 )
{
  Numeric x1, y1, z1, x2, y2, z2;
  sph2cart( x1, y1, z1, r1, lat1, lon1 );
  sph2cart( x2, y2, z2, r2, lat2, lon2 );

  l = sqrt( pow( x2-x1, 2.0 ) + pow( y2-y1, 2.0 ) + pow( x2-x1, 2.0 ) ); 
}



//! geompath_tanpos_3d
/*! 
   Position of the tangent point for 3D cases.

   The zenith angle must be >= 90.

   \param   r_tan       Out: Radius of tangent point.
   \param   lat_tan     Out: Latitude of tangent point.
   \param   lon_tan     Out: Longitude of tangent point.
   \param   l_tan       Out: Distance along path to the tangent point.
   \param   r           Radius of observation position.
   \param   lat         Latitude of observation position.
   \param   lon         Longitude of observation position.
   \param   za          LOS zenith angle at observation position.
   \param   aa          LOS azimuth angle at observation position.
   \param   ppc         Geometrical propagation path constant.

   \author Patrick Eriksson
   \date   2002-12-31
*/
void geompath_tanpos_3d( 
             double&    r_tan,
             double&    lat_tan,
             double&    lon_tan,
             double&    l_tan,
       const double&    r,
       const double&    lat,
       const double&    lon,
       const double&    za,
       const double&    aa,
       const double&    ppc )
{
  assert( za >= 90 );
  assert( r >= ppc );

  double   x, y, z, dx, dy, dz; 

  poslos2cart( x, y, z, dx, dy, dz, r, lat, lon, za, aa );

  l_tan = sqrt( r*r - ppc*ppc );

  cart2sph( r_tan, lat_tan, lon_tan, x+dx*l_tan, y+dy*l_tan, z+dz*l_tan,
            lat, lon, za, aa );
}



//! geomtanpoint
/*! 
   Position of the tangent point for 3D cases.

   Calculates the 3D geometrical tangent point for arbitrary reference
   ellipsiod. That is, a spherical planet is not assumed. The tangent
   point is thus defined as the point closest to the ellipsoid (not as the
   ppoint with za=90).
  
   Geocentric coordinates are used for both sensor and tangent point
   positions.

   The algorithm used for non-spherical cases is derived by Nick Lloyd at
   University of Saskatchewan, Canada (nick.lloyd@usask.ca), and is part of
   the operational code for both OSIRIS and SMR on-board- the Odin
   satellite.

   The zenith angle must be >= 90.

   \param   r_tan       Out: Radius of tangent point.
   \param   lat_tan     Out: Latitude of tangent point.
   \param   lon_tan     Out: Longitude of tangent point.
   \param   r           Radius of observation position.
   \param   lat         Latitude of observation position.
   \param   lon         Longitude of observation position.
   \param   za          LOS zenith angle at observation position.
   \param   aa          LOS azimuth angle at observation position.

   \author Patrick Eriksson
   \date   2012-02-12
*/
/*
void geomtanpoint( 
             double&    r_tan,
             double&    lat_tan,
             double&    lon_tan,
     ConstVectorView    refellipsoid,
       const double&    r,
       const double&    lat,
       const double&    lon,
       const double&    za,
       const double&    aa )
{
  assert( refellipsoid.nelem() == 2 );
  assert( refellipsoid[0] > 0 );
  assert( refellipsoid[1] >= 0 );
  assert( refellipsoid[1] < 1 );
  assert( r > 0 );
  assert( za >= 90 );

  if( refellipsoid[1] < 1e-7 )        // e=1e-7 corresponds to that polar radius
    {                                 // less than 1 um smaller than equatorial 
      double   x, y, z, dx, dy, dz;   // one for the Earth

      poslos2cart( x, y, z, dx, dy, dz, r, lat, lon, za, aa );
   
      const double ppc   = r * sin( DEG2RAD * abs(za) );
      const double l_tan = sqrt( r*r - ppc*ppc );
   
      cart2sph( r_tan, lat_tan, lon_tan, x+dx*l_tan, y+dy*l_tan, z+dz*l_tan );
    }

  else
    {
      // Equatorial and polar radii squared:
      const double a2 = refellipsoid[0]*refellipsoid[0];
      const double b2 = a2 * ( 1 - refellipsoid[1]*refellipsoid[1] ); 

      Vector X(3), xunit(3), yunit(3), zunit(3);

      poslos2cart( X[0], X[1], X[2], xunit[0], xunit[1], xunit[2], 
                                                         r, lat, lon, za, aa );
      cross( zunit, xunit, X );
      unitl( zunit );                // Normalisation of length to 1

      cross( yunit, zunit, xunit );
      unitl( yunit );                // Normalisation of length to 1

            double x   = X[0];
            double y   = X[1];
      const double w11 = xunit[0];
      const double w12 = yunit[0];
      const double w21 = xunit[1];
      const double w22 = yunit[1];
      const double w31 = xunit[2];
      const double w32 = yunit[2];

      const double yr = X * yunit;
      const double xr = X * xunit;

      const double A = (w11*w11 + w21*w21)/a2 + w31*w31/b2;
      const double B = 2.0*((w11*w12 + w21*w22)/a2 + (w31*w32)/b2);
      const double C = (w12*w12 + w22*w22)/a2 + w32*w32/b2;

      if( B == 0.0 )
        { x = 0.0; }
      else 
        { 
          const double K      = -2.0*A/B; 
          const double factor = 1.0/(A+(B+C*K)*K);
          x = sqrt(factor);
          y = K*x;
        }

      const double dist1 = (xr-X[0])*(xr-X[0]) + (yr-y)*(yr-y);
      const double dist2 = (xr+X[0])*(xr+X[0]) + (yr+y)*(yr+y);
 	
      if( dist1 > dist2 )
        { x = -x; }

      cart2sph( r_tan, lat_tan, lon_tan, w11*x + w12*yr, w21*x + w22*yr,
                                                         w31*x + w32*yr );
    }
}
*/



//! poslos2cart
/*! 
   Conversion from position and LOS to cartesian coordinates

   A position (in geographical coordinates) and LOS are converted to a
   cartesian position and a viewing vector. The viewing direction is the
   the vector [dx,dy,dz]. This vector is normalised (it has length 1).

   See the user guide for definition on the zenith and azimuth angles.

   \param   x     Out: x-coordinate of observation position.
   \param   y     Out: y-coordinate of observation position.
   \param   z     Out: z-coordinate of observation position.
   \param   dx    Out: x-part of LOS unit vector.
   \param   dy    Out: y-part of LOS unit vector.
   \param   dz    Out: z-part of LOS unit vector.
   \param   r     Radius of observation position.
   \param   lat   Latitude of observation position.
   \param   lon   Longitude of observation position.
   \param   za    LOS zenith angle at observation position.
   \param   aa    LOS azimuth angle at observation position.

   \author Patrick Eriksson
   \date   2002-12-30
*/
void poslos2cart(
             double&   x,
             double&   y,
             double&   z,
             double&   dx,
             double&   dy,
             double&   dz,
       const double&   r,
       const double&   lat,
       const double&   lon,
       const double&   za,
       const double&   aa )
{
  assert( r > 0 );
  assert( abs( lat ) <= 90 );
  assert( abs( lon ) <= 360 );
  assert( za >= 0 && za<=180 );

  // lat = +-90 
  // For lat = +- 90 the azimuth angle gives the longitude along which the
  // LOS goes
  if( abs( lat ) > POLELAT )
    {
      const double   s = sign( lat );

      x = 0;
      y = 0;
      z = s * r;

      dz = s * cos( DEG2RAD * za );
      dx = sin( DEG2RAD * za );
      dy = dx * sin( DEG2RAD * aa );
      dx = dx * cos( DEG2RAD * aa );
    }

  else
    {
      const double   latrad = DEG2RAD * lat;
      const double   lonrad = DEG2RAD * lon;
      const double   zarad  = DEG2RAD * za;
      const double   aarad  = DEG2RAD * aa;

      const double   coslat = cos( latrad );
      const double   sinlat = sin( latrad );
      const double   coslon = cos( lonrad );
      const double   sinlon = sin( lonrad );
      const double   cosza  = cos( zarad );
      const double   sinza  = sin( zarad );
      const double   cosaa  = cos( aarad );
      const double   sinaa  = sin( aarad );

      // This part as sph2cart but uses local variables
      x = r * coslat;   // Common term for x and y
      y = x * sinlon;
      x = x * coslon;
      z = r * sinlat;

      const double   dr   = cosza;
      const double   dlat = sinza * cosaa;         // r-term cancel out below
      const double   dlon = sinza * sinaa / coslat; 

      dx = coslat*coslon * dr - sinlat*coslon * dlat - coslat*sinlon * dlon;
      dz =        sinlat * dr +        coslat * dlat;
      dy = coslat*sinlon * dr - sinlat*sinlon * dlat + coslat*coslon * dlon;
    }
}



//! refell2r
/*!
    Reference ellipsoid radius, directly from *refellipsoid*.

    Gives the distance from the Earth's centre and the reference ellipsoid
    as a function of geoCENTRIC latitude. 

    For 1D, extract r directly as refellipsoid[0] to save time.

    For 2D and 3D and the position is inside the atmosphere, use *refell2d* and
    *refell3d* for highest internal consistency.

    \return                 Ellispoid radius
    \param  refellipsoid    In: As the WSV with same name.
    \param  latitude        In: A geoecentric latitude.

    \author Patrick Eriksson 
    \date   2012-02-07
*/
double refell2r(
       ConstVectorView  refellipsoid,
       const double&   lat )
{
  assert( refellipsoid.nelem() == 2 );
  assert( refellipsoid[0] > 0 );
  assert( refellipsoid[1] >= 0 );
  assert( refellipsoid[1] < 1 );

  if( refellipsoid[1] < 1e-7 )  // e=1e-7 corresponds to that polar radius  
    {                           // less than 1 um smaller than equatorial 
      return refellipsoid[0];   // one for the Earth
    }

  else
    {
      const double   c = 1 - refellipsoid[1]*refellipsoid[1];
      const double   b = refellipsoid[0] * sqrt( c );
      const double   v = DEG2RAD * lat;
      const double   ct = cos( v );
      const double   st = sin( v );
      
      return b / sqrt( c*ct*ct + st*st );
    }
}



//! refell2d
/*!
    Reference ellipsoid radius for points inside 2D atmospheres.

    To be consistent with the ppath calculations, the ellipsoid radius shall be
    treated to vary linear between the latitude grid points. This function
    performs this operation. The latitude position is specified by its grid
    position (*gp*).

    \return                 Ellispoid radius
    \param  refellipsoid    In: As the WSV with same name.
    \param  lat_grid        In: As the WSV with same name.
    \param  gp              In: Latitude grid position.

    \author Patrick Eriksson 
    \date   2012-02-09
*/
double refell2d(
       ConstVectorView  refellipsoid,
       ConstVectorView  lat_grid,
       const GridPos    gp )
{
  if( gp.fd[0] == 0 )
    return refell2r(refellipsoid,lat_grid[gp.idx]);
  else if( gp.fd[0] == 1 )
    return refell2r(refellipsoid,lat_grid[gp.idx+1]);
  else
    return gp.fd[1] * refell2r(refellipsoid,lat_grid[gp.idx]) +
           gp.fd[0] * refell2r(refellipsoid,lat_grid[gp.idx+1]);
}       



//! sph2cart
/*! 
   Conversion from spherical to cartesian coordinates.

   The cartesian coordinate system is defined such as the x-axis goes along
   lat=0 and lon=0, the z-axis goes along lat=0 and lon=90, and z-axis goes
   along lat=90. 

   \param   x     Out: x position.
   \param   y     Out: y position.
   \param   z     Out: z position.
   \param   r     Radius.
   \param   lat   Latitude.
   \param   lon   Longitude.

   \author Patrick Eriksson
   \date   2002-12-17
*/
void sph2cart(
            double&   x,
            double&   y,
            double&   z,
      const double&   r,
      const double&   lat,
      const double&   lon )
{
  assert( r > 0 );
  assert( abs( lat ) <= 90 );
  assert( abs( lon ) <= 360 );

  const double   latrad = DEG2RAD * lat;
  const double   lonrad = DEG2RAD * lon;

  x = r * cos( latrad );   // Common term for x and z
  y = x * sin( lonrad );
  x = x * cos( lonrad );
  z = r * sin( latrad );
}







