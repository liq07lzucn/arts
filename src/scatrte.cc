/* Copyright (C) 2002,2003 Claudia Emde <claudia@sat.physik.uni-bremen.de>
                      
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
   USA. 
*/
  
/*!
  \file   scatrte.cc
  \author Claudia Emde <claudia@sat.physik.uni-bremen.de>
  \date   Wed Jun 04 11:03:57 2003
  
  \brief  This file contains functions to calculate the radiative transfer
  inside the cloudbox.
  
*/



/*===========================================================================
  === External declarations
  ===========================================================================*/

#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include "array.h"
#include "auto_md.h"
#include "matpackVII.h"
#include "ppath.h"
#include "agenda_class.h"
#include "physics_funcs.h"
#include "lin_alg.h"
#include "math_funcs.h"
#include "messages.h"
#include "xml_io.h"
#include "rte.h"
#include "special_interp.h"
#include "scatrte.h"
#include "logic.h"




//! Calculation of scattering properties in the cloudbox.
/*! 
  Calculate ext_mat, abs_vec for all points inside the cloudbox for one 
  propagation direction.
  sca_vec can be obtained from the workspace variable scat_field.
  As we need the average for each layer, it makes sense to calculte
  the coefficients once and store them in an array instead of 
  calculating at each point the coefficient of the point above and 
  the point below. 

  \param ext_mat_field extinction matrix field
  \param abs_vec_field absorption vector field
  \param scat_p_index pressure index in cloudbox
  \param scat_lat_index latitude index in cloudbox
  \param scat_lon_index longitude index in cloudbox
  \param scat_za_index Index for propagation direction
  \param spt_calc_agenda Agenda for calculation of single scattering properties
  \param opt_prop_part_agenda Agenda for summing over all hydrometeor species
  \param cloudbox_limits Cloudbox limits.

  \author Claudia Emde
  \date 2002-06-03
*/
void cloud_fieldsCalc(// Output:
                        Tensor5View ext_mat_field,
                        Tensor4View abs_vec_field,
                        // Communication variables for *opt_prop_part_agenda*
                        Index& scat_p_index,
                        Index& scat_lat_index,
                        Index& scat_lon_index, 
                        Tensor3& ext_mat,
                        Matrix& abs_vec,  
                        // Input:
                        const Index& scat_za_index,
                        const Index& scat_aa_index,
                        const Agenda& spt_calc_agenda,
                        const Agenda& opt_prop_part_agenda,
                        const ArrayOfIndex& cloudbox_limits
                        )
{
  
  const Index atmosphere_dim = cloudbox_limits.nelem()/2;
  const Index stokes_dim = ext_mat_field.ncols();
  
  assert( atmosphere_dim == 1 || atmosphere_dim ==3 );
  
  assert(stokes_dim == ext_mat_field.nrows() &&
         stokes_dim == abs_vec_field.ncols());
  

  const Index p_low = cloudbox_limits[0];
  const Index p_up = cloudbox_limits[1];

  // If atmosohere_dim==1
  Index lat_low = 0;
  Index lat_up = 0;
  Index lon_low = 0;
  Index lon_up = 0;
  

  if (atmosphere_dim == 3)
    {
      lat_low = cloudbox_limits[2]; 
      lat_up = cloudbox_limits[3]; 
      lon_low = cloudbox_limits[4]; 
      lon_up = cloudbox_limits[5]; 
    }

  //Calculate optical properties for single particle types:
  spt_calc_agenda.execute(scat_za_index || scat_aa_index);
  
  // Calculate ext_mat, abs_vec for all points inside the cloudbox.
  // sca_vec can be obtained from the workspace variable scat_field.
  // As we need the average for each layer, it makes sense to calculte
  // the coefficients once and store them in an array instead of 
  // calculating at each point the coefficient of the point above and 
  // the point below. 
  // To use special interpolation functions for atmospheric fields we 
  // use ext_mat_field and abs_vec_field:               

  // Loop over all positions inside the cloudbox defined by the 
  // cloudbox_limits.
  for(scat_p_index = p_low; scat_p_index <= p_up; scat_p_index ++)
    {
      for(scat_lat_index = lat_low; scat_lat_index <= lat_up; scat_lat_index ++)
        {
          for(scat_lon_index = lon_low; scat_lon_index <= lon_up;
              scat_lon_index ++)
            {
              // Execute agendas silently, only the first call is
              // output on the screen (no other reason for argument 
              // in agenda.execute).
              opt_prop_part_agenda.execute(scat_za_index ||
                                            scat_aa_index ||
                                            scat_p_index-p_low ||
                                            scat_lat_index- lat_low||
                                            scat_lon_index- lon_low);
           
              // Store coefficients in arrays for the whole cloudbox.
              abs_vec_field(scat_p_index - p_low, 
                            scat_lat_index - lat_low,
                            scat_lon_index- lon_low,
                            joker) = 
                abs_vec(0, joker);
              
              ext_mat_field(scat_p_index - p_low, 
                            scat_lat_index - lat_low,
                            scat_lon_index - lon_low,
                            joker, joker) = 
                ext_mat(0, joker, joker);
            } 
        }
    }
}
  




