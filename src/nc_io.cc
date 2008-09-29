/* Copyright (C) 2008 Oliver Lemke <olemke@core-dump.info>

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


////////////////////////////////////////////////////////////////////////////
//   File description
////////////////////////////////////////////////////////////////////////////
/*!
  \file   nc_io.cc
  \author Oliver Lemke <olemke@core-dump.info>
  \date   2008-09-12

  \brief This file contains basic functions to handle NetCDF data files.

*/

#include <netcdfcpp.h>
#include "arts.h"
#include "nc_io.h"
#include "nc_io_types.h"
#include "file.h"
#include "messages.h"
#include "exceptions.h"


////////////////////////////////////////////////////////////////////////////
//   Default file name
////////////////////////////////////////////////////////////////////////////

//! Gives the default filename for the NetCDF formats.
/*!
  The default name is only used if the filename is empty.

  \param filename filename
  \param varname variable name
*/
void
filename_nc (      String&  filename,
             const String&  varname )
{
  if ("" == filename)
    {
      extern const String out_basename;
      filename = out_basename + "." + varname + ".nc";
    }
}



//! Gives the default filename, with file index, for the NetCDF formats.
/*!
  The default name is only used if the filename is empty.

  \param[out] filename   filename
  \param[in]  file_index Index appended to the filename
  \param[in]  varname    variable name
*/
void
filename_nc_with_index (
                    String&  filename,
              const Index&   file_index,
              const String&  varname )
{
  Index dummy = file_index;
  dummy += 1;

  if ("" == filename)
    {
      extern const String out_basename;
      ostringstream os;
      os << out_basename << "." << varname << "." << file_index << ".nc";
      filename = os.str();
    }
  else
    {
      ostringstream os;
      os << filename << "." << file_index << ".nc";
      filename = os.str();
    }
}
