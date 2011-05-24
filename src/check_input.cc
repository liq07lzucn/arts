/* Copyright (C) 2002-2008
   Patrick Eriksson <patrick.eriksson@chalmers.se>
   Stefan Buehler   <sbuehler@ltu.se>

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
  \file   check_input.cc
  \author Patrick Eriksson <Patrick.Eriksson@rss.chalmers.se>
  \date 2002-04-15 

  General functions to check the size and logic of input to functions.
*/



/*===========================================================================
  === External declarations
  ===========================================================================*/

#include <cmath>
#include <stdexcept>
#include "check_input.h"
#include "array.h"
#include "logic.h"
#include "gridded_fields.h"

extern const Numeric DEG2RAD;
extern const Index GFIELD3_P_GRID;
extern const Index GFIELD3_LAT_GRID;
extern const Index GFIELD3_LON_GRID;

/*===========================================================================
  === Functions for Index
  ===========================================================================*/

//! chk_if_bool 
/*! 
    Checks that a variable of type Index has the value 0 or 1.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Index.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_if_bool( 
        const String&   x_name,
        const Index&    x )
{
  if ( !is_bool(x) )
    {
      ostringstream os;
      os << "The variable *" << x_name <<  "* must be a boolean (0 or 1).\n" 
         << "The present value of *"<< x_name <<  "* is " << x << ".";
      throw runtime_error( os.str() );
    }
}

//! chk_if_in_range
/*! 
    Checks that a variable of type Index has a value inside the specified
    range.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Index.
    \param    x_low    Lowest allowed value for x.
    \param    x_high   Highest allowed value for x.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_if_in_range( 
        const String&   x_name,
        const Index&    x, 
        const Index&    x_low, 
        const Index&    x_high )
{
  if ( (x<x_low) || (x>x_high) )
    {
      ostringstream os;
      os << "The variable *" << x_name <<  "* must fulfill:\n"
         << "   " << x_low << " <= " << x_name << " <= " << x_high << "\n" 
         << "The present value of *"<< x_name <<  "* is " << x << ".";
      throw runtime_error( os.str() );
    }
}

//! chk_if_increasing
/*! 
    Checks if an ArrayOfIndex is strictly increasing. Cloned from
    Patricks similar function for Vector.

    Duplicated values are not allowed.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type ArrayOfIndex.

    \author Stefan Buehler
    \date   2007-05-18
*/
void chk_if_increasing( 
        const String&       x_name,
        const ArrayOfIndex& x ) 
{
  if ( !is_increasing(x) )
    {
      ostringstream os;
      os << "The ArrayOfIndex *" << x_name <<  "* must have strictly\n"
         << "increasing values, but this is not the case.\n";
      os << "x = " << x << "\n";
      throw runtime_error( os.str() );
    }
}


/*===========================================================================
  === Functions for Numeric
  ===========================================================================*/

//! chk_not_negative 
/*! 
    Checks that a variable of type Numeric is 0 or positive.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Numeric.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_not_negative( 
        const String&    x_name,
        const Numeric&   x ) 
{
  if ( x < 0 )
    {
      ostringstream os;
      os << "The variable *" << x_name <<  "* must be >= 0.\n"
         << "The present value of *"<< x_name <<  "* is " << x << ".";
      throw runtime_error( os.str() );
    }
}



//! chk_if_in_range
/*! 
    Checks that a variable of type Numeric has a value inside the specified
    range.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Numeric.
    \param    x_low    Lowest allowed value for x.
    \param    x_high   Highest allowed value for x.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_if_in_range( 
        const String&    x_name,
        const Numeric&   x, 
        const Numeric&   x_low, 
        const Numeric&   x_high )
{
  if ( (x<x_low) || (x>x_high) )
    {
      ostringstream os;
      os << "The variable *" << x_name <<  "* must fulfill:\n"
         << "   " << x_low << " <= " << x_name << " <= " << x_high << "\n" 
         << "The present value of *"<< x_name <<  "* is " << x << ".";
      throw runtime_error( os.str() );
    }
}



/*===========================================================================
  === Functions for Vector
  ===========================================================================*/