//! Radiative transfer calculation along a path inside the cloudbox (1D).
/*! 
  This function calculates the radiation field along a propagation path 
  step for a specified zenith direction. This function is used for the 
  sequential update if the radiation field and called inside a loop over
  the pressure grid. 
  In the function the intersection point of the propagation path with the 
  next layer is calculated and all atmospheric properties are 
  interpolated an the intersection point. Then a radiative transfer step is 
  performed using the stokes vector as output and input. The inermediate
  Stokes vectors are stored in the WSV i_field.

 WS Output:
  \param i_field Updated radiation field inside the cloudbox. 
  Variables used in scalar_gas_abs_agenda:
  \param abs_scalar_gas
  \param a_pressure
  \param a_temperature
  \param a_vmr_list
  Variables used in opt_prop_xxx_agenda:
  \param ext_mat
  \param abs_vec  
  Variables used in ppath_step_agenda:
  \param ppath_step
  WS Input:
  \param p_index // Pressure index
  \param scat_za_index // Index for proagation direction
  \param scat_za_grid
  \param cloudbox_limits 
  \param scat_field Scattered field.
  Calculate scalar gas absorption:
  \param scalar_gas_absorption_agenda
  \param vmr_field
  Scalar gas absorption:
  \param opt_prop_gas_agenda
  Propagation path calculation:
  \param ppath_step_agenda
  \param p_grid
  \param z_field
  \param r_geoid
  Calculate thermal emission:
  \param t_field
  \param f_grid
  \param f_index

  \author Claudia Emde
  \date 2002-06-04
*/
void cloud_ppath_update1D(
                  Tensor6View i_field,
                  VectorView stokes_vec,
                   // scalar_gas_abs_agenda:
                  Numeric& a_pressure,
                  Numeric& a_temperature,
                  Vector& a_vmr_list,
                  // opt_prop_xxx_agenda:
                  Tensor3& ext_mat,
                  Matrix& abs_vec,  
                  // ppath_step_agenda:
                  Ppath& ppath_step, 
                  const Index& p_index,
                  const Index& scat_za_index,
                  ConstVectorView scat_za_grid,
                  const ArrayOfIndex& cloudbox_limits,
                  ConstTensor6View scat_field,
                  // Calculate scalar gas absorption:
                  const Agenda& scalar_gas_absorption_agenda,
                  ConstTensor4View vmr_field,
                  // Gas absorption:
                  const Agenda& opt_prop_gas_agenda,
                  // Propagation path calculation:
                  const Agenda& ppath_step_agenda,
                  ConstVectorView p_grid,
                  ConstTensor3View z_field,
                  ConstMatrixView r_geoid,
                  // Calculate thermal emission:
                  ConstTensor3View t_field,
                  ConstVectorView f_grid,
                  const Index& f_index,
                  //particle opticla properties
                  ConstTensor5View ext_mat_field,
                  ConstTensor4View abs_vec_field
                  )
{

  const Index atmosphere_dim = 1;
  const Index stokes_dim = stokes_vec.nelem();

  // Grid ranges inside cloudbox:
  const Range p_range = Range(cloudbox_limits[0],
                              (cloudbox_limits[1] - cloudbox_limits[0]+1) );
  
  Vector sca_vec_av(stokes_dim,0); 

  //Initialize ppath for 1D.
  ppath_init_structure(ppath_step, 1, 1);
  // See documentation of ppath_init_structure for understanding
  // the parameters.
              
  // Assign value to ppath.pos:
  ppath_step.z[0]     = z_field(p_index,0,0);
  ppath_step.pos(0,0) = r_geoid(0,0) + ppath_step.z[0];
              
  // Define the direction:
  ppath_step.los(0,0) = scat_za_grid[scat_za_index];
              
  // Define the grid positions:
  ppath_step.gp_p[0].idx   = p_index;
  ppath_step.gp_p[0].fd[0] = 0;
  ppath_step.gp_p[0].fd[1] = 1;
              
  // Call ppath_step_agenda: 
  ppath_step_agenda.execute((scat_za_index + 
                             (p_index - cloudbox_limits[0])));
               
  // Check whether the next point is inside or outside the
  // cloudbox. Only if the next point lies inside the
  // cloudbox a radiative transfer step caclulation has to
  // be performed.
  if ((cloudbox_limits[0] <= ppath_step.gp_p[1].idx) &&
      cloudbox_limits[1] > ppath_step.gp_p[1].idx ||
      (cloudbox_limits[1] == ppath_step.gp_p[1].idx &&
       fabs(ppath_step.gp_p[1].fd[0]) < 1e-6)) 
    {
                  
      // If the intersection points lies exactly on a 
      // upper boundary the gridposition index is 
      // reduced by one and the first interpolation weight 
      // is set to 1.
                        
      for( Index i = 0; i<2; i++)
        { 
          if (cloudbox_limits[1] == ppath_step.gp_p[i].idx &&
              fabs(ppath_step.gp_p[i].fd[0]) < 1e-6)
            {
              ppath_step.gp_p[i].idx -= 1;
              ppath_step.gp_p[i].fd[0] = 1;
              ppath_step.gp_p[i].fd[1] = 0;
            }
        }
                  
                  
                  
      // Check if the agenda has returned ppath.step with 
      // reasonable values. 
      // PpathPrint( ppath_step, "ppath");
                  
      // Gridpositions inside the cloudbox.
      // The optical properties are stored only inside the
      // cloudbox. For interpolation we use grids
      // inside the cloudbox.
      ArrayOfGridPos cloud_gp_p = ppath_step.gp_p;
      ArrayOfGridPos dummy_gp;
      Vector dummy_grid(0);
                  
                                    
      for(Index i = 0; i < ppath_step.np; i++ )
        cloud_gp_p[i].idx -= cloudbox_limits[0];
                  
      Matrix itw_field;
                  
      interp_atmfield_gp2itw(
                             itw_field, atmosphere_dim,
                             p_grid[ Range(p_range)], dummy_grid,
                             dummy_grid,
                             cloud_gp_p, dummy_gp, dummy_gp);
                  
      // Ppath_step has 2 points, the starting
      // point and the intersection point.
      // But there can be points in between, when a maximum 
      // l_step is given. We have to interpolate on all the 
      // points in the ppath_step.
                  
      Tensor3 ext_mat_int(stokes_dim, stokes_dim, ppath_step.np);
      Matrix abs_vec_int(stokes_dim, ppath_step.np);
      Matrix sca_vec_int(stokes_dim, ppath_step.np);
      Vector t_int(ppath_step.np);
      Vector vmr_int(ppath_step.np);
      Vector p_int(ppath_step.np);
                  
                  
      // Calculate the average of the coefficients for the layers
      // to be considered in the 
      // radiative transfer calculation.
                  
                  
      for (Index i = 0; i < stokes_dim; i++)
        {
                      
          // Extinction matrix requires a second loop 
          // over stokes_dim
          out3 << "Interpolate ext_mat:\n";
          for (Index j = 0; j < stokes_dim; j++)
            {
              //
              // Interpolation of ext_mat
              //
              interp_atmfield_by_itw
                (ext_mat_int(i, j, joker),
                 atmosphere_dim,
                 p_grid[p_range], dummy_grid, dummy_grid,
                 ext_mat_field(joker, joker, joker, i, j),
                 "ext_mat_array",
                 cloud_gp_p, dummy_gp, dummy_gp,
                 itw_field);
            }
          // Particle absorption vector:
          //
          // Interpolation of abs_vec
          //  //
          out3 << "Interpolate abs_vec:\n";
          interp_atmfield_by_itw
            (abs_vec_int(i,joker),
             atmosphere_dim,
             p_grid[p_range], dummy_grid, dummy_grid, 
             abs_vec_field(joker, joker, joker, i),
             "abs_vec_array",
             cloud_gp_p, dummy_gp, dummy_gp,
             itw_field);
          //
          // Scattered field:
          //
          // Interpolation of sca_vec:
          //
          out3 << "Interpolate scat_field:\n";
          interp_atmfield_by_itw
            (sca_vec_int(i, joker),
             atmosphere_dim,
             p_grid[p_range], dummy_grid, dummy_grid,
             scat_field(joker, joker, joker, scat_za_index,
                        0, i),
             "scat_field",
             cloud_gp_p,
             dummy_gp, dummy_gp,
             itw_field);
        }
                    
      //
      // Planck function
      // 
      // Interpolate temperature field
      //
      out3 << "Interpolate temperature field\n";
      interp_atmfield_by_itw
        (t_int,
         atmosphere_dim,
         p_grid, dummy_grid, dummy_grid,
         t_field(joker, joker, joker),
         "t_field",
         ppath_step.gp_p,
         dummy_gp, dummy_gp,
         itw_field);

      // 
      // The vmr_field is needed for the gaseous absorption 
      // calculation.
      //
      const Index N_species = vmr_field.nbooks();
      //
      // Interpolated vmr_list, holds a vmr_list for each point in 
      // ppath_step.
      //
      Matrix vmr_list_int(N_species, ppath_step.np);

      for (Index i = 0; i < N_species; i++)
        {
          out3 << "Interpolate vmr field\n";
          interp_atmfield_by_itw
            (vmr_int,
             atmosphere_dim,
             p_grid, dummy_grid, dummy_grid,
             vmr_field(i, joker, joker, joker),
             "vmr_field",
             ppath_step.gp_p,
             dummy_gp, dummy_gp,
             itw_field);
                  
          vmr_list_int(i, joker) = vmr_int;
        }
              
      // 
      // Interpolate pressure
      //
      itw2p( p_int, p_grid, ppath_step.gp_p, itw_field); 
              
      // Radiative transfer from one layer to the next, starting
      // at the intersection with the next layer and propagating
      // to the considered point.
              
      for( Index k= ppath_step.np-1; k > 0; k--)
        {
                  
          // Length of the path between the two layers.
          Numeric l_step = ppath_step.l_step[k-1];
          // Average temperature
          a_temperature =   0.5 * (t_int[k] + t_int[k-1]);
          //
          // Average pressure
          a_pressure = 0.5 * (p_int[k] + p_int[k-1]);
          //
          // Average vmrs
          for (Index i = 0; i < N_species; i++)
            a_vmr_list[i] = 0.5 * (vmr_list_int(i,k) + 
                                   vmr_list_int(i,k-1));
          //
          // Calculate scalar gas absorption and add it to abs_vec 
          // and ext_mat.
          //
		    
          scalar_gas_absorption_agenda.execute(p_index);
		      
          opt_prop_gas_agenda.execute(p_index);
          //
          // Add average particle extinction to ext_mat. 
          //
          for (Index i = 0; i < stokes_dim; i++)
            {
              for (Index j = 0; j < stokes_dim; j++)
                {
                  ext_mat(0,i,j) += 0.5 *
                    (ext_mat_int(i,j,k) + ext_mat_int(i,j,k-1));
			  
                }
              //
              // Add average particle absorption to abs_vec.
              //
              abs_vec(0,i) += 0.5 * 
                (abs_vec_int(i,k) + abs_vec_int(i,k-1));
			  
              //
              // Averaging of sca_vec:
              //
              sca_vec_av[i] =  0.5*
                (sca_vec_int(i,k) + sca_vec_int(i,k-1));
              // 
            }
                  
                  
          // Frequency
          Numeric f = f_grid[f_index];
          //
          // Calculate Planck function
          //
          Numeric a_planck_value = planck(f, a_temperature);
                  
          // Some messages:
          out3 << "-----------------------------------------\n";
          out3 << "Input for radiative transfer step \n"
               << "calculation inside"
               << " the cloudbox:" << "\n";
          out3 << "Stokes vector at intersection point: \n" 
               << stokes_vec 
               << "\n"; 
          out3 << "l_step: ..." << l_step << "\n";
          out3 << "------------------------------------------\n";
          out3 << "Averaged coefficients: \n";
          out3 << "Planck function: " << a_planck_value << "\n";
          out3 << "Scattering vector: " << sca_vec_av << "\n"; 
          out3 << "Absorption vector: " << abs_vec(0,joker) << "\n"; 
          out3 << "Extinction matrix: " << ext_mat(0,joker,joker) << "\n"; 
                      
                  
          assert (!is_singular( ext_mat(0,joker,joker)));
                  
          // Radiative transfer step calculation. The Stokes vector
          // is updated until the considered point is reached.
          rte_step(stokes_vec, ext_mat(0,joker,joker), 
                   abs_vec(0,joker), 
                   sca_vec_av, l_step, a_planck_value);
                  
        }// End of loop over ppath_step. 
      // Assign calculated Stokes Vector to i_field. 
      i_field(p_index - cloudbox_limits[0],
              0, 0,
              scat_za_index, 0,
              joker) = stokes_vec;
                   

    } //end if inside cloudbox
  // 
  // If the intersection point is outside the cloudbox
  // no radiative transfer step is performed.
  // The value on the cloudbox boundary remains unchanged.
  //
              
}
          
 


