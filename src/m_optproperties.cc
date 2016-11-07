/* Copyright (C) 2002-2012
   Sreerekha T.R. <rekha@uni-bremen.de>
   Claudia Emde <claudia.emde@dlr.de>
   Cory Davies <cory@met.ed.ac.uk>
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA. */

/*!
  \file   m_optproperties.cc
  \author Sreerekha T.R. <rekha@uni-bremen.de>, 
          Claudia Emde <claudia.emde@dlr.de>
          Cory Davies <cory@met.ed.ac.uk>
  \date   Mon Jun 10 11:19:11 2002 
  \brief  This filecontains workspace methods for calculating the optical 
  properties for the radiative transfer. 

  Optical properties are the extinction matrix, absorption vector and
  scattering vector.  The optical properties for the gases can be
  calculated with or without Zeeman effect.
*/

/*===========================================================================
  === External declarations
  ===========================================================================*/

#include <cmath>
#include "arts.h"
#include "exceptions.h"
#include "array.h"
#include "matpackIII.h"
#include "matpackVII.h"
#include "logic.h"
#include "interpolation.h"
#include "messages.h"
#include "xml_io.h"
#include "montecarlo.h"
#include "optproperties.h"
#include "math_funcs.h"
#include "sorting.h"
#include "check_input.h"
#include "auto_md.h" 

extern const Numeric PI;
extern const Numeric DEG2RAD;
extern const Numeric RAD2DEG;

#define PART_TYPE scat_data[i_ss][i_se].ptype
#define F_DATAGRID scat_data[i_ss][i_se].f_grid
#define T_DATAGRID scat_data[i_ss][i_se].T_grid
#define ZA_DATAGRID scat_data[i_ss][i_se].za_grid
#define AA_DATAGRID scat_data[i_ss][i_se].aa_grid
#define PHA_MAT_DATA_RAW scat_data[i_ss][i_se].pha_mat_data  //CPD: changed from pha_mat_data
#define EXT_MAT_DATA_RAW scat_data[i_ss][i_se].ext_mat_data  //which wouldn't let me play with
#define ABS_VEC_DATA_RAW scat_data[i_ss][i_se].abs_vec_data  //scat_data_mono.
#define PND_LIMIT 1e-12 // If particle number density is below this value, 
                        // no transformations will be performed