//! chk_vector_length
/*! 
    Checks that a vector has the specified length.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Vector.
    \param    l        The expected length of x.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_vector_length( 
        const String&      x_name,
        ConstVectorView    x,
        const Index&       l ) 
{
  if ( x.nelem() != l )
    {
      ostringstream os;
      os << "The vector *" << x_name <<  "* must have the length " << l 
         << ".\n" 
         << "The present length of *"<< x_name <<  "* is " << x.nelem() << ".";
      throw runtime_error( os.str() );
    }
}



//! chk_vector_length
/*! 
    Checks if two vectors have the same length.

    The function gives an error message if this is not the case.

    \param    x1_name   The name of the first vector
    \param    x2_name   The name of the second vector
    \param    x1        The first vector.
    \param    x2        The second vector.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_vector_length( 
        const String&      x1_name,
        const String&      x2_name,
        ConstVectorView    x1, 
        ConstVectorView    x2 ) 
{
  if ( x1.nelem() != x2.nelem() )
    {
      ostringstream os;
      os << "The vectors *" << x1_name <<  "* and *" << x2_name 
         <<  "* must have the same length.\n"
         << "The length of *"<< x1_name <<  "* is " << x1.nelem() << ".\n"
         << "The length of *"<< x2_name <<  "* is " << x2.nelem() << ".";
      throw runtime_error( os.str() );
    }
}



//! chk_if_increasing
/*! 
    Checks if a vector is strictly increasing.

    Duplicated values are not allowed.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Vector.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_if_increasing( 
        const String&      x_name,
        ConstVectorView    x ) 
{
  if ( !is_increasing(x) )
    {
      ostringstream os;
      os << "The vector *" << x_name <<  "* must have strictly\n"
         << "increasing values, but this is not the case.\n";
      os << "x = " << x << "\n";
      throw runtime_error( os.str() );
    }
}



//! chk_if_decreasing
/*! 
    Checks if a vector is strictly decreasing.

    Duplicated values are not allowed.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A variable of type Vector.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_if_decreasing( 
        const String&      x_name,
        ConstVectorView    x ) 
{
  if ( !is_decreasing(x) )
    {
      ostringstream os;
      os << "The vector *" << x_name <<  "* must have strictly\ndecreasing "
         << "values, but this is not the case.\n";
      throw runtime_error( os.str() );
    }
}

//! chk_if_equal
/*!
 * Checks if two vectors are equal within a margin.
 *
 * \param   x1_name The name of the first variable (used in error message).
 * \param   x2_name The name of the second variable (used in error message).
 * \param   v1      First vector
 * \param   v2      Second vector
 * \param   margin  uncertainty margin. Default: 1e-6
 *
 * \author  Gerrit Holl
 * \date    2011-05-04
*/

void chk_if_equal(
        const String&   x1_name,
        const String&   x2_name,
        ConstVectorView v1,
        ConstVectorView v2,
        Numeric         margin
        )
{
  chk_vector_length(x1_name, x2_name, v1, v2);

  for (Index i = 0; i<v1.nelem(); i++)
  {
    if (abs(v1[i] - v2[i]) > margin)
      {
        ostringstream os;
        os << "Vectors " << x1_name << " and " << x2_name 
           << " differ.\n"
           << x1_name << "[" << i << "]" << " = " << v1[i] << "\n"
           << x2_name << "[" << i << "]" << " = " << v2[i] << "\n"
           << "Difference should not exceed " << margin << "\n";
       throw runtime_error(os.str());
     }
  }
}

/*===========================================================================
  === Functions for interpolation grids
  ===========================================================================*/

