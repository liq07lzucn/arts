/* Copyright (C) 2003 Mattias Ekstr�m <ekstrom@rss.chalmers.se>

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
  ===  File description
  ===========================================================================*/

/*!
  \file   sensor.h
  \author Mattias Ekstr�m <ekstrom@rss.chalmers.se>
  \date   2003-02-28

  \brief  Sensor modelling functions.

   This file contains the definition of the functions in sensor.cc
   that are of interest elsewhere.
*/


#ifndef sensor_h
#define sensor_h

#include "arts.h"
#include "matpackI.h"
#include "matpackII.h"
#include "interpolation.h"
//#include "agenda_class.h"
//#include "array.h"
#include "math_funcs.h"
//#include "mystring.h"


/*===========================================================================
  === Functions from sensor.cc
  ===========================================================================*/

void antenna_matrix(
                      Sparse&   H,
              ConstVectorView   m_za,
  const ArrayOfArrayOfMatrix&   diag,
              ConstVectorView   x_f,
              ConstVectorView   ant_za,
                 const Index&   n_pol,
                 const Index&   do_norm );

/*
void merge_grids(
              Vector&   tot,
      ConstVectorView   ref,
      ConstVectorView   rel );
*/

void mixer_matrix(
              Sparse&   H,
              Vector&   f_mixer,
      ConstVectorView   f_grid,
        const Numeric   lo,
      ConstMatrixView   filter,
          const Index   n_pol,
          const Index   n_za,
          const Index   do_norm );

void multi_mixer_matrix(
     Sparse&                H,
     ConstVectorView        f_mono,
     ConstVectorView        f_ch,
     ConstVectorView        lo,
     ConstMatrixView        sb_filter,
     const ArrayOfMatrix&   ch_resp,
     const Index&           n_za,
     const Index&           n_aa,
     const Index&           n_pol,
     const Index&           do_norm);

void polarisation_matrix(
              Sparse&   H,
      ConstMatrixView   pol,
          const Index   n_f,
          const Index   n_za,
          const Index   dim );

void rotation_matrix(
              Sparse&   H,
      ConstVectorView   rot,
          const Index   n_f,
          const Index   dim );

void scale_antenna_diagram(
           VectorView   sc,
      ConstMatrixView   srm,
       const Numeric&   f_ref,
       const Numeric&   f_new );

void sensor_integration_vector(
           VectorView   h,
      ConstVectorView   f,
      ConstVectorView   x_ftot,
      ConstVectorView   x_g );

void sensor_summation_vector(
           VectorView   h,
        const Numeric   f,
      ConstVectorView   f_grid,
        const Numeric   lo,
      ConstMatrixView   sfrm );

void spectrometer_matrix(
              Sparse&   H,
 const ArrayOfMatrix&   ch_response,
      ConstVectorView   ch_f,
      ConstVectorView   sensor_f,
         const Index&   n_za,
         const Index&   n_pol,
         const Index&   do_norm );

#endif  // sensor_h