/* Workspace method: Doxygen documentation will be auto-generated */
void pha_mat_sptFromData( // Output:
                         Tensor5& pha_mat_spt,
                         // Input:
                         const ArrayOfArrayOfSingleScatteringData& scat_data,
                         const Vector& scat_za_grid,
                         const Vector& scat_aa_grid,
                         const Index& scat_za_index, // propagation directions
                         const Index& scat_aa_index,
                         const Index& f_index,
                         const Vector& f_grid,
                         const Numeric& rtp_temperature,
                         const Tensor4& pnd_field, 
                         const Index& scat_p_index,
                         const Index& scat_lat_index,
                         const Index& scat_lon_index,
                         const Verbosity& verbosity
                         )
{
  CREATE_OUT3;
  
  out3 << "Calculate *pha_mat_spt* from database\n";

  const Index stokes_dim = pha_mat_spt.ncols();
  if (stokes_dim > 4 || stokes_dim < 1){
    throw runtime_error("The dimension of the stokes vector \n"
                        "must be 1,2,3 or 4");
  }

  // Determine total number of scattering elements
  const Index N_se_total = TotalNumberOfElements(scat_data);
  if( N_se_total != pnd_field.nbooks() )
    {
      ostringstream os;
      os << "Total number of scattering elements in scat_data "
         << "inconsistent with size of pnd_field.";
      throw runtime_error(os.str());
    }
  // as pha_mat_spt is typically initiallized from pnd_field, this theoretically
  // checks the same as the runtime_error above. Still, we keep it to be on the
  // save side.
  assert( pha_mat_spt.nshelves() == N_se_total );
  
  // Check that we don't have scat_data_mono here. Only checking the first
  // scat element, assuming the other elements have been processed in the same
  // manner. That's save against having mono data, but not against having
  // individual elements produced with only a single frequency. This, however,
  // has been checked by scat_data reading routines (ScatSpecies/Element*Add/Read).
  // Unsafe, however, remain when ReadXML is used directly or if scat_data is
  // (partly) produced from scat_data_singleTmatrix.
  if( scat_data[0][0].f_grid.nelem() < 2 )
  {
      ostringstream os;
      os << "Scattering data seems to be scat_data_mono (1 freq point only),\n"
         << "but frequency interpolable data (scat_data with >=2 freq points) "
         << "is expected here.";
      throw runtime_error( os.str() );
  }
          
  const Index N_ss = scat_data.nelem();

  // Phase matrix in laboratory coordinate system. Dimensions:
  // [frequency, za_inc, aa_inc, stokes_dim, stokes_dim]
  Tensor5 pha_mat_data_int;


  Index i_se_flat = 0;
  // Loop over scattering species
  for (Index i_ss = 0; i_ss < N_ss; i_ss++)
  {
      const Index N_se = scat_data[i_ss].nelem();

      // Loop over the included scattering elements
      for (Index i_se = 0; i_se < N_se; i_se++)
      {
          // If the particle number density at a specific point in the
          // atmosphere for the i_se scattering element is zero, we don't need
          // to do the transfromation!
          if (pnd_field(i_se_flat, scat_p_index, scat_lat_index, scat_lon_index)
              > PND_LIMIT)
          {

              // First we have to transform the data from the coordinate system
              // used in the database (depending on the kind of ptype) to the
              // laboratory coordinate system.

              // Frequency and temperature interpolation:

              // Container for data at one frequency and one temperature.
              pha_mat_data_int.resize(PHA_MAT_DATA_RAW.nshelves(),
                                      PHA_MAT_DATA_RAW.nbooks(),
                                      PHA_MAT_DATA_RAW.npages(),
                                      PHA_MAT_DATA_RAW.nrows(),
                                      PHA_MAT_DATA_RAW.ncols());


              // Gridpositions:
              GridPos freq_gp;
              gridpos(freq_gp, F_DATAGRID, f_grid[f_index]);
              GridPos t_gp;
              Vector itw;

              if( T_DATAGRID.nelem() > 1 )
              {
                  ostringstream os;
                  os << "The temperature grid of the scattering data does not\n"
                     << "cover the atmospheric temperature at cloud location.\n"
                     << "The data should include the value T = "
                     << rtp_temperature << " K.";
                  chk_interpolation_grids( os.str(), T_DATAGRID, 
                                           rtp_temperature );

                  gridpos(t_gp, T_DATAGRID, rtp_temperature);

                  // Interpolation weights:
                  itw.resize(4);
                  interpweights(itw, freq_gp, t_gp);

                  for (Index i_za_sca = 0;
                       i_za_sca < PHA_MAT_DATA_RAW.nshelves(); i_za_sca++)
                    for (Index i_aa_sca = 0;
                         i_aa_sca < PHA_MAT_DATA_RAW.nbooks(); i_aa_sca++)
                      for (Index i_za_inc = 0;
                           i_za_inc < PHA_MAT_DATA_RAW.npages(); i_za_inc++)
                        for (Index i_aa_inc = 0;
                             i_aa_inc < PHA_MAT_DATA_RAW.nrows(); i_aa_inc++)
                          for (Index i = 0; i < PHA_MAT_DATA_RAW.ncols(); i++)
                            // Interpolation of phase matrix:
                            pha_mat_data_int(i_za_sca, i_aa_sca,
                                             i_za_inc, i_aa_inc, i) =
                              interp(itw,
                                     PHA_MAT_DATA_RAW(joker, joker,
                                     i_za_sca, i_aa_sca, i_za_inc, i_aa_inc, i),
                                     freq_gp, t_gp);
              }
              else
              {
                  // Interpolation weights:
                  itw.resize(2);
                  interpweights(itw, freq_gp);
                  for (Index i_za_sca = 0;
                       i_za_sca < PHA_MAT_DATA_RAW.nshelves(); i_za_sca++)
                    for (Index i_aa_sca = 0;
                         i_aa_sca < PHA_MAT_DATA_RAW.nbooks(); i_aa_sca++)
                      for (Index i_za_inc = 0;
                           i_za_inc < PHA_MAT_DATA_RAW.npages(); i_za_inc++)
                        for (Index i_aa_inc = 0;
                             i_aa_inc < PHA_MAT_DATA_RAW.nrows(); i_aa_inc++)
                          for (Index i = 0; i < PHA_MAT_DATA_RAW.ncols(); i++)
                            // Interpolation of phase matrix:
                            pha_mat_data_int(i_za_sca, i_aa_sca,
                                             i_za_inc, i_aa_inc, i) =
                              interp(itw,
                                     PHA_MAT_DATA_RAW(joker, 0,
                                     i_za_sca, i_aa_sca, i_za_inc, i_aa_inc, i),
                                     freq_gp);
              }

              // Do the transformation into the laboratory coordinate system.
              for (Index za_inc_idx = 0; za_inc_idx < scat_za_grid.nelem();
                   za_inc_idx ++)
              {
                  for (Index aa_inc_idx = 0; aa_inc_idx < scat_aa_grid.nelem();
                       aa_inc_idx ++)
                  {
                      pha_matTransform(pha_mat_spt(i_se_flat,
                                                   za_inc_idx, aa_inc_idx,
                                                   joker, joker),
                                       pha_mat_data_int,
                                       ZA_DATAGRID, AA_DATAGRID,
                                       PART_TYPE, scat_za_index, scat_aa_index,
                                       za_inc_idx, 
                                       aa_inc_idx, scat_za_grid, scat_aa_grid,
                                       verbosity);
                  }
              }
          }
          i_se_flat++;
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void pha_mat_sptFromDataDOITOpt(// Output:
                                Tensor5& pha_mat_spt,
                                // Input:
                                const ArrayOfTensor7& pha_mat_sptDOITOpt,
                                const ArrayOfArrayOfSingleScatteringData& scat_data_mono,
                                const Index& doit_za_grid_size,
                                const Vector& scat_aa_grid,
                                const Index& scat_za_index, // propagation directions
                                const Index& scat_aa_index,
                                const Numeric& rtp_temperature,
                                const Tensor4&  pnd_field, 
                                const Index& scat_p_index,
                                const Index&  scat_lat_index,
                                const Index& scat_lon_index,
                                const Verbosity&)
{
  const Index N_se_total = TotalNumberOfElements(scat_data_mono);

  if( N_se_total != pnd_field.nbooks() )
    {
      ostringstream os;
      os << "Total number of scattering elements in scat_data(_mono) "
         << "inconsistent with size of pnd_field.";
      throw runtime_error(os.str());
    }
  // as pha_mat_spt is typically initiallized from pnd_field, this theoretically
  // checks the same as the runtime_error above. Still, we keep it to be on the
  // save side.
  assert( pha_mat_spt.nshelves() == N_se_total );

  // atmosphere_dim = 3
  if (pnd_field.ncols() > 1)
    {
      assert(pha_mat_sptDOITOpt.nelem() == N_se_total);
      // Assuming that if the size is o.k. for one scattering element, it will
      // also be o.k. for the other scattering elements. 
      assert(pha_mat_sptDOITOpt[0].nlibraries() ==
             scat_data_mono[0][0].T_grid.nelem());
      assert(pha_mat_sptDOITOpt[0].nvitrines() == doit_za_grid_size);
      assert(pha_mat_sptDOITOpt[0].nshelves() == scat_aa_grid.nelem() );
      assert(pha_mat_sptDOITOpt[0].nbooks() == doit_za_grid_size);
      assert(pha_mat_sptDOITOpt[0].npages() == scat_aa_grid.nelem()); 
    }
  
  // atmosphere_dim = 1, only zenith angle required for scattered directions. 
  else if ( pnd_field.ncols() == 1 )
    {
      //assert(is_size(scat_theta, doit_za_grid_size, 1,
      //                doit_za_grid_size, scat_aa_grid.nelem()));
      
      assert(pha_mat_sptDOITOpt.nelem() == TotalNumberOfElements(scat_data_mono));
      // Assuming that if the size is o.k. for one scattering element, it will
      // also be o.k. for the other scattering elements. 
      assert(pha_mat_sptDOITOpt[0].nlibraries() == scat_data_mono[0][0].T_grid.nelem());
      assert(pha_mat_sptDOITOpt[0].nvitrines() == doit_za_grid_size);
      assert(pha_mat_sptDOITOpt[0].nshelves() == 1);
      assert(pha_mat_sptDOITOpt[0].nbooks() == doit_za_grid_size);
      assert(pha_mat_sptDOITOpt[0].npages() == scat_aa_grid.nelem()); 
    }
  
  assert(doit_za_grid_size > 0);
  
  // Check that we do indeed have scat_data_mono here. Only checking the first
  // scat element, assuming the other elements have been processed in the same
  // manner. That's save against having scat_data here if that originated from
  // scat_data reading routines (ScatSpecies/Element*Add/Read), it's not safe
  // against data read by ReadXML directly or if scat_data has been (partly)
  // produced from scat_data_singleTmatrix. That would be too costly here,
  // though.
  // Also, we can't check here whether data is at the correct frequency since we
  // don't know f_grid and f_index here (we could pass it in, though).
  if( scat_data_mono[0][0].f_grid.nelem() > 1 )
  {
      ostringstream os;
      os << "Scattering data seems to be scat_data (several freq points),\n"
         << "but scat_data_mono (1 freq point only) is expected here.";
      throw runtime_error( os.str() );
  }
  
  // Create equidistant zenith angle grid
  Vector za_grid;
  nlinspace(za_grid, 0, 180, doit_za_grid_size);  
  
  const Index N_ss = scat_data_mono.nelem();
  const Index stokes_dim = pha_mat_spt.ncols();
  
  if (stokes_dim > 4 || stokes_dim < 1){
    throw runtime_error("The dimension of the stokes vector \n"
                        "must be 1,2,3 or 4");
  }
  
  GridPos T_gp;
  Vector itw(2);
  
  // Initialisation
  pha_mat_spt = 0.;

  Index i_se_flat = 0;

  // Do the transformation into the laboratory coordinate system.
  for (Index i_ss = 0; i_ss < N_ss; i_ss++)
  {
      const Index N_se = scat_data_mono[i_ss].nelem();

      for (Index i_se = 0; i_se < N_se; i_se++)
      {
          // If the particle number density at a specific point in the
          // atmosphere for the i_se scattering element is zero, we don't need
          // to do the transformation!
          if (pnd_field(i_se_flat, scat_p_index, scat_lat_index, scat_lon_index)
              > PND_LIMIT) //TRS
          {
              if( scat_data_mono[i_ss][i_se].T_grid.nelem() > 1)
              {
                  ostringstream os;
                  os << "The temperature grid of the scattering data does not\n"
                     << "cover the atmospheric temperature at cloud location.\n"
                     << "The data should include the value T = "
                     << rtp_temperature << " K.";
                  chk_interpolation_grids( os.str(), 
                                           scat_data_mono[i_ss][i_se].T_grid, 
                                           rtp_temperature);

                  // Gridpositions:
                  gridpos( T_gp, scat_data_mono[i_ss][i_se].T_grid, 
                           rtp_temperature);
                  // Interpolation weights:
                  interpweights(itw, T_gp);
              }



              for (Index za_inc_idx = 0; za_inc_idx < doit_za_grid_size;
                   za_inc_idx ++)
              {
                  for (Index aa_inc_idx = 0; aa_inc_idx < scat_aa_grid.nelem();
                       aa_inc_idx ++)
                  {
                      if( scat_data_mono[i_ss][i_se].T_grid.nelem() == 1)
                      {
                          pha_mat_spt(i_se_flat, za_inc_idx, aa_inc_idx,
                                      joker, joker) =
                          pha_mat_sptDOITOpt[i_se_flat](0, scat_za_index,
                                                        scat_aa_index, za_inc_idx,
                                                        aa_inc_idx, joker, joker);
                      }

                      // Temperature interpolation
                      else
                      {
                          for (Index i = 0; i< stokes_dim; i++)
                          {
                              for (Index j = 0; j< stokes_dim; j++)
                              {
                                  pha_mat_spt(i_se_flat, za_inc_idx, aa_inc_idx,
                                              i, j)=
                                  interp(itw,pha_mat_sptDOITOpt[i_se_flat]
                                         (joker, scat_za_index,
                                          scat_aa_index, za_inc_idx,
                                          aa_inc_idx, i, j) , T_gp);
                              }
                          }
                      }
                  }
              }
          }// TRS

          i_se_flat++;
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void opt_prop_sptFromData(// Output and Input:
                          Tensor3& ext_mat_spt,
                          Matrix& abs_vec_spt,
                          // Input:
                          const ArrayOfArrayOfSingleScatteringData& scat_data,
                          const Vector& scat_za_grid,
                          const Vector& scat_aa_grid,
                          const Index& scat_za_index, // propagation directions
                          const Index& scat_aa_index,
                          const Index& f_index,
                          const Vector& f_grid,
                          const Numeric& rtp_temperature, 
                          const Tensor4& pnd_field, 
                          const Index& scat_p_index,
                          const Index& scat_lat_index,
                          const Index& scat_lon_index,
                          const Verbosity& verbosity)
{
  
  const Index N_ss = scat_data.nelem();
  const Index stokes_dim = ext_mat_spt.ncols();
  const Numeric za_sca = scat_za_grid[scat_za_index];
  const Numeric aa_sca = scat_aa_grid[scat_aa_index];
  
  if (stokes_dim > 4 || stokes_dim < 1){
    throw runtime_error("The dimension of the stokes vector \n"
                        "must be 1,2,3 or 4");
  }

  DEBUG_ONLY(const Index N_se_total = TotalNumberOfElements(scat_data);)
  assert( ext_mat_spt.npages() == N_se_total );
  assert( abs_vec_spt.nrows() == N_se_total );

  // Check that we don't have scat_data_mono here. Only checking the first
  // scat element, assuming the other elements have been processed in the same
  // manner. That's save against having mono data, but not against having
  // individual elements produced with only a single frequency. This, however,
  // has been checked by scat_data reading routines (ScatSpecies/Element*Add/Read).
  // Unsafe, however, remain when ReadXML is used directly or if scat_data is
  // (partly) produced from scat_data_singleTmatrix.
  if( scat_data[0][0].f_grid.nelem() < 2 )
  {
      ostringstream os;
      os << "Scattering data seems to be scat_data_mono (1 freq point only),\n"
         << "but frequency interpolable data (scat_data with >=2 freq points) "
         << "is expected here.";
      throw runtime_error( os.str() );
  }
          
  // Phase matrix in laboratory coordinate system. Dimensions:
  // [frequency, za_inc, aa_inc, stokes_dim, stokes_dim]
  Tensor3 ext_mat_data_int;
  Tensor3 abs_vec_data_int;
  
  // Initialisation
  ext_mat_spt = 0.;
  abs_vec_spt = 0.;


  Index i_se_flat = 0;
  // Loop over the included scattering species
  for (Index i_ss = 0; i_ss < N_ss; i_ss++)
  {
      const Index N_se = scat_data[i_ss].nelem();

      // Loop over the included scattering elements
      for (Index i_se = 0; i_se < N_se; i_se++)
      {
          // If the particle number density at a specific point in the
          // atmosphere for the i_se scattering element is zero, we don't need
          // to do the transformation

          if (pnd_field(i_se_flat, scat_p_index, scat_lat_index, scat_lon_index)
              > PND_LIMIT)
          {
              // First we have to transform the data from the coordinate system
              // used in the database (depending on the kind of ptype) to the
              // laboratory coordinate system.

              // Frequency interpolation:

              // The data is interpolated on one frequency.
              //
              // Resize the variables for the interpolated data:
              //
              ext_mat_data_int.resize(EXT_MAT_DATA_RAW.npages(),
                                      EXT_MAT_DATA_RAW.nrows(),
                                      EXT_MAT_DATA_RAW.ncols());
              //
              abs_vec_data_int.resize(ABS_VEC_DATA_RAW.npages(),
                                      ABS_VEC_DATA_RAW.nrows(),
                                      ABS_VEC_DATA_RAW.ncols());


              // Gridpositions:
              GridPos freq_gp;
              gridpos(freq_gp, F_DATAGRID, f_grid[f_index]);
              GridPos t_gp;
              Vector itw;

              if ( T_DATAGRID.nelem() > 1)
              {
                  ostringstream os;
                  os << "The temperature grid of the scattering data does not\n"
                     << "cover the atmospheric temperature at cloud location.\n"
                     << "The data should include the value T = "
                     << rtp_temperature << " K.";
                  chk_interpolation_grids( os.str(), T_DATAGRID, 
                                           rtp_temperature );

                  gridpos(t_gp, T_DATAGRID, rtp_temperature);

                  // Interpolation weights:
                  itw.resize(4);
                  interpweights(itw, freq_gp, t_gp);

                  for (Index i_za_sca = 0; i_za_sca < EXT_MAT_DATA_RAW.npages();
                       i_za_sca++)
                  {
                      for(Index i_aa_sca = 0; i_aa_sca < EXT_MAT_DATA_RAW.nrows();
                          i_aa_sca++)
                      {
                          //
                          // Interpolation of extinction matrix:
                          //
                          for (Index i = 0; i < EXT_MAT_DATA_RAW.ncols(); i++)
                          {
                              ext_mat_data_int(i_za_sca, i_aa_sca, i) =
                              interp(itw, EXT_MAT_DATA_RAW(joker, joker,
                                                           i_za_sca, i_aa_sca, i),
                                     freq_gp, t_gp);
                          }
                      }
                  }

                  for (Index i_za_sca = 0; i_za_sca < ABS_VEC_DATA_RAW.npages();
                       i_za_sca++)
                  {
                      for(Index i_aa_sca = 0; i_aa_sca < ABS_VEC_DATA_RAW.nrows();
                          i_aa_sca++)
                      {
                          //
                          // Interpolation of absorption vector:
                          //
                          for (Index i = 0; i < ABS_VEC_DATA_RAW.ncols(); i++)
                          {
                              abs_vec_data_int(i_za_sca, i_aa_sca, i) =
                              interp(itw, ABS_VEC_DATA_RAW(joker, joker, i_za_sca,
                                                           i_aa_sca, i),
                                     freq_gp, t_gp);
                          }
                      }
                  }
              }
              else
              {
                  // Interpolation weights:
                  itw.resize(2);
                  interpweights(itw, freq_gp);

                  for (Index i_za_sca = 0; i_za_sca < EXT_MAT_DATA_RAW.npages();
                       i_za_sca++)
                  {
                      for(Index i_aa_sca = 0; i_aa_sca < EXT_MAT_DATA_RAW.nrows();
                          i_aa_sca++)
                      {
                          //
                          // Interpolation of extinction matrix:
                          //
                          for (Index i = 0; i < EXT_MAT_DATA_RAW.ncols(); i++)
                          {
                              ext_mat_data_int(i_za_sca, i_aa_sca, i) =
                              interp(itw, EXT_MAT_DATA_RAW(joker, 0,
                                                           i_za_sca, i_aa_sca, i),
                                     freq_gp);
                          }
                      }
                  }

                  for (Index i_za_sca = 0; i_za_sca < ABS_VEC_DATA_RAW.npages();
                       i_za_sca++)
                  {
                      for(Index i_aa_sca = 0; i_aa_sca < ABS_VEC_DATA_RAW.nrows();
                          i_aa_sca++)
                      {
                          //
                          // Interpolation of absorption vector:
                          //
                          for (Index i = 0; i < ABS_VEC_DATA_RAW.ncols(); i++)
                          {
                              abs_vec_data_int(i_za_sca, i_aa_sca, i) =
                              interp(itw, ABS_VEC_DATA_RAW(joker, 0, i_za_sca,
                                                           i_aa_sca, i),
                                     freq_gp);
                          }
                      }
                  }
              }


              //
              // Do the transformation into the laboratory coordinate system.
              //
              // Extinction matrix:
              //


              ext_matTransform(ext_mat_spt(i_se_flat, joker, joker),
                               ext_mat_data_int,
                               ZA_DATAGRID, AA_DATAGRID, PART_TYPE,
                               za_sca, aa_sca,
                               verbosity);
              //
              // Absorption vector:
              //
              abs_vecTransform(abs_vec_spt(i_se_flat, joker),
                               abs_vec_data_int,
                               ZA_DATAGRID, AA_DATAGRID, PART_TYPE,
                               za_sca, aa_sca, verbosity);
          }

          i_se_flat++;
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void ext_matAddPart(Tensor3& ext_mat,
                    const Tensor3& ext_mat_spt,
                    const Tensor4& pnd_field,
                    const Index& atmosphere_dim,
                    const Index& scat_p_index,
                    const Index& scat_lat_index,
                    const Index& scat_lon_index,
                    const Verbosity&)
                     
{
  Index N_se = ext_mat_spt.npages();
  Index stokes_dim = ext_mat_spt.nrows();
  
  Matrix ext_mat_part(stokes_dim, stokes_dim, 0.0);

  
  if (stokes_dim > 4 || stokes_dim < 1){
    throw runtime_error(
                        "The dimension of stokes vector can be "
                        "only 1,2,3, or 4");
  }
  if ( ext_mat_spt.ncols() != stokes_dim){
    
    throw runtime_error(" The columns of ext_mat_spt should "
                        "agree to stokes_dim");
  }

  if (atmosphere_dim == 1)
    {
      // this is a loop over the different scattering elements
      for (Index l = 0; l < N_se; l++)
        { 
          
          // now the last two loops over the stokes dimension.
          for (Index m = 0; m < stokes_dim; m++)
            {
              for (Index n = 0; n < stokes_dim; n++)
                //summation of the product of pnd_field and 
                //ext_mat_spt.
                ext_mat_part(m, n) += 
                  (ext_mat_spt(l, m, n) * pnd_field(l, scat_p_index, 0, 0));
            }
        }

      //Add extinction matrix due single scattering element to *ext_mat*.
      ext_mat(0, Range(joker), Range(joker)) += ext_mat_part;
    }
 
  if (atmosphere_dim == 3)
    {
      
      // this is a loop over the different scattering elements
      for (Index l = 0; l < N_se; l++)
        { 
          
          // now the last two loops over the stokes dimension.
          for (Index m = 0; m < stokes_dim; m++)
            {
              for (Index n = 0; n < stokes_dim; n++)
                //summation of the product of pnd_field and 
                //ext_mat_spt.
                ext_mat_part(m, n) +=  (ext_mat_spt(l, m, n) * 
                                        pnd_field(l, scat_p_index, 
                                                  scat_lat_index, 
                                                  scat_lon_index));
              
            } 
        }

      //Add extinction matrix due single scattering element to *ext_mat*.
      ext_mat(0, Range(joker), Range(joker)) += ext_mat_part;

    }

} 


/* Workspace method: Doxygen documentation will be auto-generated */
void abs_vecAddPart(Matrix& abs_vec,
                    const Matrix& abs_vec_spt,
                    const Tensor4& pnd_field,
                    const Index& atmosphere_dim,
                    const Index& scat_p_index,
                    const Index& scat_lat_index,
                    const Index& scat_lon_index,
                    const Verbosity&)
                    
{
  Index N_se = abs_vec_spt.nrows();
  Index stokes_dim = abs_vec_spt.ncols();

  Vector abs_vec_part(stokes_dim, 0.0);

  if ((stokes_dim > 4) || (stokes_dim <1)){
    throw runtime_error("The dimension of stokes vector "
                        "can be only 1,2,3, or 4");
  } 
 
  if (atmosphere_dim == 1)
    {
      // this is a loop over the different scattering elements
      for (Index l = 0; l < N_se ; ++l)
        {
          // now the loop over the stokes dimension.
          //(CE:) in the middle was l instead of m
          for (Index m = 0; m < stokes_dim; ++m)
             //summation of the product of pnd_field and 
            //abs_vec_spt.
            abs_vec_part[m] += 
              (abs_vec_spt(l, m) * pnd_field(l, scat_p_index, 0, 0));
          
        }
      //Add absorption due single scattering element.
      abs_vec(0, Range(joker)) += abs_vec_part;
    }
  
  if (atmosphere_dim == 3)
    {
      // this is a loop over the different scattering elements
      for (Index l = 0; l < N_se ; ++l)
        {
          
          // now the loop over the stokes dimension.
          for (Index m = 0; m < stokes_dim; ++m)
             //summation of the product of pnd_field and 
            //abs_vec_spt.
            abs_vec_part[m] += (abs_vec_spt(l, m) *
                                pnd_field(l, scat_p_index,
                                          scat_lat_index, 
                                          scat_lon_index));
          
        }
      //Add absorption due single scattering element.
      abs_vec(0,Range(joker)) += abs_vec_part;
    }
} 


/* Workspace method: Doxygen documentation will be auto-generated */
void ext_matInit(Tensor3&         ext_mat,
                 const Vector&    f_grid,
                 const Index&     stokes_dim,
                 const Index&     f_index,
                 const Verbosity& verbosity)
{
  CREATE_OUT2;
  
  Index freq_dim;

  if( f_index < 0 )
    freq_dim = f_grid.nelem();
  else
    freq_dim = 1;
 
  ext_mat.resize( freq_dim,
                  stokes_dim,
                  stokes_dim );
  ext_mat = 0;                  // Initialize to zero!

  out2 << "Set dimensions of ext_mat as ["
       << freq_dim << ","
       << stokes_dim << ","
       << stokes_dim << "] and initialized to 0.\n";
}


/* Workspace method: Doxygen documentation will be auto-generated */
void ext_matAddGas(Tensor3&      ext_mat,
                   const Tensor4& propmat_clearsky,
                   const Verbosity&)
{
  // Number of Stokes parameters:
  const Index stokes_dim = ext_mat.ncols();

  // The second dimension of ext_mat must also match the number of
  // Stokes parameters:
  if ( stokes_dim != ext_mat.nrows() )
    throw runtime_error("Row dimension of ext_mat inconsistent with "
                        "column dimension.");
  if ( stokes_dim != propmat_clearsky.ncols() )
    throw runtime_error("Col dimension of propmat_clearsky "
                        "inconsistent with col dimension in ext_mat.");

  // Number of frequencies:
  const Index f_dim = ext_mat.npages();

  // This must be consistent with the second dimension of
  // propmat_clearsky. Check this:
  if ( f_dim != propmat_clearsky.npages() )
    throw runtime_error("Frequency dimension of ext_mat and propmat_clearsky\n"
                        "are inconsistent in ext_matAddGas.");

  // Sum up absorption over all species.
  // This gives us an absorption vector for all frequencies. Of course
  // this includes the special case that there is only one frequency.
  Tensor3 abs_total(f_dim,stokes_dim,stokes_dim);
  abs_total = 0;
  
//   for ( Index i=0; i<f_dim; ++i )
//     abs_total[i] = abs_scalar_gas(i,joker).sum();
  for ( Index iv=0; iv<f_dim; ++iv )
        for ( Index is1=0; is1<stokes_dim; ++is1 )
              for ( Index is2=0; is2<stokes_dim; ++is2 )
                    abs_total(iv,is1,is2) += propmat_clearsky(joker,iv,is1,is2).sum();
  
    // Add the absorption value to all the elements:
      ext_mat += abs_total;
      
}


/* Workspace method: Doxygen documentation will be auto-generated */
void abs_vecInit(Matrix&       abs_vec,
                 const Vector& f_grid,
                 const Index&  stokes_dim,
                 const Index&  f_index,
                 const Verbosity& verbosity)
{
  CREATE_OUT2;
  
  Index freq_dim;

  if( f_index < 0 )
    freq_dim = f_grid.nelem();
  else
    freq_dim = 1;
 
  abs_vec.resize( freq_dim,
                  stokes_dim );
  abs_vec = 0;                  // Initialize to zero!

  out2 << "Set dimensions of abs_vec as ["
       << freq_dim << ","
       << stokes_dim << "] and initialized to 0.\n";
}


/* Workspace method: Doxygen documentation will be auto-generated */
void abs_vecAddGas(Matrix&       abs_vec,
                   const Tensor4& propmat_clearsky,
                   const Verbosity&)
{
  // Number of frequencies:
  const Index f_dim = abs_vec.nrows();
  const Index stokes_dim = abs_vec.ncols();
  
  // This must be consistent with the second dimension of
  // propmat_clearsky. Check this:
  if ( f_dim != propmat_clearsky.npages() )
    throw runtime_error("Frequency dimension of abs_vec and propmat_clearsky\n"
                        "are inconsistent in abs_vecAddGas.");
  if ( stokes_dim != propmat_clearsky.ncols() )
    throw runtime_error("Stokes dimension of abs_vec and propmat_clearsky\n"
                        "are inconsistent in abs_vecAddGas.");
    
  // Loop all frequencies. Of course this includes the special case
  // that there is only one frequency.
  for ( Index i=0; i<f_dim; ++i )
    {
      // Sum up the columns of propmat_clearsky and add to the first
      // element of abs_vec.
      for(Index is = 0; is < stokes_dim;is++)
        abs_vec(i,is) += propmat_clearsky(joker,i,is,0).sum();
    }

  // We don't have to do anything about higher elements of abs_vec,
  // since scalar gas absorption only influences the first element.
}


/* Workspace method: Doxygen documentation will be auto-generated */
/*
void ext_matAddGasZeeman( Tensor3&      ext_mat,
                          const Tensor3&  ext_mat_zee,
                          const Verbosity&)
{
  // Number of Stokes parameters:
  const Index stokes_dim = ext_mat.ncols();

  // The second dimension of ext_mat must also match the number of
  // Stokes parameters:
  if ( stokes_dim != ext_mat.nrows() )
    throw runtime_error("Row dimension of ext_mat inconsistent with "
                        "column dimension."); 

  for ( Index i=0; i<stokes_dim; ++i )
    {
      for ( Index j=0; j<stokes_dim; ++j )
        {
          // Add the zeeman extinction to extinction matrix.
          ext_mat(joker,i,j) += ext_mat_zee(joker, i, j);
        }
      
    }
}
*/


/* Workspace method: Doxygen documentation will be auto-generated */
/*
void abs_vecAddGasZeeman( Matrix&      abs_vec,
                          const Matrix& abs_vec_zee,
                          const Verbosity&)
{
  // Number of Stokes parameters:
  const Index stokes_dim = abs_vec_zee.ncols();
  // that there is only one frequency.
  for ( Index j=0; j<stokes_dim; ++j )
    {
      abs_vec(joker,j) += abs_vec_zee(joker,j);
    }
}
*/


/* Workspace method: Doxygen documentation will be auto-generated */
void pha_matCalc(Tensor4& pha_mat,
                 const Tensor5& pha_mat_spt,
                 const Tensor4& pnd_field,
                 const Index& atmosphere_dim,
                 const Index& scat_p_index,
                 const Index& scat_lat_index,
                 const Index& scat_lon_index,
                 const Verbosity&)
{

  Index N_se = pha_mat_spt.nshelves();
  Index Nza = pha_mat_spt.nbooks();
  Index Naa = pha_mat_spt.npages();
  Index stokes_dim = pha_mat_spt.nrows();
 
  pha_mat.resize(Nza, Naa, stokes_dim, stokes_dim);

  // Initialisation
  pha_mat = 0.0;
          
  if (atmosphere_dim == 1)
    {
      // this is a loop over the different scattering elements
      for (Index pt_index = 0; pt_index < N_se; ++ pt_index)
        {
          // these are loops over zenith angle and azimuth angle
          for (Index za_index = 0; za_index < Nza; ++ za_index)
            {
              for (Index aa_index = 0; aa_index < Naa; ++ aa_index)
                {
                  
                  // now the last two loops over the stokes dimension.
                  for (Index stokes_index_1 = 0; stokes_index_1 < stokes_dim; 
                       ++  stokes_index_1)
                    {
                      for (Index stokes_index_2 = 0; stokes_index_2 < stokes_dim;
                           ++ stokes_index_2)
                        //summation of the product of pnd_field and 
                        //pha_mat_spt.
                        pha_mat(za_index, aa_index,  
                                     stokes_index_1, stokes_index_2) += 
                          (pha_mat_spt(pt_index, za_index, aa_index,  
                                       stokes_index_1, stokes_index_2) * 
                          pnd_field(pt_index,scat_p_index, 0, 0));
                    }
                }
            }
        }
    }
          
  if (atmosphere_dim == 3)
    {
      // this is a loop over the different scattering elements
      for (Index pt_index = 0; pt_index < N_se; ++ pt_index)
        {
          
          // these are loops over zenith angle and azimuth angle
          for (Index za_index = 0; za_index < Nza; ++ za_index)
            {
              for (Index aa_index = 0; aa_index < Naa; ++ aa_index)
                {
                  
                  // now the last two loops over the stokes dimension.
                  for (Index i = 0;  i < stokes_dim; ++  i)
                    {
                      for (Index j = 0; j < stokes_dim; ++ j)
                        {
                          //summation of the product of pnd_field and 
                          //pha_mat_spt.
                          pha_mat(za_index, aa_index, i,j ) += 
                            (pha_mat_spt(pt_index, za_index, aa_index, i, j) * 
                             pnd_field(pt_index, scat_p_index,  
                                       scat_lat_index, scat_lon_index));
                          
                          
                        } 
                    }	
                }
            }		
        }	
    }		
}



/* Workspace method: Doxygen documentation will be auto-generated */
void scat_dataCheck( //Input:
                     const ArrayOfArrayOfSingleScatteringData& scat_data,
                     const String& check_type,
                     const Numeric& threshold,
                     const Verbosity& verbosity )
{
    CREATE_OUT0;
    CREATE_OUT2;
    CREATE_OUT3;

    const Index N_ss = scat_data.nelem();

    // 1) any negative values in Z11, K11, or a1? K11>=a1?
    // 2) scat_data containing any NaN?
    // 3) sca_mat norm sufficiently good (int(Z11)~=K11-a1?)

    // Loop over the included scattering species
    out2 << " checking for negative values in Z11, K11, and a1, and for K11<a1\n";
    for (Index i_ss = 0; i_ss < N_ss; i_ss++)
    {
      const Index N_se = scat_data[i_ss].nelem();

      // Loop over the included scattering elements
      for (Index i_se = 0; i_se < N_se; i_se++)
      {
        for (Index f = 0; f < F_DATAGRID.nelem(); f++)
        {
          for (Index t = 0; t < T_DATAGRID.nelem(); t++)
          {
            for (Index zai=0; zai<ABS_VEC_DATA_RAW.npages(); zai++)
              for (Index aai=0; aai<ABS_VEC_DATA_RAW.nrows(); aai++)
              {
                if( EXT_MAT_DATA_RAW(f,t,zai,aai,0)<0 ||
                    ABS_VEC_DATA_RAW(f,t,zai,aai,0)<0 )
                  {
                    ostringstream os;
                    os << "Scatt. species #" << i_ss << " element #" << i_se
                       << " contains negative K11 or a1 at f#"
                       << f << ", T#" << t << ", za#" << zai << ", aa#" << aai
                       << "\n";
                    throw runtime_error( os.str() );
                  }
                if( EXT_MAT_DATA_RAW(f,t,zai,aai,0) <
                    ABS_VEC_DATA_RAW(f,t,zai,aai,0) )
                  {
                    ostringstream os;
                    os << "Scatt. species #" << i_ss << " element #" << i_se
                       << " has K11<a1 at f#"
                       << f << ", T#" << t << ", za#" << zai << ", aa#" << aai
                       << "\n";
                    throw runtime_error( os.str() );
                  }
                for (Index zas=0; zas<PHA_MAT_DATA_RAW.nshelves(); zas++)
                  for (Index aas=0; aas<PHA_MAT_DATA_RAW.nbooks(); aas++)
                    if( PHA_MAT_DATA_RAW(f,t,zas,aas,zai,aai,0)<0 )
                    {
                      ostringstream os;
                      os << "Scatt. species #" << i_ss << " element #" << i_se
                         << " contains negative Z11 at f#" << f
                         << ", T#" << t << ", za_sca#" << zas << ", aa_sca#"
                         << aas << ", za_inc#" << zai << ", aa_inc#" << aai
                         << "\n";
                      throw runtime_error( os.str() );
                    }
              }
          }
        }
      }
    }

    // Loop over the included scattering species
    out2 << " checking for NaN anywhere in Z, K, and a\n";
    for (Index i_ss = 0; i_ss < N_ss; i_ss++)
    {
      const Index N_se = scat_data[i_ss].nelem();

      // Loop over the included scattering elements
      for (Index i_se = 0; i_se < N_se; i_se++)
      {
        for (Index f = 0; f < F_DATAGRID.nelem(); f++)
        {
          for (Index t = 0; t < T_DATAGRID.nelem(); t++)
          {
            for (Index zai=0; zai<ABS_VEC_DATA_RAW.npages(); zai++)
              for (Index aai=0; aai<ABS_VEC_DATA_RAW.nrows(); aai++)
              {
                for (Index st=0; st<ABS_VEC_DATA_RAW.ncols(); st++)
                  if( isnan(ABS_VEC_DATA_RAW(f,t,zai,aai,st)) )
                  {
                    ostringstream os;
                    os << "Scatt. species #" << i_ss << " element #" << i_se
                       << " contains NaN in abs_vec at f#" << f << ", T#"
                       << t << ", za#" << zai << ", aa#" << aai << ", stokes #"
                       << st << "\n";
                    throw runtime_error( os.str() );
                  }
                for (Index st=0; st<EXT_MAT_DATA_RAW.ncols(); st++)
                  if( isnan(EXT_MAT_DATA_RAW(f,t,zai,aai,st)) )
                  {
                    ostringstream os;
                    os << "Scatt. species #" << i_ss << " element #" << i_se
                       << " contains NaN in ext_mat at f#" << f << ", T#"
                       << t << ", za#" << zai << ", aa#" << aai << ", stokes #"
                       << st << "\n";
                    throw runtime_error( os.str() );
                  }
                for (Index zas=0; zas<PHA_MAT_DATA_RAW.nshelves(); zas++)
                  for (Index aas=0; aas<PHA_MAT_DATA_RAW.nbooks(); aas++)
                    for (Index st=0; st<PHA_MAT_DATA_RAW.ncols(); st++)
                      if( isnan(PHA_MAT_DATA_RAW(f,t,zas,aas,zai,aai,st)) )
                      {
                        ostringstream os;
                        os << "Scatt. species #" << i_ss << " element #" << i_se
                           << " contains NaN in pha_mat at f#" << f << ", T#"
                           << t << ", za_sca#" << zas << ", aa_sca#" << aas
                           << ", za_inc#" << zai << ", aa_inc#" << aai
                           << ", stokes #" << "\n";
                        throw runtime_error( os.str() );
                      }
              }
          }
        }
      }
    }


    if( check_type=="all" || check_type=="All" ||
        check_type=="ALL" )
    {
      // Loop over the included scattering species
      out2 << " checking normalization of scattering matrix\n";
      for (Index i_ss = 0; i_ss < N_ss; i_ss++)
      {
        const Index N_se = scat_data[i_ss].nelem();

        // Loop over the included scattering elements
        for (Index i_se = 0; i_se < N_se; i_se++)
        {
          switch (PART_TYPE)
          {
            case PTYPE_MACROS_ISO:
            {
              for (Index f = 0; f < F_DATAGRID.nelem(); f++)
              {
                for (Index t = 0; t < T_DATAGRID.nelem(); t++)
                {
                  Numeric Csca = AngIntegrate_trapezoid(
                                  PHA_MAT_DATA_RAW(f, t, joker, 0, 0, 0, 0),
                                  ZA_DATAGRID);
                  Numeric Cext_data = EXT_MAT_DATA_RAW(f,t,0,0,0);
                  Numeric Cabs = Cext_data - Csca;
                  Numeric Cabs_data = ABS_VEC_DATA_RAW(f,t,0,0,0);
                  Numeric Csca_data = Cext_data - Cabs_data;

                  out3 << "  Coefficients in database: "
                       << "Cext: " << Cext_data << " Cabs: " << Cabs_data
                       << " Csca: " << Csca_data << "\n"
                       << "  Calculated coefficients: "
                       << "Cabs calc: " << Cabs
                       << " Csca calc: " << Csca << "\n"
                       << "  Deviations "
                       << "Cabs: " << 1e2*Cabs/Cabs_data-1e2
                       << "% Csca: " << 1e2*Csca/Csca_data-1e2
                       << "% Alb: " << (Csca-Csca_data)/Cext_data << "\n";

                  //if (abs(Csca/Csca_data-1.)*Csca_data/Cext_data > threshold)
                  // below equivalent to the above
                  // (it's actually the (absolute) albedo deviation!)
                  if (abs(Csca-Csca_data)/Cext_data > threshold)
                  {
                    ostringstream os;
                    os << "  Deviations in scat_data too large:\n"
                       << "  scat dev [%] " << 1e2*Csca/Csca_data-1e2
                       << " at albedo of " << Csca_data/Cext_data << "\n"
                       << "  Check entry for scattering element " << i_se
                       << " of scattering species " << i_ss << " at "
                       << f << ".frequency and " << t << ".temperature!\n";
                    throw runtime_error( os.str() );
                  }
                }
              }
              break;
            }
                    
            case PTYPE_HORIZ_AL:
            {
              for (Index f = 0; f < F_DATAGRID.nelem(); f++)
              {
                for (Index t = 0; t < T_DATAGRID.nelem(); t++)
                {
                  for (Index iza = 0; iza < ABS_VEC_DATA_RAW.npages(); iza++)
                  {
                    Numeric Csca = 2 * AngIntegrate_trapezoid(
                                    PHA_MAT_DATA_RAW(f, t, joker, joker, iza, 0, 0),
                                    ZA_DATAGRID, AA_DATAGRID );
                    Numeric Cext_data = EXT_MAT_DATA_RAW(f,t,iza,0,0);
                    Numeric Cabs = Cext_data - Csca;
                    Numeric Cabs_data = ABS_VEC_DATA_RAW(f,t,iza,0,0);
                    Numeric Csca_data = Cext_data - Cabs_data;

                    out3 << "  Coefficients in database: "
                         << "Cext: " << Cext_data << " Cabs: " << Cabs_data
                         << " Csca: " << Csca_data << "\n"
                         << "  Calculated coefficients: "
                         << "Cabs calc: " << Cabs
                         << " Csca calc: " << Csca << "\n"
                         << "  Deviations "
                         << "Cabs: " << 1e2*Cabs/Cabs_data-1e2
                         << "% Csca: " << 1e2*Csca/Csca_data-1e2
                         << "% Alb: " << (Csca-Csca_data)/Cext_data << "\n";

                    //if (abs(Csca/Csca_data-1.)*Csca_data/Cext_data > threshold)
                    // below equivalent to the above
                    // (it's actually the (absolute) albedo deviation!)
                    if (abs(Csca-Csca_data)/Cext_data > threshold)
                    {
                      ostringstream os;
                      os << "  Deviations in scat_data too large:\n"
                         << "  scat dev [%] " << 1e2*Csca/Csca_data-1e2
                         << " at albedo of " << Csca_data/Cext_data << "\n"
                         << "  Check entry for scattering element " << i_se
                         << " of scattering species " << i_ss << " at "
                         << f << ". frequency, " << t << ". temperature, and "
                         << iza << ". incident polar angle!\n";
                      throw runtime_error( os.str() );
                    }
                  }
                }
              }
              break;
            }

            default:
            {
              out0 << "  WARNING:\n"
                   << "  scat_data consistency check not implemented (yet?!) for\n"
                   << "  ptype " << PART_TYPE << "!\n";
            }
          }
        }
      }
    }
    else
    {
      out0 << "  WARNING:\n"
           << "  Normalization check on pha_mat switched off.\n"
           << "  Scattering solution might be wrong.\n";
    }
}



/* Workspace method: Doxygen documentation will be auto-generated */
void DoitScatteringDataPrepare(//Output:
                               ArrayOfTensor7& pha_mat_sptDOITOpt,
                               ArrayOfArrayOfSingleScatteringData& scat_data_mono,
                               //Input:
                               const Index& doit_za_grid_size,
                               const Vector& scat_aa_grid,
                               const ArrayOfArrayOfSingleScatteringData& scat_data,
                               const Vector& f_grid,
                               const Index& f_index,
                               const Index& atmosphere_dim,
                               const Index& stokes_dim,
                               const Verbosity& verbosity)
{
  
  // Interpolate all the data in frequency
  scat_data_monoCalc(scat_data_mono, scat_data, f_grid, f_index, verbosity);
  
  // For 1D calculation the scat_aa dimension is not required:
  Index N_aa_sca;
  if(atmosphere_dim == 1)
    N_aa_sca = 1;
  else
    N_aa_sca = scat_aa_grid.nelem();
  
  Vector za_grid;
  nlinspace(za_grid, 0, 180, doit_za_grid_size);

  assert( scat_data.nelem() == scat_data_mono.nelem() );
  
  const Index N_ss = scat_data.nelem();
  
  pha_mat_sptDOITOpt.resize(TotalNumberOfElements(scat_data));

  Index i_se_flat = 0;
  for (Index i_ss = 0; i_ss < N_ss; i_ss++)
  {
      const Index N_se = scat_data[i_ss].nelem();

      for (Index i_se = 0; i_se < N_se; i_se++)
      {
          Index N_T = scat_data_mono[i_ss][i_se].T_grid.nelem();
          pha_mat_sptDOITOpt[i_se_flat].resize(N_T, doit_za_grid_size, N_aa_sca,
                                          doit_za_grid_size, scat_aa_grid.nelem(),
                                          stokes_dim, stokes_dim);

          //    Initialize:
          pha_mat_sptDOITOpt[i_se_flat]= 0.;

          // Calculate all scattering angles for all combinations of incoming
          // and scattered directions and interpolation.
          for (Index t_idx = 0; t_idx < N_T; t_idx ++)
          {
              // These are the scattered directions as called in *scat_field_calc*
              for (Index za_sca_idx = 0; za_sca_idx < doit_za_grid_size; za_sca_idx ++)
              {
                  for (Index aa_sca_idx = 0; aa_sca_idx < N_aa_sca; aa_sca_idx ++)
                  {
                      // Integration is performed over all incoming directions
                      for (Index za_inc_idx = 0; za_inc_idx < doit_za_grid_size;
                           za_inc_idx ++)
                      {
                          for (Index aa_inc_idx = 0; aa_inc_idx <
                               scat_aa_grid.nelem();
                               aa_inc_idx ++)
                          {
                              pha_matTransform(pha_mat_sptDOITOpt[i_se_flat]
                                               (t_idx, za_sca_idx,
                                                aa_sca_idx, za_inc_idx, aa_inc_idx,
                                                joker, joker),
                                               scat_data_mono[i_ss][i_se].
                                               pha_mat_data
                                               (0,t_idx,joker,joker,joker,
                                                joker,joker),
                                               scat_data_mono[i_ss][i_se].za_grid,
                                               scat_data_mono[i_ss][i_se].aa_grid,
                                               scat_data_mono[i_ss][i_se].ptype,
                                               za_sca_idx,
                                               aa_sca_idx,
                                               za_inc_idx,
                                               aa_inc_idx,
                                               za_grid,
                                               scat_aa_grid,
                                               verbosity);
                          }
                      }
                  }
              }
          }
          
          i_se_flat++;
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void scat_data_monoCalc(ArrayOfArrayOfSingleScatteringData& scat_data_mono,
                        const ArrayOfArrayOfSingleScatteringData& scat_data,
                        const Vector& f_grid,
                        const Index& f_index,
                        const Verbosity&)
{
  //Extrapolation factor:
  //const Numeric extpolfac = 0.5;
  
  // Check, whether single scattering data contains the right frequencies:
  // The check was changed to allow extrapolation at the boundaries of the 
  // frequency grid.
  for (Index h = 0; h<scat_data.nelem(); h++)
  {
    for (Index i = 0; i<scat_data[h].nelem(); i++)
    {
      // check with extrapolation
      chk_interpolation_grids("scat_data.f_grid to f_grid",
                              scat_data[h][i].f_grid,
                              f_grid[f_index]);
    }
  }

  //Initialise scat_data_mono
  scat_data_mono.resize(scat_data.nelem());

  // Loop over the included scattering species
  for (Index i_ss = 0; i_ss<scat_data.nelem(); i_ss++)
  {
      const Index N_se = scat_data[i_ss].nelem();

      //Initialise scat_data_mono
      scat_data_mono[i_ss].resize(N_se);

      // Loop over the included scattering elements
      for (Index i_se = 0; i_se < N_se; i_se++)
      {
          // Gridpositions:
          GridPos freq_gp;
          gridpos(freq_gp, F_DATAGRID, f_grid[f_index]);

          // Interpolation weights:
          Vector itw(2);
          interpweights(itw, freq_gp);

          //Stuff that doesn't need interpolating
          scat_data_mono[i_ss][i_se].ptype=PART_TYPE;
          scat_data_mono[i_ss][i_se].f_grid.resize(1);
          scat_data_mono[i_ss][i_se].f_grid=f_grid[f_index];
          scat_data_mono[i_ss][i_se].T_grid=scat_data[i_ss][i_se].T_grid;
          scat_data_mono[i_ss][i_se].za_grid=ZA_DATAGRID;
          scat_data_mono[i_ss][i_se].aa_grid=AA_DATAGRID;

          //Phase matrix data
          scat_data_mono[i_ss][i_se].pha_mat_data.resize(1,
                                                   PHA_MAT_DATA_RAW.nvitrines(),
                                                   PHA_MAT_DATA_RAW.nshelves(),
                                                   PHA_MAT_DATA_RAW.nbooks(),
                                                   PHA_MAT_DATA_RAW.npages(),
                                                   PHA_MAT_DATA_RAW.nrows(),
                                                   PHA_MAT_DATA_RAW.ncols());

          for (Index t_index = 0; t_index < PHA_MAT_DATA_RAW.nvitrines(); t_index ++)
          {
              for (Index i_za_sca = 0; i_za_sca < PHA_MAT_DATA_RAW.nshelves();
                   i_za_sca++)
              {
                  for (Index i_aa_sca = 0; i_aa_sca < PHA_MAT_DATA_RAW.nbooks();
                       i_aa_sca++)
                  {
                      for (Index i_za_inc = 0; i_za_inc <
                           PHA_MAT_DATA_RAW.npages();
                           i_za_inc++)
                      {
                          for (Index i_aa_inc = 0;
                               i_aa_inc < PHA_MAT_DATA_RAW.nrows();
                               i_aa_inc++)
                          {
                              for (Index i = 0; i < PHA_MAT_DATA_RAW.ncols(); i++)
                              {
                                  scat_data_mono[i_ss][i_se].pha_mat_data(0, t_index,
                                                                    i_za_sca,
                                                                    i_aa_sca,
                                                                    i_za_inc,
                                                                    i_aa_inc, i) =
                                  interp(itw,
                                         PHA_MAT_DATA_RAW(joker, t_index,
                                                          i_za_sca,
                                                          i_aa_sca, i_za_inc,
                                                          i_aa_inc, i),
                                         freq_gp);
                              }
                          }
                      }
                  }
              }
              //Extinction matrix data
              scat_data_mono[i_ss][i_se].ext_mat_data.resize(1, T_DATAGRID.nelem(),
                                                       EXT_MAT_DATA_RAW.npages(),
                                                       EXT_MAT_DATA_RAW.nrows(),
                                                       EXT_MAT_DATA_RAW.ncols());
              for (Index i_za_sca = 0; i_za_sca < EXT_MAT_DATA_RAW.npages();
                   i_za_sca++)
              {
                  for(Index i_aa_sca = 0; i_aa_sca < EXT_MAT_DATA_RAW.nrows();
                      i_aa_sca++)
                  {
                      //
                      // Interpolation of extinction matrix:
                      //
                      for (Index i = 0; i < EXT_MAT_DATA_RAW.ncols(); i++)
                      {
                          scat_data_mono[i_ss][i_se].ext_mat_data(0, t_index,
                                                            i_za_sca, i_aa_sca, i)
                          = interp(itw, EXT_MAT_DATA_RAW(joker, t_index, i_za_sca,
                                                         i_aa_sca, i),
                                   freq_gp);
                      }
                  }
              }
              //Absorption vector data
              scat_data_mono[i_ss][i_se].abs_vec_data.resize(1, T_DATAGRID.nelem(),
                                                       ABS_VEC_DATA_RAW.npages(),
                                                       ABS_VEC_DATA_RAW.nrows(),
                                                       ABS_VEC_DATA_RAW.ncols());
              for (Index i_za_sca = 0; i_za_sca < ABS_VEC_DATA_RAW.npages() ;
                   i_za_sca++)
              {
                  for(Index i_aa_sca = 0; i_aa_sca < ABS_VEC_DATA_RAW.nrows();
                      i_aa_sca++)
                  {
                      //
                      // Interpolation of absorption vector:
                      //
                      for (Index i = 0; i < ABS_VEC_DATA_RAW.ncols(); i++)
                      {
                          scat_data_mono[i_ss][i_se].abs_vec_data(0, t_index, i_za_sca,
                                                            i_aa_sca, i) =
                          interp(itw, ABS_VEC_DATA_RAW(joker, t_index, i_za_sca,
                                                       i_aa_sca, i),
                                 freq_gp);
                      }
                  }
              }
          }
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void opt_prop_sptFromMonoData(// Output and Input:
                              Tensor3& ext_mat_spt,
                              Matrix& abs_vec_spt,
                              // Input:
                              const ArrayOfArrayOfSingleScatteringData& scat_data_mono,
                              const Vector& scat_za_grid,
                              const Vector& scat_aa_grid,
                              const Index& scat_za_index, // propagation directions
                              const Index& scat_aa_index,
                              const Numeric& rtp_temperature,
                              const Tensor4& pnd_field, 
                              const Index& scat_p_index,
                              const Index& scat_lat_index,
                              const Index& scat_lon_index,
                              const Verbosity& verbosity)
{
  DEBUG_ONLY(const Index N_se_total = TotalNumberOfElements(scat_data_mono);)
  const Index stokes_dim = ext_mat_spt.ncols();
  const Numeric za_sca = scat_za_grid[scat_za_index];
  const Numeric aa_sca = scat_aa_grid[scat_aa_index];
  
  if (stokes_dim > 4 || stokes_dim < 1){
    throw runtime_error("The dimension of the stokes vector \n"
                         "must be 1,2,3 or 4");
  }
  
  assert( ext_mat_spt.npages() == N_se_total );
  assert( abs_vec_spt.nrows() == N_se_total );

  // Check that we do indeed have scat_data_mono here. Only checking the first
  // scat element, assuming the other elements have been processed in the same
  // manner. That's save against having scat_data here if that originated from
  // scat_data reading routines (ScatSpecies/Element*Add/Read), it's not safe
  // against data read by ReadXML directly or if scat_data has been (partly)
  // produced from scat_data_singleTmatrix. That would be too costly here,
  // though.
  // Also, we can't check here whether data is at the correct frequency since we
  // don't know f_grid and f_index here (we could pass it in, though).
  if( scat_data_mono[0][0].f_grid.nelem() > 1 )
  {
      ostringstream os;
      os << "Scattering data seems to be scat_data (several freq points),\n"
         << "but scat_data_mono (1 freq point only) is expected here.";
      throw runtime_error( os.str() );
  }

  // Initialisation
  ext_mat_spt = 0.;
  abs_vec_spt = 0.;

  GridPos t_gp;
  
  Vector itw(2);

  Index i_se_flat = 0;
  // Loop over the included scattering elements
  for (Index i_ss = 0; i_ss < scat_data_mono.nelem(); i_ss++)
  {
      // Loop over the included scattering elements
      for (Index i_se = 0; i_se < scat_data_mono[i_ss].nelem(); i_se++)
      {
          // If the particle number density at a specific point in the
          // atmosphere for the i_se scattering element is zero, we don't need
          // to do the transformation!
          if (pnd_field(i_se_flat, scat_p_index, scat_lat_index, scat_lon_index)
              > PND_LIMIT)
          {

              // First we have to transform the data from the coordinate system
              // used in the database (depending on the kind of ptype) to the
              // laboratory coordinate system.

              //
              // Do the transformation into the laboratory coordinate system.
              //
              // Extinction matrix:
              //
              Index ext_npages = scat_data_mono[i_ss][i_se].ext_mat_data.npages();
              Index ext_nrows = scat_data_mono[i_ss][i_se].ext_mat_data.nrows();
              Index ext_ncols = scat_data_mono[i_ss][i_se].ext_mat_data.ncols();
              Index abs_npages = scat_data_mono[i_ss][i_se].abs_vec_data.npages();
              Index abs_nrows = scat_data_mono[i_ss][i_se].abs_vec_data.nrows();
              Index abs_ncols = scat_data_mono[i_ss][i_se].abs_vec_data.ncols();
              Tensor3 ext_mat_data1temp(ext_npages,ext_nrows,ext_ncols,0.0);
              Tensor3 abs_vec_data1temp(abs_npages,abs_nrows,abs_ncols,0.0);

              //Check that scattering data temperature range covers required temperature
              ConstVectorView t_grid = scat_data_mono[i_ss][i_se].T_grid;

              if (t_grid.nelem() > 1)
              {
                  ostringstream os;
                  os << "The temperature grid of the scattering data does not\n"
                     << "cover the atmospheric temperature at cloud location.\n"
                     << "The data should include the value T = "
                     << rtp_temperature << " K.";
                  chk_interpolation_grids( os.str(), t_grid, rtp_temperature );

                  //interpolate over temperature
                  gridpos(t_gp, t_grid, rtp_temperature);
                  interpweights(itw, t_gp);
                  for (Index i_p = 0; i_p < ext_npages ; i_p++)
                  {
                      for (Index i_r = 0; i_r < ext_nrows ; i_r++)
                      {
                          for (Index i_c = 0; i_c < ext_ncols ; i_c++)
                          {
                              ext_mat_data1temp(i_p,i_r,i_c) = 
                                interp(itw,
                                       scat_data_mono[i_ss][i_se].ext_mat_data( 
                                       0,joker,i_p,i_r,i_c),
                                       t_gp);
                          }
                      }
                  }
              }
              else
              {
                  ext_mat_data1temp =
                  scat_data_mono[i_ss][i_se].ext_mat_data(0,0,joker,joker,joker);
              }

              ext_matTransform(ext_mat_spt(i_se_flat, joker, joker),
                               ext_mat_data1temp,
                               scat_data_mono[i_ss][i_se].za_grid,
                               scat_data_mono[i_ss][i_se].aa_grid,
                               scat_data_mono[i_ss][i_se].ptype,
                               za_sca, aa_sca,
                               verbosity);
              //
              // Absorption vector:
              //

              if (t_grid.nelem() > 1)
              {
                  //interpolate over temperature
                  for (Index i_p = 0; i_p < abs_npages ; i_p++)
                  {
                      for (Index i_r = 0; i_r < abs_nrows ; i_r++)
                      {
                          for (Index i_c = 0; i_c < abs_ncols ; i_c++)
                          {
                              abs_vec_data1temp(i_p,i_r,i_c) =
                              interp(itw,
                                     scat_data_mono[i_ss][i_se].abs_vec_data( 
                                     0,joker,i_p,i_r,i_c),
                                     t_gp);
                          }
                      }
                  }
              }
              else
              {
                  abs_vec_data1temp =
                  scat_data_mono[i_ss][i_se].abs_vec_data(0,0,joker,joker,joker);
              }

              abs_vecTransform(abs_vec_spt(i_se_flat, joker),
                               abs_vec_data1temp,
                               scat_data_mono[i_ss][i_se].za_grid,
                               scat_data_mono[i_ss][i_se].aa_grid,
                               scat_data_mono[i_ss][i_se].ptype,
                               za_sca, aa_sca,
                               verbosity);                
          }
          
          i_se_flat++;
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void pha_mat_sptFromMonoData(// Output:
                             Tensor5& pha_mat_spt,
                             // Input:
                             const ArrayOfArrayOfSingleScatteringData& scat_data_mono,
                             const Index& doit_za_grid_size,
                             const Vector& scat_aa_grid,
                             const Index& scat_za_index, // propagation directions
                             const Index& scat_aa_index,
                             const Numeric& rtp_temperature,
                             const Tensor4& pnd_field, 
                             const Index& scat_p_index,
                             const Index& scat_lat_index,
                             const Index& scat_lon_index,
                             const Verbosity& verbosity)
{
  CREATE_OUT3;
  
  out3 << "Calculate *pha_mat_spt* from scat_data_mono. \n";
  
  Vector za_grid;
  nlinspace(za_grid, 0, 180, doit_za_grid_size); 

  const Index N_se_total = TotalNumberOfElements(scat_data_mono);
  if( N_se_total != pnd_field.nbooks() )
    {
      ostringstream os;
      os << "Total number of scattering elements in scat_data(_mono) "
         << "inconsistent with size of pnd_field.";
      throw runtime_error(os.str());
    }
  // as pha_mat_spt is typically initialized from pnd_field, this theoretically
  // checks the same as the runtime_error above. Still, we keep it to be on the
  // save side.
  assert( pha_mat_spt.nshelves() == N_se_total );

  const Index stokes_dim = pha_mat_spt.ncols();
  if (stokes_dim > 4 || stokes_dim < 1){
    throw runtime_error("The dimension of the stokes vector \n"
                        "must be 1,2,3 or 4");
  }
  
  // Check that we do indeed have scat_data_mono here. Only checking the first
  // scat element, assuming the other elements have been processed in the same
  // manner. That's save against having scat_data here if that originated from
  // scat_data reading routines (ScatSpecies/Element*Add/Read), it's not safe
  // against data read by ReadXML directly or if scat_data has been (partly)
  // produced from scat_data_singleTmatrix. That would be too costly here,
  // though.
  // Also, we can't check here whether data is at the correct frequency since we
  // don't know f_grid and f_index here (we could pass it in, though).
  if( scat_data_mono[0][0].f_grid.nelem() > 1 )
  {
      ostringstream os;
      os << "Scattering data seems to be scat_data (several freq points),\n"
         << "but scat_data_mono (1 freq point only) is expected here.";
      throw runtime_error( os.str() );
  }

  GridPos T_gp;
  Vector itw(2);

  // Initialisation
  pha_mat_spt = 0.;

  Index i_se_flat = 0;
  for (Index i_ss = 0; i_ss < scat_data_mono.nelem(); i_ss ++)
  {
      for (Index i_se = 0; i_se < scat_data_mono[i_ss].nelem(); i_se ++)
      {
          // If the particle number density at a specific point in the
          // atmosphere for scattering element i_se is zero, we don't need to
          // do the transformation!
          if (pnd_field(i_se_flat, scat_p_index, scat_lat_index, scat_lon_index)
              > PND_LIMIT)
          {
              // Temporary phase matrix which includes all temperatures.
              Tensor3 pha_mat_spt_tmp(scat_data_mono[i_ss][i_se].T_grid.nelem(),
                                      pha_mat_spt.nrows(), pha_mat_spt.ncols());

              pha_mat_spt_tmp = 0.;

              if( scat_data_mono[i_ss][i_se].T_grid.nelem() > 1 )
              {
                  ostringstream os;
                  os << "The temperature grid of the scattering data does not\n"
                     << "cover the atmospheric temperature at cloud location.\n"
                     << "The data should include the value T = "
                     << rtp_temperature << " K.";
                  chk_interpolation_grids( os.str(), 
                                           scat_data_mono[i_ss][i_se].T_grid, 
                                           rtp_temperature );

                  // Gridpositions:
                  gridpos( T_gp, scat_data_mono[i_ss][i_se].T_grid, 
                           rtp_temperature );
                  // Interpolation weights:
                  interpweights(itw, T_gp);
              }

/*
              if( i_se_flat==65 && rtp_temperature==292.61 )
                {
                  Tensor7 p=scat_data_mono[i_ss][i_se].pha_mat_data;
                  cout << "#original pfct:\n";
                  for( Index i_za=0; i_za<p.nshelves(); i_za++ )
                  {
                    cout << "  "
                         << scat_data_mono[i_ss][i_se].za_grid[i_za];
                    for( Index i_t=0; i_t<p.nvitrines(); i_t++ )
                      cout << "  " << p(0,i_t,i_za,0,0,0,0);
                    cout << "\n";
                  }
                }

              Tensor3 pha_mat_lab;
              if( i_se_flat==65 )
              {
                pha_mat_lab.resize( scat_data_mono[i_ss][i_se].T_grid.nelem(),
                                    doit_za_grid_size,scat_aa_grid.nelem() );
                pha_mat_lab=0.;
              }
*/
              // Do the transformation into the laboratory coordinate system.
              for (Index za_inc_idx = 0; za_inc_idx < doit_za_grid_size;
                   za_inc_idx ++)
              {
                  for (Index aa_inc_idx = 0; aa_inc_idx < scat_aa_grid.nelem();
                       aa_inc_idx ++)
                  {
                      for (Index t_idx = 0; t_idx <
                           scat_data_mono[i_ss][i_se].T_grid.nelem();
                           t_idx ++)
                      {
                          pha_matTransform( pha_mat_spt_tmp(t_idx, joker, joker),
                                           scat_data_mono[i_ss][i_se].
                                           pha_mat_data
                                           (0,t_idx,joker,joker,joker,
                                            joker,joker),
                                           scat_data_mono[i_ss][i_se].za_grid,
                                           scat_data_mono[i_ss][i_se].aa_grid,
                                           scat_data_mono[i_ss][i_se].ptype,
                                           scat_za_index, scat_aa_index,
                                           za_inc_idx,
                                           aa_inc_idx, za_grid, scat_aa_grid,
                                           verbosity );
                      }
/*
                      if( i_se_flat==65 )
                        pha_mat_lab(joker, za_inc_idx, aa_inc_idx) = 
                          pha_mat_spt_tmp(joker, 0, 0);
*/
                      // Temperature interpolation
                      if( scat_data_mono[i_ss][i_se].T_grid.nelem() > 1)
                      {
                          for (Index i = 0; i< stokes_dim; i++)
                          {
                              for (Index j = 0; j< stokes_dim; j++)
                              {
                                  pha_mat_spt(i_se_flat,
                                              za_inc_idx, aa_inc_idx, i, j)=
                                  interp(itw,
                                         pha_mat_spt_tmp(joker, i, j), T_gp);
                              }
                          }
                      }
                      else // no temperature interpolation required
                      {
                          pha_mat_spt(i_se_flat,
                                      za_inc_idx, aa_inc_idx, joker, joker) =
                          pha_mat_spt_tmp(0, joker, joker);
                      }
                  }
              }
/*
              if( i_se_flat==65 && rtp_temperature==292.61 )
                {
                  cout << "\n#angle-converted pfct:\n";
                  for( Index i_za=0; i_za<pha_mat_lab.nrows(); i_za++ )
                  {
                    cout << "  "
                         << za_grid[i_za];
                    for( Index i_t=0; i_t<pha_mat_lab.npages(); i_t++ )
                      cout << "  " << pha_mat_lab(i_t,i_za,0);
                    cout << "\n";
                  }
                  cout << "\n#temperature-interpolated pfct:\n";
                  for( Index i_za=0; i_za<pha_mat_lab.nrows(); i_za++ )
                  {
                    cout << "  "
                         << za_grid[i_za];
                    cout << "  " << pha_mat_spt(i_se_flat,i_za,0,0,0);
                    cout << "\n";
                  }
                }
*/
         }

          i_se_flat++;
      }
  }
}


/* Workspace method: Doxygen documentation will be auto-generated */
void ScatSpeciesMerge(//WS Output:
                      Tensor4& pnd_field,
                      ArrayOfArrayOfSingleScatteringData& scat_data,
                      ArrayOfArrayOfScatteringMetaData& scat_meta,
                      ArrayOfString& scat_species,
                      Index& cloudbox_checked,
                      //WS Input:
                      const Index& atmosphere_dim,
                      const Index& cloudbox_on,
                      const ArrayOfIndex& cloudbox_limits,
                      const Tensor3& t_field,
                      const Tensor3& z_field,
                      const Matrix& z_surface,
                      const Verbosity& /*verbosity*/)
{
    // cloudbox variable state should be ok before entering here
    if (!cloudbox_checked)
        throw std::runtime_error(
              "You must call *cloudbox_checkedCalc* before this method.");
    //however, we modify cloudbox variables. hence force re-checking the new
    //variables by resetting cloudbox_checked to False.
    cloudbox_checked = 0;
    
    if (atmosphere_dim != 1)
        throw std::runtime_error(
              "Merging scattering elements only works with a 1D atmoshere");

    // Cloudbox on/off?
    if ( !cloudbox_on )
    {
        /* Must initialise pnd_field anyway; but empty */
        pnd_field.resize(0, 0, 0, 0);
        return;
    }
    
    // ------- setup new pnd_field and scat_data -------------------
    ArrayOfIndex limits(2);
    //pressure
    limits[0] = cloudbox_limits[0];
    limits[1] = cloudbox_limits[1] + 1;

    Tensor4 pnd_field_merged(limits[1] - limits[0],
                          limits[1] - limits[0],
                          1,
                          1,
                          0.);

    ArrayOfArrayOfSingleScatteringData scat_data_merged;
    scat_data_merged.resize(1);
    scat_data_merged[0].resize(pnd_field_merged.nbooks());
    ArrayOfArrayOfScatteringMetaData scat_meta_merged;
    scat_meta_merged.resize(1);
    scat_meta_merged[0].resize(pnd_field_merged.nbooks());
    ArrayOfString scat_species_merged;
    scat_species_merged.resize(1);
    scat_species_merged[0] = "mergedfield-mergedpsd";
    for (Index sp = 0; sp < scat_data_merged[0].nelem(); sp++)
    {
        SingleScatteringData &this_part = scat_data_merged[0][sp];
        this_part.ptype = scat_data[0][0].ptype;
        this_part.description = "Merged scattering elements";
        this_part.f_grid = scat_data[0][0].f_grid;
        this_part.za_grid = scat_data[0][0].za_grid;
        this_part.aa_grid = scat_data[0][0].aa_grid;
        this_part.pha_mat_data.resize(scat_data[0][0].pha_mat_data.nlibraries(),
                                      1,
                                      scat_data[0][0].pha_mat_data.nshelves(),
                                      scat_data[0][0].pha_mat_data.nbooks(),
                                      scat_data[0][0].pha_mat_data.npages(),
                                      scat_data[0][0].pha_mat_data.nrows(),
                                      scat_data[0][0].pha_mat_data.ncols());
        this_part.ext_mat_data.resize(scat_data[0][0].ext_mat_data.nshelves(),
                                      1,
                                      scat_data[0][0].ext_mat_data.npages(),
                                      scat_data[0][0].ext_mat_data.nrows(),
                                      scat_data[0][0].ext_mat_data.ncols());
        this_part.abs_vec_data.resize(scat_data[0][0].abs_vec_data.nshelves(),
                                      1,
                                      scat_data[0][0].abs_vec_data.npages(),
                                      scat_data[0][0].abs_vec_data.nrows(),
                                      scat_data[0][0].abs_vec_data.ncols());
        this_part.pha_mat_data = 0.;
        this_part.ext_mat_data = 0.;
        this_part.abs_vec_data = 0.;
        this_part.T_grid.resize(1);
        this_part.T_grid[0] = t_field(sp, 0, 0);

        ScatteringMetaData &this_meta = scat_meta_merged[0][sp];
        ostringstream os;
        os << "Merged scattering element of cloudbox-level #" << sp;
        this_meta.description = os.str();
        this_meta.source = "ARTS internal";
        this_meta.refr_index = "Unknown";
        this_meta.mass = -1.;
        this_meta.diameter_max = -1.;
        this_meta.diameter_volume_equ = -1.;
        this_meta.diameter_area_equ_aerodynamical = -1.;
    }

    // Check that all scattering elements have same ptype and data dimensions
    SingleScatteringData &first_part = scat_data[0][0];
    for (Index i_ss = 0; i_ss < scat_data.nelem(); i_ss++)
    {
        for (Index i_se = 0; i_se < scat_data[i_ss].nelem(); i_se++)
        {
            SingleScatteringData &orig_part = scat_data[i_ss][i_se];

            if (orig_part.ptype != first_part.ptype)
                throw std::runtime_error(
                  "All scattering elements must have the same type");

            if (orig_part.f_grid.nelem() != first_part.f_grid.nelem())
                throw std::runtime_error(
                  "All scattering elements must have the same f_grid");

            if (!is_size(orig_part.pha_mat_data(joker, 0, joker, joker,
                                                joker, joker, joker),
                         first_part.pha_mat_data.nlibraries(),
                         first_part.pha_mat_data.nshelves(),
                         first_part.pha_mat_data.nbooks(),
                         first_part.pha_mat_data.npages(),
                         first_part.pha_mat_data.nrows(),
                         first_part.pha_mat_data.ncols()
                         ))
                throw std::runtime_error(
                  "All scattering elements must have the same pha_mat_data size"
                  " (except for temperature).");

            if (!is_size(orig_part.ext_mat_data(joker, 0, joker, joker, joker),
                         first_part.ext_mat_data.nshelves(),
                         first_part.ext_mat_data.npages(),
                         first_part.ext_mat_data.nrows(),
                         first_part.ext_mat_data.ncols()
                         ))
                throw std::runtime_error(
                  "All scattering elements must have the same ext_mat_data size"
                  " (except for temperature).");

            if (!is_size(orig_part.abs_vec_data(joker, 0, joker, joker, joker),
                         first_part.abs_vec_data.nshelves(),
                         first_part.abs_vec_data.npages(),
                         first_part.abs_vec_data.nrows(),
                         first_part.abs_vec_data.ncols()
                         ))
                throw std::runtime_error(
                  "All scattering elements must have the same abs_vec_data size"
                  " (except for temperature).");
        }
    }
    
    //----- Start pnd_field_merged and scat_data_array_merged calculations -----
    
    GridPos T_gp;
    Vector itw(2);
    
    Index nlevels = pnd_field_merged.nbooks();
    // loop over pressure levels in cloudbox
    for (Index i_lv = 0; i_lv < nlevels-1; i_lv++)
    {
        pnd_field_merged(i_lv,i_lv,0,0) = 1.;

        SingleScatteringData &this_part = scat_data_merged[0][i_lv];
        for (Index i_ss = 0; i_ss < scat_data.nelem(); i_ss++)
        {
            for (Index i_se = 0; i_se < scat_data[i_ss].nelem(); i_se++)
            {
                SingleScatteringData &orig_part = scat_data[i_ss][i_se];
                const Index pnd_index = FlattenedIndex(scat_data, i_ss, i_se);

                // If the particle number density at a specific point in the
                // atmosphere for the i_se scattering element is zero, we don't
                // need to do the transformation!
                if (pnd_field(pnd_index, i_lv, 0, 0) > PND_LIMIT) //TRS
                {
                    Numeric temperature = this_part.T_grid[0];
                    if( orig_part.T_grid.nelem() > 1)
                    {
                        ostringstream os;
                        os << "The temperature grid of the scattering data "
                           << "does not cover the\n"
                           << "atmospheric temperature at cloud location. "
                           << "The data should\n"
                           << "include the value T = "<< temperature << " K.\n"
                           << "Offending particle is scat_data[" << i_ss
                           << "][" << i_se << "]:\n"
                           << "Description: " << orig_part.description << "\n";
                        chk_interpolation_grids(os.str(), orig_part.T_grid,
                                                temperature);

                        // Gridpositions:
                        gridpos( T_gp, orig_part.T_grid, temperature );
                        // Interpolation weights:
                        interpweights(itw, T_gp);
                    }

                    ////////// Extinction matrix and absorption vector

                    // Loop over frequencies
                    for (Index i_f = 0;
                         i_f < orig_part.pha_mat_data.nlibraries(); i_f++)
                    {

                        // Loop over zenith angles
                        for (Index i_za = 0;
                             i_za < orig_part.ext_mat_data.npages(); i_za++)
                        {
                            // Loop over azimuth angles
                            for (Index i_aa = 0;
                                 i_aa < orig_part.ext_mat_data.nrows(); i_aa++)
                            {
                                // Weighted sum of ext_mat_data and abs_vec_data
                                if( orig_part.T_grid.nelem() == 1)
                                {
                                    Vector v = orig_part.ext_mat_data(i_f, 0, i_za, i_aa, joker);
                                    v *= pnd_field(pnd_index, i_lv, 0, 0);
                                    this_part.ext_mat_data(i_f, 0, i_za, 0, joker) += v;

                                    v = orig_part.abs_vec_data(i_f, 0, i_za, i_aa, joker);
                                    v *= pnd_field(pnd_index, i_lv, 0, 0);
                                    this_part.abs_vec_data(i_f, 0, i_za, i_aa, joker) += v;
                                }
                                else
                                {
                                    for (Index i = 0;
                                         i < orig_part.ext_mat_data.ncols(); i++)
                                    {
                                        // Temperature interpolation
                                        this_part.ext_mat_data(i_f, 0, i_za, i_aa, i) +=
                                        pnd_field(pnd_index, i_lv, 0, 0)
                                        * interp(itw,
                                                 orig_part.ext_mat_data(i_f, joker, i_za, i_aa, i),
                                                 T_gp);
                                    }
                                    for (Index i = 0;
                                         i < orig_part.abs_vec_data.ncols(); i++)
                                    {
                                        // Temperature interpolation
                                        this_part.abs_vec_data(i_f, 0, i_za, i_aa, i) +=
                                        pnd_field(pnd_index, i_lv, 0, 0)
                                        * interp(itw,
                                                 orig_part.abs_vec_data(i_f, joker, i_za, i_aa, i),
                                                 T_gp);
                                    }
                                }
                            }
                        }

                        ////////// Phase matrix

                        // Loop over outgoing zenith angles
                        for (Index i_za_out = 0;
                             i_za_out < orig_part.pha_mat_data.nshelves(); i_za_out++)
                        {
                            // Loop over outgoing azimuth angles
                            for (Index i_aa_out = 0;
                                 i_aa_out < orig_part.pha_mat_data.nbooks(); i_aa_out++)
                            {
                                // Loop over incoming zenith angles
                                for (Index i_za_inc = 0;
                                     i_za_inc < orig_part.pha_mat_data.npages(); i_za_inc++)
                                {
                                    // Loop over incoming azimuth angles
                                    for (Index i_aa_inc = 0;
                                         i_aa_inc < orig_part.pha_mat_data.nrows(); i_aa_inc++)
                                    {
                                        // Weighted sum of pha_mat_data
                                        if( orig_part.T_grid.nelem() == 1)
                                        {
                                            Vector v = orig_part.pha_mat_data(i_f, 0,
                                                                              i_za_out, i_aa_out,
                                                                              i_za_inc, i_aa_inc,
                                                                              joker);
                                            v *= pnd_field(pnd_index, i_lv, 0, 0);
                                            this_part.pha_mat_data(i_f, 0,
                                                                   i_za_out, i_aa_out,
                                                                   i_za_inc, i_aa_inc, joker) = v;
                                        }
                                        else
                                        {
                                            // Temperature interpolation
                                            for (Index i = 0;
                                                 i < orig_part.pha_mat_data.ncols(); i++)
                                            {
                                                this_part.pha_mat_data(i_f, 0, i_za_out,
                                                                       i_aa_out, i_za_inc, i_aa_inc, i) +=
                                                pnd_field(pnd_index, i_lv, 0, 0)
                                                * interp(itw,
                                                         orig_part.pha_mat_data(i_f, joker,
                                                                                i_za_out, i_aa_out,
                                                                                i_za_inc, i_aa_inc, i),
                                                         T_gp);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Set new pnd_field at lowest altitude to 0 if the cloudbox doesn't touch
    // the ground.
    // The consistency for the original pnd_field has already been ensured by
    // cloudbox_checkedCalc
    if (z_field(cloudbox_limits[0], 0, 0) > z_surface(0, 0))
        pnd_field_merged(0, 0, 0, 0) = 0.;

    pnd_field = pnd_field_merged;
    scat_data = scat_data_merged;
    scat_meta = scat_meta_merged;
    scat_species = scat_species_merged;
}



/* Workspace method: Doxygen documentation will be auto-generated */
void ExtractFromMetaSingleScatSpecies(
                     //WS Output:
                     Vector& meta_param,
                     //WS Input:
                     const ArrayOfArrayOfScatteringMetaData& scat_meta,
                     const String& meta_name,
                     const Index& scat_species_index,
                     const Verbosity& /*verbosity*/)
{
    if ( scat_species_index<0 )
    {
      ostringstream os;
      os << "scat_species_index can't be <0!";
      throw runtime_error( os.str() );
    }

    const Index nss = scat_meta.nelem();

    // check that scat_meta actually has at least scat_species_index elements
    if ( !(nss>scat_species_index) )
    {
      ostringstream os;
      os << "Can not extract data for scattering species #"
         << scat_species_index << "\n"
         << "because scat_meta has only " << nss << " elements.";
      throw runtime_error( os.str() );
    }

    const Index nse = scat_meta[scat_species_index].nelem();
    meta_param.resize(nse);

    for ( Index i=0; i<nse; i++ )
      {
        if ( meta_name=="mass" )
          meta_param[i] =
            scat_meta[scat_species_index][i].mass;
        else if ( meta_name=="diameter_max" )
          meta_param[i] =
            scat_meta[scat_species_index][i].diameter_max;
        else if ( meta_name=="diameter_volume_equ" )
          meta_param[i] =
            scat_meta[scat_species_index][i].diameter_volume_equ;
        else if ( meta_name=="diameter_area_equ_aerodynamical" )
          meta_param[i] =
            scat_meta[scat_species_index][i].diameter_area_equ_aerodynamical;
        else
          {
            ostringstream os;
            os << "Meta parameter \"" << meta_name << "\"is unknown.";
            throw runtime_error( os.str() );
          }
      }
}



/* Workspace method: Doxygen documentation will be auto-generated */
void TestScatDataInterp(
   const ArrayOfArrayOfSingleScatteringData&   scat_data,
   const Index&       stokes_dim,
   const Index&       atmosphere_dim,
   const Vector&      f_grid,
   const Vector&      rtp_los,
   const Numeric&     rtp_temperature,
   const Index&       scat_elem_index,
   const Index&       compare,
   const Index&       za_printinfo_index,
   const Index&       aa_printinfo_index,
   const Verbosity&   verbosity )
{

  cout << endl;
  cout << "========== START ==========" << endl;
  cout << endl;
  cout << "LOS direction: " << rtp_los << endl;

  if ( (rtp_los[0] > 180. ) || (rtp_los[0] < 0.) )
    throw runtime_error( "LOS zenith angle must be between 0 and 180deg." );
  if ( (rtp_los[1] > 180. ) || (rtp_los[1] < -180.) )
    throw runtime_error( "LOS azimuth angle must be between -180 and 180deg." );
  
  // Hard-coded grids
  const Vector pha_mat_za( 0.0, 37, 5.0 );
  const Vector pha_mat_aa( -180.0, 37, 10.0 );

  // Some sizes
  Index N_se = TotalNumberOfElements(scat_data);
  const Index n_za        = pha_mat_za.nelem();
  const Index n_aa        = pha_mat_aa.nelem();
  const Index n_f = f_grid.nelem();

  // Input checks
  if( f_grid.nelem() != 1 )
    throw runtime_error( "Only length 1 *f_grid* allowed." );
  if( rtp_los.nelem() != 2 )
    throw runtime_error( "Only length 2 *rtp_los* allowed." );
  if( scat_elem_index < 0  ||  scat_elem_index >= N_se )
    throw runtime_error( "Invalid choice for *scat_element_index*" );

  Index printinfo=1;
  if ( za_printinfo_index<0 || aa_printinfo_index<0 )
    printinfo=0;
  else if ( za_printinfo_index>=n_za || aa_printinfo_index>=n_aa )
    {
      ostringstream os;
      os << "Printout indices too large (requirement: za<" << n_za << ", aa<"
         << n_aa << ").\n";
      throw runtime_error( os.str() );
    }

  // Common variables
  const Index f_index = 0;
  //
  Vector pnd_vec(N_se,0); pnd_vec[scat_elem_index] = 1;

  
  ArrayOfArrayOfSingleScatteringData   scat_data_mono;
  //
  scat_data_monoCalc( scat_data_mono, scat_data,
                      f_grid, f_index, verbosity );
  //

  ////// Monte Carlo //////
  //
  Matrix ext_mat_mc( stokes_dim, stokes_dim, 0.0 );
  Vector abs_vec_mc( stokes_dim, 0.0 );
  Tensor4 pha_mat_mc( n_za, n_aa, stokes_dim, stokes_dim, 0.0 );
  //
  Vector out;
  mirror_los( out, rtp_los, 3 );

  //  
  opt_propCalc( ext_mat_mc, abs_vec_mc, out[0], out[1], scat_data_mono,
                stokes_dim, pnd_vec, rtp_temperature, verbosity );
  //
  for( Index iz=0; iz<n_za; iz++ )
    {
      Vector in_los(2);
      in_los[0] = pha_mat_za[iz];
      
      for( Index ia=0; ia<n_aa; ia++ )
        {
          Vector inc;
          in_los[1] = pha_mat_aa[ia];
          mirror_los( inc, in_los, 3 );
          pha_mat_singleCalc( pha_mat_mc(iz,ia,joker,joker),
                              out[0], out[1], inc[0], inc[1],
                              scat_data_mono, stokes_dim, pnd_vec,
                              rtp_temperature, verbosity );
        }
    }

  if (printinfo)
    {
      Vector in_los(2);
      in_los[0] = pha_mat_za[za_printinfo_index];
      in_los[1] = pha_mat_aa[aa_printinfo_index];
      cout << "Incident LOS direction: " << in_los << endl;
      cout << endl;

      Vector inc;
      mirror_los( inc, in_los, 3 );
      cout << "----- MC -----" << endl;
      cout << "photon propagation direction: " << out << endl;
      cout << "incident scattered photon propagation direction: " << inc << endl;
      cout << endl;

      cout << "absorption vector:\n" << abs_vec_mc << endl;
      cout << "extinction matrix:\n" << ext_mat_mc << endl;
      cout << "phase matrix (" << za_printinfo_index
           << "," << aa_printinfo_index << "):\n"
           << pha_mat_mc(za_printinfo_index,aa_printinfo_index,joker,joker)
           << endl;
      cout << endl;
    }



  ////// RT4 //////
  //

  // RT4 (acc. to full RT results seems to be consistent with DOIT. and is
  // easier to handle (aka follow code flow) for me (JM) than DOIT
  // implementation.
  Matrix abs_vec_spt(N_se, stokes_dim, 0.);
  Tensor3 ext_mat_spt(N_se, stokes_dim, stokes_dim, 0.);
  Matrix abs_vec(n_f, stokes_dim, 0.);
  Tensor3 ext_mat(n_f, stokes_dim, stokes_dim, 0.);
  Tensor4 pha_mat_rt4( n_za, n_aa, stokes_dim, stokes_dim, 0.0 );

  Tensor4 pnd(N_se, 1, 1, 1, 0.);
  pnd(scat_elem_index,0,0,0) = 1.;
  Vector sza_grid(1);
  sza_grid[0] = rtp_los[0];
  Vector saa_grid(1);
  saa_grid[0] = rtp_los[1];
  Vector siza_grid(n_za+1);
  Vector siaa_grid(n_aa+1);
  siza_grid[0] = sza_grid[0];
  siza_grid[Range(1,n_za)] = pha_mat_za;
  siaa_grid[0] = saa_grid[0];
  siaa_grid[Range(1,n_aa)] = pha_mat_aa;


  opt_prop_sptFromMonoData( ext_mat_spt, abs_vec_spt,
                            scat_data_mono, sza_grid, saa_grid,
                            0, 0, rtp_temperature,
                            pnd, 0, 0, 0, verbosity );

  ext_matInit( ext_mat, f_grid, stokes_dim, f_index, verbosity );
  abs_vecInit( abs_vec, f_grid, stokes_dim, f_index, verbosity );
  ext_matAddPart( ext_mat, ext_mat_spt, pnd, atmosphere_dim, 0, 0, 0, verbosity );
  abs_vecAddPart( abs_vec, abs_vec_spt, pnd, atmosphere_dim, 0, 0, 0, verbosity );

  Index i_se_flat=0;
  for (Index i_ss = 0; i_ss < scat_data_mono.nelem(); i_ss++)
    for (Index i_se = 0; i_se < scat_data_mono[i_ss].nelem(); i_se++)
    {
      if (i_se_flat == scat_elem_index)
      {
        SingleScatteringData ssd=scat_data_mono[i_ss][i_se];
        Index i_pfct = ssd.T_grid.nelem()/2;

        if (ssd.ptype == PTYPE_MACROS_ISO)
          for (Index iza=0; iza<n_za; iza++)
            for (Index iaa=0; iaa<n_aa; iaa++)
              pha_matTransform( pha_mat_rt4(iza,iaa,joker,joker),
                                ssd.pha_mat_data(0,i_pfct,joker,
                                                 joker,joker,joker,joker),
                                ssd.za_grid, ssd.aa_grid, ssd.ptype,
                                0, 0, iza+1, iaa+1,
                                siza_grid, siaa_grid,
                                verbosity );

        else if (ssd.ptype == PTYPE_HORIZ_AL)
          {
            Index nza_se = ssd.za_grid.nelem();
            ConstVectorView za_datagrid = ssd.za_grid;
            ConstVectorView this_za_datagrid =
              za_datagrid[Range(0,ssd.pha_mat_data.npages())];

            // in the actual SSP prep for RT4, the phase matrix is first
            // extracted and the azimuthal average (0th Fourier mode) is derived
            // at and based on the scatt elements' own polar angle grid.
            // That's not appropriate here for comparison to MC extraction,
            // hence here we interpolate to the requested azimuthal angles at
            // the scatt elements' own polar angles.
            // That is, we keep the original scat angle grids (inc and sca!)
            // meaning we loop over them and derive a pha_mat for each of them
            // (and each requested incident azimuth for fixed scattered aimuth)
            for (Index iaa=0; iaa<n_aa; iaa++)
              {
                Tensor4 pha_mat_interp(nza_se,nza_se/2+1,stokes_dim,stokes_dim,0.);

                GridPos daa_gp;
                Vector itw_aa(2);
                Numeric daa = abs(saa_grid[0]-pha_mat_aa[iaa]);
                if (daa>180.)
                  daa = 360.-daa;

                gridpos(daa_gp,ssd.aa_grid,daa);
                interpweights(itw_aa,daa_gp);

                for (Index iza=0; iza<nza_se/2+1; iza++)
                  for (Index sza=0; sza<nza_se; sza++)
                    for (Index ist1=0; ist1<stokes_dim; ist1++)
                      for (Index ist2=0; ist2<stokes_dim; ist2++)
                        pha_mat_interp(sza,iza,ist1,ist2) = interp(itw_aa,
                          ssd.pha_mat_data(0,i_pfct,sza,Range(joker),iza,0,ist1*4+ist2),
                          daa_gp);

            // in the actual SSP prep for RT4, the extracted azimuthal mode is
            // then interpolated to the RT4 solver polar angles.
            // Here, we're interpolating to the fixed scattered polar angle
            // and the hardcoded incidence angle grid. Need to do this for each
            // (delta) azimuth angle.
                for (Index iza=0; iza<n_za; iza++)
                {
                  GridPos za_sca_gp;
                  GridPos za_inc_gp;
                  Vector itw_za(4);
                  Numeric za_sca = sza_grid[0]; 
                  Numeric za_inc = pha_mat_za[iza]; 

                  if (za_inc>90)
                    {
                      gridpos(za_inc_gp,this_za_datagrid,180-za_inc);
                      gridpos(za_sca_gp,za_datagrid,180-za_sca);
                    }
                  else
                    {
                      gridpos(za_inc_gp,this_za_datagrid,za_inc);
                      gridpos(za_sca_gp,za_datagrid,za_sca);
                    }

                  interpweights(itw_za,za_sca_gp,za_inc_gp);
      
                  for (Index ist1=0; ist1<stokes_dim; ist1++)
                    for (Index ist2=0; ist2<stokes_dim; ist2++)
                      pha_mat_rt4(iza,iaa,ist1,ist2) = interp(itw_za,
                        pha_mat_interp(Range(joker),Range(joker),ist1,ist2),
                                       za_sca_gp,za_inc_gp);
                }
              }
          }

        else
          {
            ostringstream os;
            os << "Unsuitable particle type encountered.";
            throw runtime_error( os.str() );
          }
      }
      i_se_flat++;
    }

  if (printinfo)
    {
      cout << "----- RT4 -----" << endl;
      cout << "absorption vector:\n" << abs_vec(0,joker) << endl;
      cout << "extinction matrix:\n" << ext_mat(0,joker,joker) << endl;
      cout << "phase matrix (" << za_printinfo_index
           << "," << aa_printinfo_index << "):\n"
           << pha_mat_rt4(za_printinfo_index,aa_printinfo_index,joker,joker)
           << endl;
      cout << endl;
    }

  if (printinfo)
    {
      Vector dabs_vec(stokes_dim, -999.);
      for (Index ist=0; ist<stokes_dim; ist++)
        dabs_vec[ist] = abs_vec(0,ist)-abs_vec_mc[ist];
      Matrix dext_mat(stokes_dim,stokes_dim, -999.);
      for (Index ist1=0; ist1<stokes_dim; ist1++)
        for (Index ist2=0; ist2<stokes_dim; ist2++)
          dext_mat(ist1,ist2) = ext_mat(0,ist1,ist2)-ext_mat_mc(ist1,ist2);
      Matrix dpha_mat(stokes_dim,stokes_dim, -999.);
      for (Index ist1=0; ist1<stokes_dim; ist1++)
        for (Index ist2=0; ist2<stokes_dim; ist2++)
          dpha_mat(ist1,ist2) = 
              pha_mat_rt4(za_printinfo_index,aa_printinfo_index,ist1,ist2)
            - pha_mat_mc(za_printinfo_index,aa_printinfo_index,ist1,ist2);

      cout << "----- differences (RT4-MC) -----" << endl;
      cout << "absorption vector:\n" << dabs_vec << endl;
      cout << "extinction matrix:\n" << dext_mat << endl;
      cout << "phase matrix (" << za_printinfo_index
           << "," << aa_printinfo_index << "):\n"
           << dpha_mat << endl;
      cout << endl;
    }
  cout << "========== END ==========" << endl;
  cout << endl;


  ////// Compare //////
  //
  if (compare)
    {
      Numeric dmax;
      dmax = 0.5e-6*(abs_vec(0,0)+abs_vec_mc[0]);
      Compare(abs_vec(0,joker), abs_vec_mc, dmax, "Deviation in abs_vec",
              "RT4", "MC", "", "", verbosity);
      dmax = 0.5e-6*(ext_mat(0,0,0)+ext_mat_mc(0,0));
      Compare(ext_mat(0,joker,joker), ext_mat_mc, dmax, "Deviation in ext_mat",
              "RT4", "MC", "", "", verbosity);
      for (Index iza=0; iza<n_za; iza++)
        for (Index iaa=0; iaa<n_aa; iaa++)
        {
          ostringstream os;
          os << "Deviation in pha_mat at za[" << iza << "]=" << pha_mat_za[iza]
             << "deg and aa[" << iaa << "]=" << pha_mat_aa[iaa] << "deg.";
          dmax = 0.5e-6*(pha_mat_rt4(iza,iaa,0,0)+pha_mat_mc(iza,iaa,0,0));
          Compare(pha_mat_rt4(iza,iaa,joker,joker), pha_mat_mc(iza,iaa,joker,joker),
                  dmax, os.str(), "RT4", "MC", "", "", verbosity);
        }
    }

}