//! Check interpolation grids
/*!
  This function checks if old and new grid for an interpolation are
  ok. If not, it throws a detailed runtime error message. This is
  intended for workspace method input variable checking. 
  
  \param[in] which_interpolation A string describing the interpolation for
                                 which the grids are intended. 
  \param[in] old_grid            The original grid.
  \param[in] new_grid            The new grid.
  \param[in] order               Interpolation order. (Default value is 1.)
  \param[in] extpolfac           The extrapolation fraction. See gridpos function
                                 for details. Has a default value, which is
                                 consistent with gridpos.  
  
  \author Stefan Buehler
  \date   2008-11-24 
*/
void chk_interpolation_grids(const String&   which_interpolation,
                             ConstVectorView old_grid,
                             ConstVectorView new_grid,
                             const Index     order,
                             const Numeric&  extpolfac )
{
  const Index n_old = old_grid.nelem();

  ostringstream os;
  os << "There is a problem with the grids for the\n"
     << "following interpolation: " << which_interpolation << ".\n";

  // Old grid must have at least order+1 elements:
  if (n_old < order+1)
    {
      os << "The original grid must have at least " << order+1 << " elements.";
      throw runtime_error( os.str() );
    }
  
  // Decide whether we have an ascending or descending grid:
  bool ascending = ( old_grid[0] <= old_grid[1] );

  // Minimum and maximum allowed value from old grid. (Will include
  // extrapolation tolerance.)
  Numeric og_min, og_max;

  if (ascending)  
    {
      // Old grid must be strictly increasing (no duplicate values.)
      if ( !is_increasing(old_grid) )
        {
          os << "The original grid must be strictly sorted\n"
             << "(no duplicate values). Yours is:\n"
             << old_grid << ".";
          throw runtime_error( os.str() );
        }

      // Limits of extrapolation. 
      og_min = old_grid[0] - 
               extpolfac * ( old_grid[1] - old_grid[0] );
      og_max = old_grid[n_old-1] + 
               extpolfac * ( old_grid[n_old-1] - old_grid[n_old-2] );
    }
  else
    {
      // Old grid must be strictly decreasing (no duplicate values.)
      if ( !is_decreasing(old_grid) )
        {
          os << "The original grid must be strictly sorted\n"
             << "(no duplicate values). Yours is:\n"
             << old_grid << ".";
          throw runtime_error( os.str() );
        }

      // The max is now the first point, the min the last point!
      // I think the sign is right here...
      og_max = old_grid[0] - 
               extpolfac * ( old_grid[1] - old_grid[0] );
      og_min = old_grid[n_old-1] + 
               extpolfac * ( old_grid[n_old-1] - old_grid[n_old-2] );
    }
  
  // Min and max of new grid:
  const Numeric ng_min = min(new_grid);
  const Numeric ng_max = max(new_grid);

  // New grid must be inside old grid (plus extpolfac).
  // (Values right on the edge (ng_min==og_min) are still allowed.)

  if (ng_min < og_min)
    {
      os << "The minimum of the new grid must be inside\n"
         << "the original grid. (We allow a bit of extrapolation,\n"
         << "but not so much).\n"
         << "Minimum of original grid:           " << min(old_grid) << "\n"
         << "Minimum allowed value for new grid: " << og_min << "\n"
         << "Actual minimum of new grid:         " << ng_min;
      throw runtime_error( os.str() );
    }

  if (ng_max > og_max)
    {
      os << "The maximum of the new grid must be inside\n"
         << "the original grid. (We allow a bit of extrapolation,\n"
         << "but not so much).\n"
         << "Maximum of original grid:           " << max(old_grid) << "\n"
         << "Maximum allowed value for new grid: " << og_max << "\n"
         << "Actual maximum of new grid:         " << ng_max;
      throw runtime_error( os.str() );
    }

  // If we get here, than everything should be fine.
}


//! Check interpolation grids
/*!
  This function checks if old and new grid for an interpolation are
  ok. If not, it throws a detailed runtime error message. This is
  intended for workspace method input variable checking. 
  
  This is for the special case that the new grid is just a single
  Numeric, instead of a Vector. ("Red" interpolation.)
  It just calles the other more general chk_interpolation_grids
  function for which both grid arguments are vectors.

  \param[in] which_interpolation A string describing the interpolation for
                                 which the grids are intended. 
  \param[in] old_grid            The original grid.
  \param[in] new_grid            The new grid.
  \param[in] order               Interpolation order. (Default value is 1.)
  \param[in] extpolfac           The extrapolation fraction. See gridpos function
                                 for details. Has a default value, which is
                                 consistent with gridpos.  
  
  \author Stefan Buehler
  \date   2008-11-24 
*/
void chk_interpolation_grids(const String&   which_interpolation,
                             ConstVectorView old_grid,
                             const Numeric&  new_grid,
                             const Index     order,
                             const Numeric&  extpolfac )
{
  Vector v(1, new_grid);
  chk_interpolation_grids(which_interpolation,
                          old_grid,
                          v,
                          order,
                          extpolfac );
}

/*===========================================================================
  === Functions for Matrix
  ===========================================================================*/

//! chk_matrix_ncols
/*! 
    Checks that a matrix has the specified number of columns.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A matrix.
    \param    l        The expected length of x.

    \author Patrick Eriksson 
    \date   2002-05-16
*/
void chk_matrix_ncols( 
        const String&      x_name,
        ConstMatrixView    x,
        const Index&       l ) 
{
  if ( x.ncols() != l )
    {
      ostringstream os;
      os << "The matrix *" << x_name <<  "* must have " << l << " columns,\n"
         << "but the number of columns is " << x.ncols() << ".";
      throw runtime_error( os.str() );
    }
}



//! chk_matrix_nrows
/*! 
    Checks that a matrix has the specified number of rows.

    The function gives an error message if this is not the case.

    \param    x_name   The name of the variable.
    \param    x        A matrix.
    \param    l        The expected length of x.

    \author Patrick Eriksson 
    \date   2002-05-16
*/
void chk_matrix_nrows( 
        const String&      x_name,
        ConstMatrixView    x,
        const Index&       l ) 
{
  if ( x.nrows() != l )
    {
      ostringstream os;
      os << "The matrix *" << x_name <<  "* must have " << l << " rows,\n"
         << "but the number of rows is " << x.nrows() << ".";
      throw runtime_error( os.str() );
    }
}



/*===========================================================================
  === Functions related to atmospheric and surface grids and fields.
  ===========================================================================*/

