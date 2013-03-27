//----------------------------  petsc_51.cc  ---------------------------
//    $Id$
//    Version: $Name$ 
//
//    Copyright (C) 2004, 2005 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//----------------------------  petsc_51.cc  ---------------------------


// check copy constructor PETScWrappers::Vector::Vector(Vector) 

#include "../tests.h"
#include <deal.II/lac/petsc_vector.h>    
#include <fstream>
#include <iostream>
#include <vector>


void test (PETScWrappers::Vector &v)
{
                                   // set some entries of the vector
  for (unsigned int i=0; i<v.size(); ++i)
    if (i%3 == 0)
      v(i) = i+1.;
  v.compress ();

                                   // then copy it
  PETScWrappers::Vector w (v);
  w.compress ();

                                   // make sure they're equal
  deallog << v*w << ' ' << v.l2_norm() * w.l2_norm()
          << ' ' << v*w - v.l2_norm() * w.l2_norm() << std::endl;
  const double eps=typeid(PetscScalar)==typeid(double) ? 1e-14 : 1e-5;
  Assert (std::fabs(v*w - v.l2_norm() * w.l2_norm()) < eps*(v*w),
          ExcInternalError());

  deallog << "OK" << std::endl;
}



int main (int argc,char **argv) 
{
  std::ofstream logfile("51/output");
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  try
    {
      Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);
      {
        PETScWrappers::Vector v (100);
        test (v);
      }
      
    }
  catch (std::exception &exc)
    {
      std::cerr << std::endl << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      std::cerr << "Exception on processing: " << std::endl
		<< exc.what() << std::endl
		<< "Aborting!" << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      
      return 1;
    }
  catch (...) 
    {
      std::cerr << std::endl << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      std::cerr << "Unknown exception!" << std::endl
		<< "Aborting!" << std::endl
		<< "----------------------------------------------------"
		<< std::endl;
      return 1;
    };
}