//! Radiative transfer calculation along a path inside the cloudbox (1D).
/*! 
  This function calculates the radiation field along a propagation path 
  step for a specified zenith direction. This function is used for the 
  sequential update if the radiation field and called inside a loop over
  the pressure grid. 
  In the function the intersection point of the propagation path with the 
  next layer is calculated and all atmospheric properties are 
  interpolated an the intersection point. Then a radiative transfer step is 
  performed using the stokes vector as output and input. The inermediate
  Stokes vectors are stored in the WSV i_field.

 WS Output:
  \param i_field Updated radiation field inside the cloudbox. 
  Variables used in scalar_gas_abs_agenda:
  \param abs_scalar_gas
  \param a_pressure
  \param a_temperature
  \param a_vmr_list
  Variables used in opt_prop_xxx_agenda:
  \param ext_mat
  \param abs_vec  
  Variables used in ppath_step_agenda:
  \param ppath_step
  WS Input:
  \param p_index // Pressure index
  \param scat_za_index // Index for proagation direction
  \param scat_za_grid
  \param cloudbox_limits 
  \param scat_field Scattered field.
  Calculate scalar gas absorption:
  \param scalar_gas_absorption_agenda
  \param vmr_field
  Scalar gas absorption:
  \param opt_prop_gas_agenda
  Propagation path calculation:
  \param ppath_step_agenda
  \param p_grid
  \param z_field
  \param r_geoid
  Calculate thermal emission:
  \param t_field
  \param f_grid
  \param f_index

  \author Claudia Emde
  \date 2002-06-04
*/
void cloud_ppath_update3D(
                  Tensor6View i_field,
                  VectorView stokes_vec,
                   // scalar_gas_abs_agenda:
                  Numeric& a_pressure,
                  Numeric& a_temperature,
                  Vector& a_vmr_list,
                  // opt_prop_xxx_agenda:
                  Tensor3& ext_mat,
                  Matrix& abs_vec,  
                  // ppath_step_agenda:
                  Ppath& ppath_step, 
                  const Index& p_index,
                  const Index& lat_index,
                  const Index& lon_index,
                  const Index& scat_za_index,
                  const Index& scat_aa_index,
                  ConstVectorView scat_za_grid,
                  ConstVectorView scat_aa_grid,
                  const ArrayOfIndex& cloudbox_limits,
                  ConstTensor6View scat_field,
                  // Calculate scalar gas absorption:
                  const Agenda& scalar_gas_absorption_agenda,
                  ConstTensor4View vmr_field,
                  // Gas absorption:
                  const Agenda& opt_prop_gas_agenda,
                  // Propagation path calculation:
                  const Agenda& ppath_step_agenda,
                  ConstVectorView p_grid,
                  ConstVectorView lat_grid,
                  ConstVectorView lon_grid,
                  ConstTensor3View z_field,
                  ConstMatrixView r_geoid,
                  // Calculate thermal emission:
                  ConstTensor3View t_field,
                  ConstVectorView f_grid,
                  const Index& f_index,
                  //particle opticla properties
                  ConstTensor5View ext_mat_field,
                  ConstTensor4View abs_vec_field
                  )
{

  
  const Index atmosphere_dim = 3;
  const Index stokes_dim = stokes_vec.nelem();

  assert( is_size( i_field, 
                   (cloudbox_limits[1] - cloudbox_limits[0]) + 1,
                   (cloudbox_limits[3] - cloudbox_limits[2]) + 1, 
                   (cloudbox_limits[5] - cloudbox_limits[4]) + 1,
                   scat_za_grid.nelem(), 
                   scat_aa_grid.nelem(),
                   stokes_dim));
  
  assert( is_size( scat_field, 
                   (cloudbox_limits[1] - cloudbox_limits[0]) + 1,
                   (cloudbox_limits[3] - cloudbox_limits[2]) + 1, 
                   (cloudbox_limits[5] - cloudbox_limits[4]) + 1,
                   scat_za_grid.nelem(), 
                   scat_aa_grid.nelem(),
                   stokes_dim));

  assert( stokes_vec.nelem() == stokes_dim);

  // Grid ranges inside cloudbox:
  const Range p_range = Range(cloudbox_limits[0],
                              (cloudbox_limits[1] - cloudbox_limits[0]+1) );
  const Range lat_range = Range(cloudbox_limits[2],
                                (cloudbox_limits[3] - cloudbox_limits[2]+1) );
  const Range lon_range = Range(cloudbox_limits[4],
                                (cloudbox_limits[5] - cloudbox_limits[4]+1) );

  // The definition of the azimth angle grids is different for clearsky and
  // cloudbox. (SHOULD BE FIXED!!!!)
  Vector aa_grid(scat_aa_grid.nelem());
  for(Index i = 0; i<scat_aa_grid.nelem(); i++)
    aa_grid[i] = scat_aa_grid[i] - 180;
  
  Vector sca_vec_av(stokes_dim,0); 

  //Initialize ppath for 3D.
  ppath_init_structure(ppath_step, 3, 1);
  // See documentation of ppath_init_structure for
  // understanding the parameters.
              
  // Assign value to ppath.pos:
                    
  ppath_step.z[0] = z_field(p_index,lat_index,
                            lon_index);

  // The first dimension of pos are the points in 
  // the propagation path. 
  // Here we initialize the first point.
  // The second is: radius, latitude, longitude

  ppath_step.pos(0,0) =
    r_geoid(lat_index, lon_index) + ppath_step.z[0];
  ppath_step.pos(0,1) = lat_grid[lat_index];
  ppath_step.pos(0,2) = lon_grid[lon_index];
              
  // Define the direction:
  ppath_step.los(0,0) = scat_za_grid[scat_za_index];
  ppath_step.los(0,1) = aa_grid[scat_aa_index];
              
  // Define the grid positions:
  ppath_step.gp_p[0].idx   = p_index;
  ppath_step.gp_p[0].fd[0] = 0.;
  ppath_step.gp_p[0].fd[1] = 1.;

  ppath_step.gp_lat[0].idx   = lat_index;
  ppath_step.gp_lat[0].fd[0] = 0.;
  ppath_step.gp_lat[0].fd[1] = 1.;
                    
  ppath_step.gp_lon[0].idx   = lon_index;
  ppath_step.gp_lon[0].fd[0] = 0.;
  ppath_step.gp_lon[0].fd[1] = 1.;
              
  // Call ppath_step_agenda: 
  ppath_step_agenda.execute(scat_za_index &&
                            scat_aa_index &&
                            p_index - cloudbox_limits[0] &&
                            lat_index - cloudbox_limits[2]&&
                            lon_index - cloudbox_limits[4]);
              
                    
  // Check if the agenda has returned ppath.step with 
  // reasonable values. 
  // PpathPrint( ppath_step, "ppath");

  const Numeric TOL = 1e-6;  
  
  // Check whether the next point is inside or outside the
  // cloudbox. Only if the next point lies inside the
  // cloudbox a radiative transfer step caclulation has to
  // be performed.
  // Tolerance value for checking if a point is exactly on
  // a grid point.
  if (
      // inside pressure boundaries
      (cloudbox_limits[0] <= ppath_step.gp_p[1].idx) &&
      (cloudbox_limits[1] > ppath_step.gp_p[1].idx ||
       (cloudbox_limits[1] == ppath_step.gp_p[1].idx &&
        fabs(ppath_step.gp_p[1].fd[0]) < TOL)) &&
      // inside latitude boundaries 
      (cloudbox_limits[2] <= ppath_step.gp_lat[1].idx) &&
      (cloudbox_limits[3] > ppath_step.gp_lat[1].idx ||
       (cloudbox_limits[3] == ppath_step.gp_lat[1].idx &&
        fabs(ppath_step.gp_lat[1].fd[0]) < TOL)) &&
      // inside longitude boundaries 
      (cloudbox_limits[4] <= ppath_step.gp_lon[1].idx) &&
      (cloudbox_limits[5] > ppath_step.gp_lon[1].idx ||
       (cloudbox_limits[5] == ppath_step.gp_lon[1].idx &&
        fabs(ppath_step.gp_lon[1].fd[0]) < TOL )) 
      )
    {
      // If the intersection points lies exactly on a 
      // upper boundary the gridposition index is 
      // reduced by one and the first interpolation weight 
      // is set to 1.
                        
      for( Index i = 0; i<2; i++)
        { 
          if (cloudbox_limits[1] == ppath_step.gp_p[i].idx &&
              fabs(ppath_step.gp_p[i].fd[0]) < TOL)
            {
              ppath_step.gp_p[i].idx -= 1;
              ppath_step.gp_p[i].fd[0] = 1;
              ppath_step.gp_p[i].fd[1] = 0;
            }
                            
          if (cloudbox_limits[3]==ppath_step.gp_lat[i].idx &&
              fabs(ppath_step.gp_lat[i].fd[0]) < TOL)
            {
              ppath_step.gp_lat[i].idx -= 1;
              ppath_step.gp_lat[i].fd[0] = 1;
              ppath_step.gp_lat[i].fd[1] = 0;
            }
                            
          if (cloudbox_limits[5]==ppath_step.gp_lon[i].idx &&
              fabs(ppath_step.gp_lon[i].fd[0]) < TOL)
            {
              ppath_step.gp_lon[i].idx -= 1;
              ppath_step.gp_lon[i].fd[0] = 1;
              ppath_step.gp_lon[i].fd[1] = 0;
            }
        }
                        
      // Gridpositions inside the cloudbox.
      // The optical properties are stored only inside the
      // cloudbox. For interpolation we use grids
      // inside the cloudbox.
                        
      ArrayOfGridPos cloud_gp_p = ppath_step.gp_p;
      ArrayOfGridPos cloud_gp_lat = ppath_step.gp_lat;
      ArrayOfGridPos cloud_gp_lon = ppath_step.gp_lon;
                        
      for(Index i = 0; i<2; i++ )
        {
          cloud_gp_p[i].idx -= cloudbox_limits[0];  
          cloud_gp_lat[i].idx -= cloudbox_limits[2];
          cloud_gp_lon[i].idx -= cloudbox_limits[4];
        }

      Matrix itw_field;
                        
      interp_atmfield_gp2itw
        (itw_field, atmosphere_dim,
         p_grid[ Range( cloudbox_limits[0], 
                        (cloudbox_limits[1]-
                         cloudbox_limits[0]+1))],
         lat_grid[ Range( cloudbox_limits[2], 
                          (cloudbox_limits[3]-
                           cloudbox_limits[2]+1))],
         lon_grid[ Range( cloudbox_limits[4], 
                          (cloudbox_limits[5]-
                           cloudbox_limits[4]+1))],
         cloud_gp_p, cloud_gp_lat, cloud_gp_lon );

      // Ppath_step always has 2 points, the starting
      // point and the intersection point. If more points are included 
      // it can become larger.
      Tensor3 ext_mat_int(stokes_dim, stokes_dim, 
                          ppath_step.np);
      Matrix abs_vec_int(stokes_dim, ppath_step.np);
      Matrix sca_vec_int(stokes_dim, ppath_step.np);
      Matrix sto_vec_int(stokes_dim, ppath_step.np);
      Vector t_int(ppath_step.np);
      Vector vmr_int(ppath_step.np);
      Vector p_int(ppath_step.np);

      // Interpolate ext_mat, abs_vec and sca_vec on the
      // intersection point.
                        
      // Calculate the average of the coefficients for the 
      // layers to be considered in the 
      // radiative transfer calculation.
                        
      for (Index i = 0; i < stokes_dim; i++)
        {
                            
          // Extinction matrix requires a second loop 
          // over stokes_dim
          out3 << "Interpolate ext_mat:\n";
          for (Index j = 0; j < stokes_dim; j++)
            {
              // Interpolation of ext_mat
              //
              interp_atmfield_by_itw
                (ext_mat_int(i, j, joker),
                 atmosphere_dim,
                 p_grid[p_range],
                 lat_grid[lat_range],
                 lon_grid[lon_range],
                 ext_mat_field( joker, joker, joker, i , j),
                 "ext_mat_field",
                 cloud_gp_p, cloud_gp_lat, cloud_gp_lon,
                 itw_field);
            }
          // Absorption vector:
          //
          // Interpolation of abs_vec
          //
          out3 << "Interpolate abs_vec:\n";
          interp_atmfield_by_itw
            (abs_vec_int(i,joker),
             atmosphere_dim,
             p_grid[p_range],
             lat_grid[lat_range],
             lon_grid[lon_range],
             abs_vec_field( joker, joker, joker, i),
             "abs_vec_field",
             cloud_gp_p, cloud_gp_lat, cloud_gp_lon,
             itw_field);
          //
          // Scattered field:
          //
          // Interpolation of sca_vec:
          //
          out3 << "Interpolate scat_field:\n";
          interp_atmfield_by_itw
            (sca_vec_int(i, joker),
             atmosphere_dim,
             p_grid[p_range], lat_grid[lat_range],
             lon_grid[lon_range],
             scat_field(joker, joker, joker, scat_za_index,
                        scat_aa_index, i),
             "scat_field",
             cloud_gp_p,
             cloud_gp_lat,
             cloud_gp_lon,
             itw_field);
        }
      //
      // Planck function
      // 
      // Interpolate temperature field
      //
      out3 << "Interpolate temperature field\n";
      interp_atmfield_by_itw
        (t_int,
         atmosphere_dim,
         p_grid, lat_grid,
         lon_grid,
         t_field(joker, joker, joker),
         "t_field",
         ppath_step.gp_p,
         ppath_step.gp_lat,
         ppath_step.gp_lon,
         itw_field);

      // 
      // The vmr_field is needed for the gaseous absorption 
      // calculation.
      //
      const Index N_species = vmr_field.nbooks();
      //
      // Interpolated vmr_list, holds a vmr_list for
      //each point in 
      // ppath_step.
      //
      Matrix vmr_list_int(N_species, ppath_step.np);

      for (Index i = 0; i < N_species; i++)
        {
          out3 << "Interpolate vmr field\n";
          interp_atmfield_by_itw
            (vmr_int,
             atmosphere_dim,
             p_grid, lat_grid, lon_grid,
             vmr_field(i, joker, joker, joker),
             "vmr_field",
             ppath_step.gp_p,
             ppath_step.gp_lat,
             ppath_step.gp_lon,
             itw_field);
                  
          vmr_list_int(i, joker) = vmr_int;
        }
      // 
      // Interpolate pressure, latitude, longitude
      //
      Matrix itw_p(ppath_step.gp_p.nelem(),2);
      interpweights(itw_p, ppath_step.gp_p);

      itw2p( p_int, p_grid, ppath_step.gp_p, itw_p); 
      
      a_vmr_list.resize(N_species);
       
      // Radiative transfer from one layer to the next,
      // starting at the intersection with the next layer 
      // and propagating to the considered point.
      for( Index k= ppath_step.np-1; k > 0; k--)
        {
          // Length of the path between the two layers.
          Numeric l_step = ppath_step.l_step[k-1];
                    
                            
          // Average temperature
          a_temperature =   0.5 * (t_int[k] + t_int[k-1]);
          //
          // Average pressure
          a_pressure = 0.5 * (p_int[k] + p_int[k-1]);
          //
          // Average vmrs
          for (Index i = 0; i < N_species; i++)
            a_vmr_list[i] = 0.5 * (vmr_list_int(i,k) + 
                                   vmr_list_int(i,k-1));
          //
          // Calculate scalar gas absorption and add it to 
          // abs_vec and ext_mat.
          //
          scalar_gas_absorption_agenda.execute(p_index);
          opt_prop_gas_agenda.execute(p_index);
                            
          //
          // Add average particle extinction to ext_mat. 
          //
          for (Index i = 0; i < stokes_dim; i++)
            {
              for (Index j = 0; j < stokes_dim; j++)
                {
                  ext_mat(0,i,j) += 0.5 *
                    (ext_mat_int(i,j,k) +
                     ext_mat_int(i,j,k-1));
                }
              //
              // Add average particle absorption to abs_vec.
              //
              abs_vec(0,i) += 0.5 * 
                (abs_vec_int(i,k) + abs_vec_int(i,k-1));
              //
              // Averaging of sca_vec:
              //
              sca_vec_av[i] =  0.5*
                (sca_vec_int(i,k) + sca_vec_int(i,k-1));
              // 
            }
          // Frequency
          Numeric f = f_grid[f_index];
          //
          // Calculate Planck function
          //
          Numeric a_planck_value = planck(f, a_temperature);
                            
          // Some messages:
          out3 << "-------------------------------------\n";
          out3 << "Input for radiative transfer step \n"
               << "calculation inside"
               << " the cloudbox:" << "\n";
          out3 << "Stokes vector at intersection point: \n" 
               << stokes_vec 
               << "\n"; 
          out3 << "l_step: ..." << l_step << "\n";
          out3 << "--------------------------------------\n";
          out3 << "Averaged coefficients: \n";
          out3 << "Planck function: " 
               << a_planck_value << "\n";
          out3 << "Scattering vector: " 
               << sca_vec_av << "\n"; 
          out3 << "Absorption vector: " 
               << abs_vec(0,joker) << "\n"; 
          out3 << "Extinction matrix: " 
               << ext_mat(0,joker,joker) << "\n"; 
                      
                  
          assert (!is_singular( ext_mat(0,joker,joker)));
                            
          // Radiative transfer step calculation. 
          // The Stokes vector is
          // updated until the considered point is reached.
          rte_step(stokes_vec, ext_mat(0,joker,joker), 
                   abs_vec(0,joker), 
                   sca_vec_av, l_step, a_planck_value);
        }
                        
                        
      // Assign calculated Stokes Vector to i_field. 
      i_field(p_index - cloudbox_limits[0],
              lat_index - cloudbox_limits[2],
              lon_index - cloudbox_limits[4],
              scat_za_index, scat_aa_index,
              joker) = stokes_vec;
      //
    } //end if
  // 
  // If the intersection point is outside the cloudbox
  // no radiative transfer step is performed.
  // The value on the cloudbox boundary remains unchanged.
  //
                    
              
}