//! chk_atm_grids 
/*! 
    Checks if the atmospheric grids and the specified atmospheric 
    dimensionality match, and if the grids are ordered correctly.

    The function gives an error message if this is not the case.

    \param    dim          The atmospheric dimensionality.
    \param    p_grid       The pressure grid.
    \param    lat_grid     The latitude grid.
    \param    lon_grid     The longitude grid.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_atm_grids( 
        const Index&      dim,
        ConstVectorView   p_grid,
        ConstVectorView   lat_grid,
        ConstVectorView   lon_grid )
{
  // p_grid
  if( p_grid.nelem() < 2 )
    throw runtime_error( "The length of *p_grid* must be >= 2." );
  chk_if_decreasing( "p_grid", p_grid );

  // lat_grid
  if( dim == 1 )
    {
      if( lat_grid.nelem() > 1 )
        throw runtime_error(
                       "For dim=1, the length of *lat_grid* must be 0 or 1." );
    }
  else
    {
      if( lat_grid.nelem() < 2 )
        throw runtime_error(
                          "For dim>1, the length of *lat_grid* must be >= 2.");
      chk_if_increasing( "lat_grid", lat_grid );
    }

  // lon_grid
  if( dim < 3 )
    { 
      if( lon_grid.nelem() > 1 )
        throw runtime_error(
                       "For dim<3, the length of *lon_grid* must be 0 or 1." );
    }
  else
    {
      if( lon_grid.nelem() < 2 )
        throw runtime_error(
                          "For dim=3, the length of *lon_grid* must be >= 2.");
      chk_if_increasing( "lon_grid", lon_grid );
    }

  // Check that latitude and longitude grids are inside OK ranges for 3D
  if( dim == 3 )
    {
      if( lat_grid[0] < -90 )
        throw runtime_error( 
                  "The latitude grid cannot extend below -90 degrees for 3D" );
      if( lat_grid[lat_grid.nelem() - 1] > 90 )
        throw runtime_error( 
                  "The latitude grid cannot extend above 90 degrees for 3D" );
      if( lon_grid[0] < -360 )
        throw runtime_error( 
                "No longitude (in lon_grid) can be below -360 degrees." );
      if( lon_grid[lon_grid.nelem() - 1] > 360 )
        throw runtime_error( 
                "No longitude (in lon_grid) can be above 360 degrees." );
      if( lon_grid[lon_grid.nelem() - 1]-lon_grid[0] > 360 )
        throw runtime_error( 
         "The longitude grid is not allowed to cover more than 360 degrees." );
    }
}



//! chk_atm_field (simple fields)
/*! 
    Checks if an atmospheric field matches the dimensionality and the grids.

    The function gives an error message if this is not the case.

    \param    x_name       The name of the atmospheric field.
    \param    x            A variable holding an atmospheric field.
    \param    dim          The atmospheric dimensionality.
    \param    p_grid       The pressure grid.
    \param    lat_grid     The latitude grid.
    \param    lon_grid     The longitude grid.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_atm_field( 
        const String&     x_name,
        ConstTensor3View    x, 
        const Index&      dim,
        ConstVectorView   p_grid,
        ConstVectorView   lat_grid,
        ConstVectorView   lon_grid )
{
  // It is assumed that grids OK-ed through chk_atm_grids
  Index npages=p_grid.nelem(), nrows=1, ncols=1;
  if( dim > 1 )
    nrows = lat_grid.nelem();
  if( dim > 2 )
    ncols = lon_grid.nelem();
  if( x.ncols()!=ncols || x.nrows()!=nrows || x.npages()!=npages ) 
    {
      ostringstream os;
      os << "The atmospheric field *" << x_name <<  "* has wrong size.\n"
         << "Expected size is " << npages << " x " << nrows << " x " 
         << ncols << ", while actual size is " << x.npages() << " x " 
         << x.nrows() << " x " << x.ncols() << ".";
      throw runtime_error( os.str() );
    }
  // If all lons are covered, check if cyclic
  if( dim == 3  &&  (lon_grid[ncols-1]-lon_grid[0]) == 360 )
    {
      const Index ic = ncols-1;
      for( Index ip=0; ip<npages; ip++ )
        {
          for( Index ir=0; ir<nrows; ir++ )
            {
              if( fabs(x(ip,ir,ic)-x(ip,ir,0)) > 0 )
                {
                  ostringstream os;
                  os << "The variable *" << x_name <<  "* covers 360 "
                     << "degrees in the longitude direction, but the field "
                     << "seems to deviate between first and last longitude "
                     << "point. The field must be \"cyclic\".";
                  throw runtime_error( os.str() );
                }
            }
        }
    }
}

//! chk_atm_field (fields with one more dimension)
/*! 
    Checks if an atmospheric field matches the dimensionality and the
    grids. This is the version for fields like vmr_field, which are a
    Tensor4, not a Tensor3. (First dimension is the gas species.)

    The function gives an error message if this is not the case.

    \param    x_name       The name of the atmospheric field.
    \param    x            A variable holding an atmospheric field.
    \param    dim          The atmospheric dimensionality.
    \param    nspecies     Number of species.
    \param    p_grid       The pressure grid.
    \param    lat_grid     The latitude grid.
    \param    lon_grid     The longitude grid.

    \author Stefan Buehler, cloned from Patrick Eriksson 
    \date   2002-12-20
*/
void chk_atm_field( 
        const String&   x_name,
        ConstTensor4View  x, 
        const Index&    dim,
        const Index&    nspecies,
        ConstVectorView p_grid,
        ConstVectorView lat_grid,
        ConstVectorView lon_grid )
{
  Index npages=p_grid.nelem(), nrows=1, ncols=1;
  if( dim > 1 )
    nrows = lat_grid.nelem();
  if( dim > 2 )
    ncols = lon_grid.nelem();

  const Index nbooks=nspecies;

  if( x.ncols()!=ncols || x.nrows()!=nrows || x.npages()!=npages ||
      x.nbooks()!=nbooks ) 
    {
      ostringstream os;
      os << "The atmospheric field *" << x_name <<  "* has wrong size.\n"
         << "Expected size is "
         << nbooks << " x " << npages << " x "
         << nrows << " x " << ncols << ",\n"
         << "while actual size is "
         << x.nbooks() << " x " << x.npages() << " x "
         << x.nrows() << " x " << x.ncols() << ".";
      throw runtime_error( os.str() );
    }
  // If all lons are covered, check if cyclic
  if( dim == 3  &&  (lon_grid[ncols-1]-lon_grid[0]) == 360 )
    {
      const Index ic = ncols-1;
      for( Index is=0; is<nspecies; is++ )
        {
        for( Index ip=0; ip<npages; ip++ )
        {
          for( Index ir=0; ir<nrows; ir++ )
            {
              if( fabs(x(is,ip,ir,ic)-x(is,ip,ir,0)) > 0 )
                {
                  ostringstream os;
                  os << "The variable *" << x_name <<  "* covers 360 degrees"
                     << "in the longitude direction, but at least one field "
                     << "seems to deviate between first and last longitude "
                     << "point. The field must be \"cyclic\". This was found "
                     << "for field with index " << is <<" (0-based).";
                  throw runtime_error( os.str() );
                }
            }
        }
        }
    }
}


//! chk_atm_surface
/*! 
    Checks if a surface-type variable matches the dimensionality and the grids.

    Examples of surface-type variables are *z_surface* and *r_geoid*.

    The function gives an error message if this is not the case.

    \param    x_name       The name of the surface-type variable.
    \param    x            The variable holding the surface data.
    \param    dim          The atmospheric dimensionality.
    \param    lat_grid     The latitude grid.
    \param    lon_grid     The longitude grid.

    \author Patrick Eriksson 
    \date   2002-04-15
*/
void chk_atm_surface( 
        const String&     x_name,
        const Matrix&     x, 
        const Index&      dim,
        ConstVectorView   lat_grid,
        ConstVectorView   lon_grid )
{
  Index ncols=1, nrows=1;
  if( dim > 1 )
    nrows = lat_grid.nelem();
  if( dim > 2 )
    ncols = lon_grid.nelem();
  if( x.ncols()!=ncols || x.nrows()!=nrows ) 
    {
      ostringstream os;
      os << "The surface variable *" << x_name <<  "* has wrong size.\n"
         << "Expected size is " << nrows << " x " << ncols << ","
         << " while actual size is " << x.nrows() << " x " << x.ncols() << ".";
      throw runtime_error( os.str() );
    }
  // If all lons are covered, check if cyclic
  if( dim == 3  &&  (lon_grid[ncols-1]-lon_grid[0]) == 360 )
    {
      const Index ic = ncols-1;
      for( Index ir=0; ir<nrows; ir++ )
        {
          if( fabs(x(ir,ic)-x(ir,0)) > 0 )
            {
              ostringstream os;
              os << "The variable *" << x_name <<  "* covers 360 "
                 << "degrees in the longitude direction, but the data "
                 << "seems to deviate between first and last longitude "
                 << "point. The surface must be \"cyclic\".";
              throw runtime_error( os.str() );
            }
        }
    }
}



/*===========================================================================
  === Function(s) releated with the cloud box.
  ===========================================================================*/