/*! Calculated for a given point and a given direction one
  propagation path step.

  This function initializes the ppath structure and 
  executes ppath_step_agenda. Output of the fuinction is 
  a propagation path with two points. The starting point 
  and the next point.

  The function is needed in the sequential update
  (i_fieldUpdateSeq3D).
  
  \param ppath_step Propagation path step
  \param ppath_step_agenda Agenda for calculating propagation paths
  \param p Pressure index 
  \param lat Latitude index
  \param lon Longitude index
  \param z_field Altitude field
  \param r_geoid Geoid
  \param scat_za_grid Zenith angle grid
  \param aa_grid Azimuth angle grid
  \param scat_za_index Zenith angle index
  \param scat_aa_index Azimuth angle index
  \param lat_grid Latitude grid
  \param lon_grid Longitude grid

  \author Claudia Emde
  \date 2002-06-06
*/
void ppath_step_in_cloudbox(//Output:
                            Ppath& ppath_step,
                            //Input:
                            const Agenda& ppath_step_agenda,
                            const Index& p,
                            const Index& lat, 
                            const Index& lon,
                            ConstTensor3View z_field,
                            ConstMatrixView r_geoid,
                            ConstVectorView scat_za_grid,
                            ConstVectorView aa_grid,
                            const Index& scat_za_index,
                            const Index& scat_aa_index,
                            ConstVectorView lat_grid,
                            ConstVectorView lon_grid)
{
  //Initialize ppath for 3D.
  ppath_init_structure(ppath_step, 3, 1);
  // See documentation of ppath_init_structure for
  // understanding the parameters.
    
  // Assign value to ppath.pos:
  //
  ppath_step.z[0] = z_field(p, lat, lon);
                                  
  // The first dimension of pos are the points in 
  // the propagation path. 
  // Here we initialize the first point.
  // The second is: radius, latitude, longitude

  ppath_step.pos(0,0) = r_geoid(lat, lon) + ppath_step.z[0];
  ppath_step.pos(0,1) = lat_grid[lat];
  ppath_step.pos(0,2) = lon_grid[lon];
              
  // Define the direction:
  ppath_step.los(0,0) = scat_za_grid[scat_za_index];
  ppath_step.los(0,1) = aa_grid[scat_aa_index];
              
  // Define the grid positions:
  ppath_step.gp_p[0].idx   = p;
  ppath_step.gp_p[0].fd[0] = 0.;
  ppath_step.gp_p[0].fd[1] = 1.;

  ppath_step.gp_lat[0].idx   = lat;
  ppath_step.gp_lat[0].fd[0] = 0.;
  ppath_step.gp_lat[0].fd[1] = 1.;
                    
  ppath_step.gp_lon[0].idx   = lon;
  ppath_step.gp_lon[0].fd[0] = 0.;
  ppath_step.gp_lon[0].fd[1] = 1.;
              
  // Call ppath_step_agenda: 
  ppath_step_agenda.execute();
}