//! chk_cloudbox
/*! 
    Checks the consistency of the cloud box workspace variables. 

    The consistency is checked both internally and with respect to the grids.

    The function gives an error message if a consistency failure is found.

    \param    dim                The atmospheric dimensionality.
    \param    p_grid             The pressure grid.
    \param    lat_grid           The latitude grid.
    \param    lon_grid           The longitude grid.
    \param    cloudbox_on        Flag to activate the cloud box.
    \param    cloudbox_limits    Index limits of the cloud box.

    \author Patrick Eriksson 
    \date   2002-05-11
*/
void chk_cloudbox(
        const Index&          dim,
        ConstVectorView       p_grid,
        ConstVectorView       lat_grid,
        ConstVectorView       lon_grid,
        const Index&          cloudbox_on,    
        const ArrayOfIndex&   cloudbox_limits )
{
  // Demanded space between cloudbox and lat and lon edges [degrees]
  const Numeric llmin = 20;

  chk_if_bool(  "cloudbox_on", cloudbox_on );

  if( cloudbox_on )
    {
      if( cloudbox_limits.nelem() != dim*2 )
        {
          ostringstream os;
          os << "The array *cloudbox_limits* has incorrect length.\n"
             << "For dim = " << dim << " the length shall be " << dim*2
             << " but it is " << cloudbox_limits.nelem() << ".";
          throw runtime_error( os.str() );
        }
       if( cloudbox_limits[1]<=cloudbox_limits[0] || cloudbox_limits[0]<0 ||
                                           cloudbox_limits[1]>=p_grid.nelem() )
        {
          ostringstream os;
          os << "Incorrect value(s) for cloud box pressure limit(s) found."
             << "\nValues are either out of range or upper limit is not "
             << "greater than lower limit.\nWith present length of "
             << "*p_grid*, OK values are 0 - " << p_grid.nelem()-1
             << ".\nThe pressure index limits are set to " 
             << cloudbox_limits[0] << " - " << cloudbox_limits[1] << ".";
          throw runtime_error( os.str() );
        }
      if( dim >= 2 )
        {
          const Index n = lat_grid.nelem();
          if( cloudbox_limits[3]<=cloudbox_limits[2] || cloudbox_limits[2]<1 ||
                                                      cloudbox_limits[3]>=n-1 )
            {
              ostringstream os;
              os << "Incorrect value(s) for cloud box latitude limit(s) found."
                 << "\nValues are either out of range or upper limit is not "
                 << "greater than lower limit.\nWith present length of "
                 << "*lat_grid*, OK values are 1 - " << n-2
                 << ".\nThe latitude index limits are set to " 
                 << cloudbox_limits[2] << " - " << cloudbox_limits[3] << ".";
              throw runtime_error( os.str() );
            }
          if( ( lat_grid[cloudbox_limits[2]] - lat_grid[0] < llmin )  &&
              ( dim==2  ||  (dim==3 && lat_grid[0]>-90) ) )
            {
              ostringstream os;
              os << "Too small distance between cloudbox and lower end of\n"
                 << "latitude grid. This distance must be " << llmin 
                 << " degrees. Cloudbox ends at " << lat_grid[cloudbox_limits[2]]
                 << " and latitude grid starts at " << lat_grid[0] << ".";
              throw runtime_error( os.str() );
            }
          if( ( lat_grid[n-1] - lat_grid[cloudbox_limits[3]] < llmin )  &&
              ( dim==2  ||  (dim==3 && lat_grid[n-1]<90) ) )
            {
              ostringstream os;
              os << "Too small distance between cloudbox and upper end of\n"
                 << "latitude grid. This distance must be " << llmin 
                 << "degrees. Cloudbox ends at " << lat_grid[cloudbox_limits[3]]
                 << " and latitude grid ends at " << lat_grid[n-1] << ".";
              throw runtime_error( os.str() );
            }
        }
      if( dim >= 3 )
        {
          const Index n = lon_grid.nelem();
          if( cloudbox_limits[5]<=cloudbox_limits[4] || cloudbox_limits[4]<1 ||
                                                      cloudbox_limits[5]>=n-1 )
            {
              ostringstream os;
              os << "Incorrect value(s) for cloud box longitude limit(s) found"
                 << ".\nValues are either out of range or upper limit is not "
                 << "greater than lower limit.\nWith present length of "
                 << "*lon_grid*, OK values are 1 - " << n-2
                 << ".\nThe longitude limits are set to " 
                 << cloudbox_limits[4] << " - " << cloudbox_limits[5] << ".";
              throw runtime_error( os.str() );
            }
          if( lon_grid[n-1] - lon_grid[0] < 360 )
            {
              const Numeric latmax = max( abs(lat_grid[cloudbox_limits[2]]),
                                          abs(lat_grid[cloudbox_limits[3]]) );
              const Numeric lfac = 1 / cos( DEG2RAD*latmax );
              if( lon_grid[cloudbox_limits[4]]-lon_grid[0] < llmin/lfac )
                {
                  ostringstream os;
                  os << "Too small distance between cloudbox and lower end of\n"
                     << "longitude grid. This distance must here be " 
                     << llmin/lfac << " degrees.";
                  throw runtime_error( os.str() );
                }
              if( lon_grid[n-1] - lon_grid[cloudbox_limits[5]] < llmin/lfac )
                {
                  ostringstream os;
                  os << "Too small distance between cloudbox and upper end of\n"
                     << "longitude grid. This distance must here be " 
                     << llmin/lfac << " degrees.";
                  throw runtime_error( os.str() );
                }
            }
        }
    }
}