/*! Checks, whether the secon point of a propagation path 
  is inside the cloudbox.

  This function is needed in the sequential update
  (i_fieldUpdateSeq3D).
  
  \return true is returned if the point is inside the 
          cloudbox.
          
  \param ppath_step Propagation path step.
  \param cloudbox_limits The limits of the cloudbox.
 
  \author Claudia Emde
  \date 2002-06-06
*/
bool is_inside_cloudbox(const Ppath& ppath_step,
                        const ArrayOfIndex& cloudbox_limits)
                        
{
  // Here the numerical accuracy is specified.
  const Numeric TOL = 1e-2;

  const Index p_low = cloudbox_limits[0];
  const Index p_up = cloudbox_limits[1];
  const Index lat_low = cloudbox_limits[2]; 
  const Index lat_up = cloudbox_limits[3]; 
  const Index lon_low = cloudbox_limits[4]; 
  const Index lon_up = cloudbox_limits[5]; 

  if (
      // inside pressure boundaries
      (p_low <= ppath_step.gp_p[1].idx) &&
      (p_up > ppath_step.gp_p[1].idx ||
       (p_up == ppath_step.gp_p[1].idx &&
        fabs(ppath_step.gp_p[1].fd[0]) < TOL)) &&
      // inside latitude boundaries 
      (lat_low <= ppath_step.gp_lat[1].idx) &&
      (lat_up > ppath_step.gp_lat[1].idx ||
       (lat_up == ppath_step.gp_lat[1].idx &&
        fabs(ppath_step.gp_lat[1].fd[0]) < TOL)) &&
      // inside longitude boundaries 
      (lon_low <= ppath_step.gp_lon[1].idx) &&
      (lon_up > ppath_step.gp_lon[1].idx ||
       (lon_up == ppath_step.gp_lon[1].idx &&
        fabs(ppath_step.gp_lon[1].fd[0]) < TOL )) 
      )
    {
      return true;
    }
  return false;
}