/*===========================================================================
  === Functions for Agendas
  ===========================================================================*/

//! chk_not_empty
/*! 
    Checks that an agenda is not empty.

    The function gives an error message if the agenda is empty.

    \param    x_name   The name of the agenda.
    \param    x        A variable of type Agenda.

    \author Patrick Eriksson 
    \date   2002-08-20
*/
void chk_not_empty( 
        const String&      x_name,
        const Agenda&      x ) 
{
  if( x.nelem() == 0 )
    {
      ostringstream os;
      os << "The agenda *" << x_name << "* is empty.\nIt is not allowed \n"
         << "that an agenda that is actually used to be empty.\n"
         << "Empty agendas are only created of methods setting dummy values \n"
         << "to variables.";
      throw runtime_error( os.str() );
    }
}

/*===========================================================================
  === Functions for Tensors
  ===========================================================================*/

//! Runtime check for size of Vector.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstVectorView  x,
               const Index&     c ) 
{
  if ( !is_size(x,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.nelem()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! Runtime check for size of Matrix.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    r        Required number of rows
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstMatrixView  x,
               const Index&     r,
               const Index&     c ) 
{
  if ( !is_size(x,r,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << r 
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.nrows()      
         << " " << x.ncols()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! Runtime check for size of Tensor.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    p        Required number of pages
  \param    r        Required number of rows
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstTensor3View x,
               const Index&     p,
               const Index&     r,
               const Index&     c ) 
{
  if ( !is_size(x,p,r,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << p 
         << " " << r 
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.npages()     
         << " " << x.nrows()      
         << " " << x.ncols()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! Runtime check for size of Tensor.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    b        Required number of books
  \param    p        Required number of pages
  \param    r        Required number of rows
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstTensor4View x,
               const Index&     b,
               const Index&     p,
               const Index&     r,
               const Index&     c ) 
{
  if ( !is_size(x,b,p,r,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << b 
         << " " << p 
         << " " << r 
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.nbooks()     
         << " " << x.npages()     
         << " " << x.nrows()      
         << " " << x.ncols()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! Runtime check for size of Tensor.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    s        Required number of shelves
  \param    b        Required number of books
  \param    p        Required number of pages
  \param    r        Required number of rows
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstTensor5View x,
               const Index&     s,
               const Index&     b,
               const Index&     p,
               const Index&     r,
               const Index&     c ) 
{
  if ( !is_size(x,s,b,p,r,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << s 
         << " " << b 
         << " " << p 
         << " " << r 
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.nshelves()   
         << " " << x.nbooks()     
         << " " << x.npages()     
         << " " << x.nrows()      
         << " " << x.ncols()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! Runtime check for size of Tensor.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    v        Required number of vitrines
  \param    s        Required number of shelves
  \param    b        Required number of books
  \param    p        Required number of pages
  \param    r        Required number of rows
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstTensor6View x,
               const Index&     v,
               const Index&     s,
               const Index&     b,
               const Index&     p,
               const Index&     r,
               const Index&     c ) 
{
  if ( !is_size(x,v,s,b,p,r,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << v 
         << " " << s 
         << " " << b 
         << " " << p 
         << " " << r 
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.nvitrines()  
         << " " << x.nshelves()   
         << " " << x.nbooks()     
         << " " << x.npages()     
         << " " << x.nrows()      
         << " " << x.ncols()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! Runtime check for size of Tensor.
/*! 
  This is the runtime version of is_size. An appropriate error message
  is generated if the size is not correct.

  \param    x_name   The name of the agenda.
  \param    x        A variable of type Agenda.
  \param    l        Required number of libraries
  \param    v        Required number of vitrines
  \param    s        Required number of shelves
  \param    b        Required number of books
  \param    p        Required number of pages
  \param    r        Required number of rows
  \param    c        Required number of columns

  \author Stefan Buehler
  \date   2002-11-29
*/
void chk_size( const String&    x_name,
               ConstTensor7View x,
               const Index&     l,
               const Index&     v,
               const Index&     s,
               const Index&     b,
               const Index&     p,
               const Index&     r,
               const Index&     c ) 
{
  if ( !is_size(x,l,v,s,b,p,r,c) )
    {
      ostringstream os;
      os << "The object *" << x_name
         << "* does not have the right size.\n"
         << "Dimensions should be:"
         << " " << l 
         << " " << v 
         << " " << s 
         << " " << b 
         << " " << p 
         << " " << r 
         << " " << c 
         << ",\nbut they are:         "
         << " " << x.nlibraries() 
         << " " << x.nvitrines()  
         << " " << x.nshelves()   
         << " " << x.nbooks()     
         << " " << x.npages()     
         << " " << x.nrows()      
         << " " << x.ncols()      
         << ".";
      throw runtime_error( os.str() );
    }
}

//! chk_pnd_field_raw_only_in_cloudbox
/*! 
    Checks whether the pnd_field is zero outside the cloudbox.
    This is of a higher level than chk_pnd_data because it does
    not require any filename and because it works on all pnd_field_raw
    rather than just one element. Otherwise, it is mostly a new
    implementation of the same functionality.

    \param    dim                The atmospheric dimensionality.
    \param    pnd_field_raw      All pnd_field_raw data.
    \param    p_grid             Pressure grid.
    \param    lat_grid           Latitude grid.
    \param    lon_grid           Longitude grid.
    \param    cloudbox_limits    The edges of the cloudbox.

    \author Gerrit Holl
    \date   2011-03-24
*/
void chk_pnd_field_raw_only_in_cloudbox(
        const Index&                 dim,
        const ArrayOfGriddedField3&  pnd_field_raw,  
        ConstVectorView              p_grid,
        ConstVectorView              lat_grid,
        ConstVectorView              lon_grid,
        const ArrayOfIndex&          cloudbox_limits )
{
    Numeric p, lat, lon, v;
    Index n, p_i, lat_i, lon_i;
    // For any non-zero point, verify we're outside the cloudbox
    for (n=0; n < pnd_field_raw.nelem(); n++) {
        for (p_i=0; p_i < pnd_field_raw[n].data.npages(); p_i++) {
            for (lat_i=0; lat_i < pnd_field_raw[n].data.nrows(); lat_i++) {
                for (lon_i=0; lon_i < pnd_field_raw[n].data.ncols(); lon_i++) {
                    v = pnd_field_raw[n].data(p_i, lat_i, lon_i);
                    if (v != 0) {
                        // Verify pressure is between cloudbox limits
                        p = pnd_field_raw[n].get_numeric_grid(GFIELD3_P_GRID)[p_i];
                        if (!((p <= p_grid[cloudbox_limits[0]]) &
                              (p >= p_grid[cloudbox_limits[1]]))) {
                            ostringstream os;
                            os << "Found non-zero pnd outside cloudbox. "
                               << "Cloudbox extends from p="
                               << p_grid[cloudbox_limits[0]]
                               << " Pa to p="
                               << p_grid[cloudbox_limits[1]]
                               << " Pa, but found pnd=" << v
                               << "/m³ at p=" << p << " Pa.";
                            throw runtime_error(os.str());
                        }
                        // Verify latitude is too
                        if (dim > 1) {
                            lat = pnd_field_raw[n].get_numeric_grid(GFIELD3_LAT_GRID)[lat_i];
                            if (!((lat >= lat_grid[cloudbox_limits[2]]) &
                                  (lat <= lat_grid[cloudbox_limits[3]]))) {
                                ostringstream os;
                                os << "Found non-zero pnd outside cloudbox. "
                                   << "Cloudbox extends from lat="
                                   << lat_grid[cloudbox_limits[2]]
                                   << "° to lat="
                                   << lat_grid[cloudbox_limits[3]]
                                   << "°, but found pnd=" << v
                                   << "/m³ at lat=" << lat << "°.";
                                throw runtime_error(os.str());
                            }
                        }
                        // Etc. for longitude
                        if (dim > 2) {
                            lon = pnd_field_raw[n].get_numeric_grid(GFIELD3_LON_GRID)[lon_i];
                            if (!((lon >= lon_grid[cloudbox_limits[4]]) &
                                  (lon <= lon_grid[cloudbox_limits[5]]))) {
                                ostringstream os;
                                os << "Found non-zero pnd outside cloudbox. "
                                   << "Cloudbox extends from lon="
                                   << lon_grid[cloudbox_limits[4]]
                                   << "° to lat="
                                   << lon_grid[cloudbox_limits[5]]
                                   << "°, but found pnd=" << v
                                   << "/m³ at lon=" << lon << "°.";
                                throw runtime_error(os.str());
                            }
                        }
                    }
                }
            }
        }
    }
}
