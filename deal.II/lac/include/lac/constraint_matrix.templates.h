//---------------------------------------------------------------------------
//    $Id$
//    Version: $Name$
//
//    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//---------------------------------------------------------------------------
#ifndef __deal2__dof_constraints_templates_h
#define __deal2__dof_constraints_templates_h


#include <base/config.h>
#include <lac/constraint_matrix.h>
#include <lac/vector.h>
#include <lac/full_matrix.h>
#include <lac/sparsity_pattern.h>
#include <lac/sparse_matrix.h>
#include <lac/trilinos_sparse_matrix.h>
#include <lac/trilinos_block_sparse_matrix.h>
#include <lac/petsc_block_sparse_matrix.h>
#include <lac/petsc_parallel_block_sparse_matrix.h>
#include <lac/block_sparsity_pattern.h>
#include <lac/block_sparse_matrix.h>

DEAL_II_NAMESPACE_OPEN


template<typename number>
void
ConstraintMatrix::condense (const SparseMatrix<number> &uncondensed,
			    SparseMatrix<number>       &condensed) const
{
				   // create two dummy vectors and enter the
				   // other function
  Vector<number> dummy (0);
  condense (uncondensed, dummy, condensed, dummy);
}



template<typename number>
void
ConstraintMatrix::condense (SparseMatrix<number> &uncondensed) const
{
  Vector<number> dummy (0);
  condense (uncondensed, dummy);
}



template <typename number>
void
ConstraintMatrix::condense (BlockSparseMatrix<number> &uncondensed) const
{
  BlockVector<number> dummy (0);
  condense (uncondensed, dummy);
}



template<class VectorType>
void
ConstraintMatrix::condense (const VectorType &uncondensed,
			    VectorType       &condensed) const
{
  Assert (sorted == true, ExcMatrixNotClosed());
  Assert (condensed.size()+n_constraints() == uncondensed.size(),
	  ExcDimensionMismatch(condensed.size()+n_constraints(),
			       uncondensed.size()));
  
				   // store for each line of the
				   // vector its new line number after
				   // compression. If the shift is -1,
				   // this line will be condensed away
  std::vector<int> new_line;

  new_line.reserve (uncondensed.size());

  std::vector<ConstraintLine>::const_iterator next_constraint = lines.begin();
  unsigned int                                shift           = 0;
  unsigned int n_rows = uncondensed.size();

  if (next_constraint == lines.end()) 
				     // if no constraint is to be handled
    for (unsigned int row=0; row!=n_rows; ++row)
      new_line.push_back (row);
  else
    for (unsigned int row=0; row!=n_rows; ++row)
      if (row == next_constraint->line)
	{
					   // this line is constrained
	  new_line.push_back (-1);
					   // note that @p lines is ordered
	  ++shift;
	  ++next_constraint;
	  if (next_constraint == lines.end())
					     // nothing more to do; finish rest
					     // of loop
	    {
	      for (unsigned int i=row+1; i<n_rows; ++i)
		new_line.push_back (i-shift);
	      break;
	    };
	}
      else
	new_line.push_back (row-shift);


  next_constraint = lines.begin();
				   // note: in this loop we need not check
				   // whether @p next_constraint is a valid
				   // iterator, since @p next_constraint is
				   // only evaluated so often as there are
				   // entries in new_line[*] which tells us
				   // which constraints exist
  for (unsigned int row=0; row<uncondensed.size(); ++row)
    if (new_line[row] != -1)
				       // line not constrained
				       // copy entry
      condensed(new_line[row]) += uncondensed(row);

    else
				       // line must be distributed
      {
	for (unsigned int q=0; q!=next_constraint->entries.size(); ++q) 
	  condensed(new_line[next_constraint->entries[q].first])
	    +=
	    uncondensed(row) * next_constraint->entries[q].second;

	++next_constraint;
      };
}



template <class VectorType>
void
ConstraintMatrix::condense (VectorType &vec) const
{
  Assert (sorted == true, ExcMatrixNotClosed());

                                   // distribute all entries, and set them to zero
  std::vector<ConstraintLine>::const_iterator constraint_line = lines.begin();
  for (; constraint_line!=lines.end(); ++constraint_line)
    {
      for (unsigned int q=0; q!=constraint_line->entries.size(); ++q) 
        vec(constraint_line->entries[q].first)
          += (vec(constraint_line->line) *
              constraint_line->entries[q].second);
      vec(constraint_line->line) = 0.;

				   // in case the constraint is
				   // inhomogeneous, this function is not
				   // appropriate. Throw an exception.
      Assert (constraint_line->inhomogeneity == 0.,
	      ExcMessage ("Inhomogeneous constraint cannot be condensed "
			  "without any matrix specified."));
    }
}



template<typename number, class VectorType>
void
ConstraintMatrix::condense (const SparseMatrix<number> &uncondensed,
			    const VectorType           &uncondensed_vector,
			    SparseMatrix<number>       &condensed,
			    VectorType                 &condensed_vector) const
{
				   // check whether we work on real vectors
				   // or we just used a dummy when calling
				   // the other function above.
  const bool use_vectors = (uncondensed_vector.size() == 0 && 
			    condensed_vector.size() == 0) ? false : true;

  const SparsityPattern &uncondensed_struct = uncondensed.get_sparsity_pattern ();
  
  Assert (sorted == true, ExcMatrixNotClosed());
  Assert (uncondensed_struct.is_compressed() == true, ExcMatrixNotClosed());
  Assert (condensed.get_sparsity_pattern().is_compressed() == true, ExcMatrixNotClosed());
  Assert (uncondensed_struct.n_rows() == uncondensed_struct.n_cols(),
	  ExcNotQuadratic());
  Assert (condensed.n() == condensed.m(),
	  ExcNotQuadratic());
  Assert (condensed.n()+n_constraints() == uncondensed.n(),
	  ExcDimensionMismatch(condensed.n()+n_constraints(), uncondensed.n()));
  if (use_vectors == true)
    {
      Assert (condensed_vector.size()+n_constraints() == uncondensed_vector.size(),
	      ExcDimensionMismatch(condensed_vector.size()+n_constraints(),
				   uncondensed_vector.size()));
      Assert (condensed_vector.size() == condensed.m(),
	      ExcDimensionMismatch(condensed_vector.size(), condensed.m()));
    }

				   // store for each line of the matrix
				   // its new line number
				   // after compression. If the shift is
				   // -1, this line will be condensed away
  std::vector<int> new_line;

  new_line.reserve (uncondensed_struct.n_rows());

  std::vector<ConstraintLine>::const_iterator next_constraint = lines.begin();
  unsigned int                                shift           = 0;
  const unsigned int n_rows = uncondensed_struct.n_rows();

  if (next_constraint == lines.end()) 
				     // if no constraint is to be handled
    for (unsigned int row=0; row!=n_rows; ++row)
      new_line.push_back (row);
  else
    for (unsigned int row=0; row!=n_rows; ++row)
      if (row == next_constraint->line)
	{
					   // this line is constrained
	  new_line.push_back (-1);
					   // note that @p lines is ordered
	  ++shift;
	  ++next_constraint;
	  if (next_constraint == lines.end())
					     // nothing more to do; finish rest
					     // of loop
	    {
	      for (unsigned int i=row+1; i<n_rows; ++i)
		new_line.push_back (i-shift);
	      break;
	    };
	}
      else
	new_line.push_back (row-shift);


  next_constraint = lines.begin();

				   // note: in this loop we need not check
				   // whether @p next_constraint is a valid
				   // iterator, since @p next_constraint is
				   // only evaluated so often as there are
				   // entries in new_line[*] which tells us
				   // which constraints exist
  for (unsigned int row=0; row<uncondensed_struct.n_rows(); ++row)
    if (new_line[row] != -1)
      {
				       // line not constrained
				       // copy entries if column will not
				       // be condensed away, distribute
				       // otherwise
	for (unsigned int j=uncondensed_struct.get_rowstart_indices()[row];
	     j<uncondensed_struct.get_rowstart_indices()[row+1]; ++j)
	  if (new_line[uncondensed_struct.get_column_numbers()[j]] != -1)
	    condensed.add (new_line[row], new_line[uncondensed_struct.get_column_numbers()[j]],
			   uncondensed.global_entry(j));
	  else 
	    {
					     // let c point to the
					     // constraint of this column
	      std::vector<ConstraintLine>::const_iterator c = lines.begin();
	      while (c->line != uncondensed_struct.get_column_numbers()[j])
		++c;

	      for (unsigned int q=0; q!=c->entries.size(); ++q)
					       // distribute to rows with
					       // appropriate weight
		condensed.add (new_line[row], new_line[c->entries[q].first],
			       uncondensed.global_entry(j) * c->entries[q].second);

				   // take care of inhomogeneity:
				   // need to subtract this element from the
				   // vector. this corresponds to an
				   // explicit elimination in the respective
				   // row of the inhomogeneous constraint in
				   // the matrix with Gauss elimination
	      if (use_vectors == true)
		condensed_vector(new_line[row]) -= uncondensed.global_entry(j) * 
		                                   c->inhomogeneity;
	    }

	if (use_vectors == true)
	  condensed_vector(new_line[row]) += uncondensed_vector(row);	  
      }
    else
				       // line must be distributed
      {
	for (unsigned int j=uncondensed_struct.get_rowstart_indices()[row];
	     j<uncondensed_struct.get_rowstart_indices()[row+1]; ++j)
					   // for each column: distribute
	  if (new_line[uncondensed_struct.get_column_numbers()[j]] != -1)
					     // column is not constrained
	    for (unsigned int q=0; q!=next_constraint->entries.size(); ++q) 
	      condensed.add (new_line[next_constraint->entries[q].first],
			     new_line[uncondensed_struct.get_column_numbers()[j]],
			     uncondensed.global_entry(j) *
			     next_constraint->entries[q].second);
	
	  else
					     // not only this line but
					     // also this col is constrained
	    {
					       // let c point to the constraint
					       // of this column
	      std::vector<ConstraintLine>::const_iterator c = lines.begin();
	      while (c->line != uncondensed_struct.get_column_numbers()[j])
		++c;
	      
	      for (unsigned int p=0; p!=c->entries.size(); ++p)
		for (unsigned int q=0; q!=next_constraint->entries.size(); ++q)
		  condensed.add (new_line[next_constraint->entries[q].first],
				 new_line[c->entries[p].first],
				 uncondensed.global_entry(j) *
				 next_constraint->entries[q].second *
				 c->entries[p].second);

	      if (use_vectors == true)
		for (unsigned int q=0; q!=next_constraint->entries.size(); ++q)
		  condensed_vector (new_line[next_constraint->entries[q].first])
		    -= uncondensed.global_entry(j) * 
		       next_constraint->entries[q].second *
		       c->inhomogeneity;
	    };

				   // condense the vector
	if (use_vectors == true)
	  for (unsigned int q=0; q!=next_constraint->entries.size(); ++q) 
	    condensed_vector(new_line[next_constraint->entries[q].first])
	      +=
	      uncondensed_vector(row) * next_constraint->entries[q].second;

	++next_constraint;
      };
}



template<typename number, class VectorType>
void
ConstraintMatrix::condense (SparseMatrix<number> &uncondensed,
			    VectorType           &vec) const
{
				   // check whether we work on real vectors
				   // or we just used a dummy when calling
				   // the other function above.
  const bool use_vectors = vec.size() == 0 ? false : true;

  const SparsityPattern &sparsity = uncondensed.get_sparsity_pattern ();

  Assert (sorted == true, ExcMatrixNotClosed());
  Assert (sparsity.is_compressed() == true, ExcMatrixNotClosed());
  Assert (sparsity.n_rows() == sparsity.n_cols(),
	  ExcNotQuadratic());
  if (use_vectors == true)
    {
      Assert (vec.size() == sparsity.n_rows(), 
	      ExcDimensionMismatch(vec.size(), sparsity.n_rows()));
    }

  double average_diagonal = 0;
  for (unsigned int i=0; i<uncondensed.m(); ++i)
    average_diagonal += std::fabs (uncondensed.diag_element(i));
  average_diagonal /= uncondensed.m();
  
				   // store for each index whether it must be
				   // distributed or not. If entry is
				   // invalid_unsigned_int, no distribution is
				   // necessary.  otherwise, the number states
				   // which line in the constraint matrix
				   // handles this index
  std::vector<unsigned int> distribute (sparsity.n_rows(),
                                        numbers::invalid_unsigned_int);
  
  for (unsigned int c=0; c<lines.size(); ++c)
    distribute[lines[c].line] = c;

  const unsigned int n_rows = sparsity.n_rows();
  for (unsigned int row=0; row<n_rows; ++row)
    {
      if (distribute[row] == numbers::invalid_unsigned_int)
					 // regular line. loop over cols
        {
          for (typename SparseMatrix<number>::iterator
                 entry = uncondensed.begin(row);
               entry != uncondensed.end(row); ++entry)
            {
              const unsigned int column = entry->column();
              
                                               // end of row reached?
                                               // this should not
                                               // happen, since we only
                                               // operate on compressed
                                               // matrices!
              Assert (column != SparsityPattern::invalid_entry,
                      ExcMatrixNotClosed());
	    
              if (distribute[column] != numbers::invalid_unsigned_int)
                                                 // distribute entry at
                                                 // regular row @p row
                                                 // and irregular column
                                                 // sparsity.get_column_numbers()[j];
                                                 // set old entry to
                                                 // zero
                {
                  for (unsigned int q=0;
                       q!=lines[distribute[column]].entries.size(); ++q)
                    uncondensed.add (row,
                                     lines[distribute[column]].entries[q].first,
                                     entry->value() *
                                     lines[distribute[column]].entries[q].second);

				   // need to subtract this element from the
				   // vector. this corresponds to an
				   // explicit elimination in the respective
				   // row of the inhomogeneous constraint in
				   // the matrix with Gauss elimination
		  if (use_vectors == true)
		    vec(row) -= 
		      entry->value() * lines[distribute[column]].inhomogeneity;

                                                   // set old value to zero
                  entry->value() = 0.;
                }
            }
        }
      else
					 // row must be distributed
        {
          for (typename SparseMatrix<number>::iterator
                 entry = uncondensed.begin(row);
               entry != uncondensed.end(row); ++entry)
            {
              const unsigned int column = entry->column();

                                               // end of row reached?
                                               // this should not
                                               // happen, since we only
                                               // operate on compressed
                                               // matrices!
              Assert (column != SparsityPattern::invalid_entry,
                      ExcMatrixNotClosed());

              if (distribute[column] == numbers::invalid_unsigned_int)
                                                 // distribute entry at
                                                 // irregular row
                                                 // @p row and regular
                                                 // column
                                                 // column. set
                                                 // old entry to zero
                {
                  for (unsigned int q=0;
                       q!=lines[distribute[row]].entries.size(); ++q) 
                    uncondensed.add (lines[distribute[row]].entries[q].first,
                                     column,
                                     entry->value() *
                                     lines[distribute[row]].entries[q].second);

                                                   // set old entry to zero
                  entry->value() = 0.;
                }
              else
                                                 // distribute entry at
                                                 // irregular row @p row and
                                                 // irregular column
                                                 // @p column set old entry
                                                 // to one on main
                                                 // diagonal, zero otherwise
                {
                  for (unsigned int p=0; p!=lines[distribute[row]].entries.size(); ++p)
		    {
		      for (unsigned int q=0;
			   q!=lines[distribute[column]].entries.size(); ++q)
			uncondensed.add (lines[distribute[row]].entries[p].first,
					 lines[distribute[column]].entries[q].first,
					 entry->value() *
					 lines[distribute[row]].entries[p].second *
					 lines[distribute[column]].entries[q].second);

		      if (use_vectors == true)
			vec(lines[distribute[row]].entries[p].first) -= 
			  entry->value() * lines[distribute[row]].entries[p].second *
			  lines[distribute[column]].inhomogeneity;
		    }

                                                   // set old entry to correct
                                                   // value
                  entry->value() = (row == column ? average_diagonal : 0. );
                }
            }

				   // take care of vector
	  if (use_vectors == true)
	    {
	      for (unsigned int q=0; q!=lines[distribute[row]].entries.size(); ++q) 
		vec(lines[distribute[row]].entries[q].first)
		  += (vec(row) * lines[distribute[row]].entries[q].second);

	      vec(lines[distribute[row]].line) = 0.;
	    }
        }
    }
}



template <typename number, class BlockVectorType>
void
ConstraintMatrix::condense (BlockSparseMatrix<number> &uncondensed,
			    BlockVectorType           &vec) const
{
				   // check whether we work on real vectors
				   // or we just used a dummy when calling
				   // the other function above.
  const bool use_vectors = vec.n_blocks() == 0 ? false : true;

  const unsigned int blocks = uncondensed.n_block_rows();
  
  const BlockSparsityPattern &
    sparsity = uncondensed.get_sparsity_pattern ();

  Assert (sorted == true, ExcMatrixNotClosed());
  Assert (sparsity.is_compressed() == true, ExcMatrixNotClosed());
  Assert (sparsity.n_rows() == sparsity.n_cols(),
	  ExcNotQuadratic());
  Assert (sparsity.n_block_rows() == sparsity.n_block_cols(),
	  ExcNotQuadratic());
  Assert (sparsity.n_block_rows() == sparsity.n_block_cols(),
	  ExcNotQuadratic());
  Assert (sparsity.get_column_indices() == sparsity.get_row_indices(),
	  ExcNotQuadratic());

  if (use_vectors == true)
    {
      Assert (vec.size() == sparsity.n_rows(), 
	      ExcDimensionMismatch(vec.size(), sparsity.n_rows()));
      Assert (vec.n_blocks() == sparsity.n_block_rows(),
	      ExcDimensionMismatch(vec.n_blocks(), sparsity.n_block_rows()));
    }

  double average_diagonal = 0;
  for (unsigned int b=0; b<uncondensed.n_block_rows(); ++b)
    for (unsigned int i=0; i<uncondensed.block(b,b).m(); ++i)
      average_diagonal += std::fabs (uncondensed.block(b,b).diag_element(i));
  average_diagonal /= uncondensed.m();

  const BlockIndices &
    index_mapping = sparsity.get_column_indices();
  
				   // store for each index whether it must be
				   // distributed or not. If entry is
				   // numbers::invalid_unsigned_int,
				   // no distribution is necessary.
				   // otherwise, the number states which line
				   // in the constraint matrix handles this
				   // index
  std::vector<unsigned int> distribute (sparsity.n_rows(),
                                        numbers::invalid_unsigned_int);
  
  for (unsigned int c=0; c<lines.size(); ++c)
    distribute[lines[c].line] = c;

  const unsigned int n_rows = sparsity.n_rows();
  for (unsigned int row=0; row<n_rows; ++row)
    {
				       // get index of this row
				       // within the blocks
      const std::pair<unsigned int,unsigned int>
	block_index = index_mapping.global_to_local(row);
      const unsigned int block_row = block_index.first;
      
      if (distribute[row] == numbers::invalid_unsigned_int)
					 // regular line. loop over
					 // all columns and see
					 // whether this column must
					 // be distributed
	{

					   // to loop over all entries
					   // in this row, we have to
					   // loop over all blocks in
					   // this blockrow and the
					   // corresponding row
					   // therein
	  for (unsigned int block_col=0; block_col<blocks; ++block_col)
	    {
              for (typename SparseMatrix<number>::iterator
                     entry = uncondensed.block(block_row, block_col).begin(block_index.second);
                   entry != uncondensed.block(block_row, block_col).end(block_index.second);
                   ++entry)
                {
                  const unsigned int global_col
                    = index_mapping.local_to_global(block_col,entry->column());
		    
                  if (distribute[global_col] != numbers::invalid_unsigned_int)
                                                     // distribute entry at
                                                     // regular row @p row
                                                     // and irregular column
                                                     // global_col; set old
                                                     // entry to zero
                    {
                      const double old_value = entry->value ();
			
                      for (unsigned int q=0;
                           q!=lines[distribute[global_col]].entries.size(); ++q)
                        uncondensed.add (row,
                                         lines[distribute[global_col]].entries[q].first,
                                         old_value *
                                         lines[distribute[global_col]].entries[q].second);

				   // need to subtract this element from the
				   // vector. this corresponds to an
				   // explicit elimination in the respective
				   // row of the inhomogeneous constraint in
				   // the matrix with Gauss elimination
		      if (use_vectors == true)
			vec(row) -= entry->value() * 
			            lines[distribute[global_col]].inhomogeneity;

                      entry->value() = 0.;
                    }
                }
	    }
	}
      else
	{
					   // row must be
					   // distributed. split the
					   // whole row into the
					   // chunks defined by the
					   // blocks
	  for (unsigned int block_col=0; block_col<blocks; ++block_col)
	    {
              for (typename SparseMatrix<number>::iterator
                     entry = uncondensed.block(block_row, block_col).begin(block_index.second);
                   entry != uncondensed.block(block_row, block_col).end(block_index.second);
                   ++entry)
                {
                  const unsigned int global_col
                    = index_mapping.local_to_global (block_col, entry->column());
		    
                  if (distribute[global_col] ==
                      numbers::invalid_unsigned_int)
                                                     // distribute
                                                     // entry at
                                                     // irregular
                                                     // row @p row
                                                     // and regular
                                                     // column
                                                     // global_col. set
                                                     // old entry to
                                                     // zero
                    {
                      const double old_value = entry->value();
			  
                      for (unsigned int q=0;
                           q!=lines[distribute[row]].entries.size(); ++q) 
                        uncondensed.add (lines[distribute[row]].entries[q].first,
                                         global_col,
                                         old_value *
                                         lines[distribute[row]].entries[q].second);

                      entry->value() = 0.;
                    }
                  else
                                                     // distribute entry at
                                                     // irregular row @p row
                                                     // and irregular column
                                                     // @p global_col set old
                                                     // entry to one if on
                                                     // main diagonal, zero
                                                     // otherwise
                    {
                      const double old_value = entry->value ();
			  
                      for (unsigned int p=0; p!=lines[distribute[row]].entries.size(); ++p)
			{
			  for (unsigned int q=0; q!=lines[distribute[global_col]].entries.size(); ++q)
			    uncondensed.add (lines[distribute[row]].entries[p].first,
					     lines[distribute[global_col]].entries[q].first,
					     old_value *
					     lines[distribute[row]].entries[p].second *
					     lines[distribute[global_col]].entries[q].second);

			  if (use_vectors == true)
			    vec(lines[distribute[row]].entries[p].first) -= 
			      old_value * lines[distribute[row]].entries[p].second *
			      lines[distribute[global_col]].inhomogeneity;
			}

                      entry->value() = (row == global_col ? average_diagonal : 0. );
                    }
                }
	    }

	  				   // take care of vector
	  if (use_vectors == true)
	    {
	      for (unsigned int q=0; q!=lines[distribute[row]].entries.size(); ++q) 
		vec(lines[distribute[row]].entries[q].first)
		  += (vec(row) * lines[distribute[row]].entries[q].second);

	      vec(lines[distribute[row]].line) = 0.;
	    }
	}
    }
}



template <class VectorType>
void
ConstraintMatrix::set_zero (VectorType &vec) const
{
  Assert (sorted == true, ExcMatrixNotClosed());

  std::vector<ConstraintLine>::const_iterator constraint_line = lines.begin();
  for (; constraint_line!=lines.end(); ++constraint_line)
    vec(constraint_line->line) = 0.;
}



template <typename VectorType>
void
ConstraintMatrix::
distribute_local_to_global (const Vector<double>            &local_vector,
                            const std::vector<unsigned int> &local_dof_indices,
                            VectorType                      &global_vector) const
{
  Assert (local_vector.size() == local_dof_indices.size(),
          ExcDimensionMismatch(local_vector.size(), local_dof_indices.size()));
  Assert (sorted == true, ExcMatrixNotClosed());

  const unsigned int n_local_dofs = local_vector.size();
  
                                   // have a special case where there are no
                                   // constraints at all, since then we can be
                                   // a lot faster
  Threads::ThreadMutex::ScopedLock lock(mutex);
  for (unsigned int i=0; i<n_local_dofs; ++i)
    {
				   // let's see if we can use the bool
				   // vector that tells about whether a
				   // certain constraint exists.
      if (constraint_line_exists.size() <= local_dof_indices[i])
	{
	  global_vector(local_dof_indices[i]) += local_vector(i);
	  continue;
	}
      if (constraint_line_exists[local_dof_indices[i]] == false)
	{
	  global_vector(local_dof_indices[i]) += local_vector(i);
	  continue;
	}
                                           // first figure out whether this
                                           // dof is constrained
      ConstraintLine index_comparison;
      index_comparison.line = local_dof_indices[i];

      const std::vector<ConstraintLine>::const_iterator
	position = std::lower_bound (lines.begin(),
				     lines.end(),
				     index_comparison);

                                           // if the line is not
                                           // constrained, then simply
                                           // copy the data. otherwise
                                           // distribute it, but make
                                           // sure we don't touch the
                                           // entries of fixed dofs
					   //
					   // there is one critical
					   // point: sometimes a dof
					   // may be both constrained
					   // and fixed, for example
					   // hanging nodes in 3d at
					   // the boundary. in that
					   // case, we don't quite
					   // know what to do --
					   // handle the constraint or
					   // the fixed
					   // value. however, this
					   // isn't so hard if all the
					   // nodes that this node is
					   // constrained to are also
					   // fixed nodes, in which
					   // case we could do both
					   // but opt to copy the
					   // element. however, we
					   // have to check that all
					   // the nodes to which it is
					   // constrained are also
					   // fixed
      Assert (position->line == local_dof_indices[i],
	      ExcInternalError());
      for (unsigned int j=0; j<position->entries.size(); ++j)
	global_vector(position->entries[j].first)
	  += local_vector(i) * position->entries[j].second;
    }
}



template <typename MatrixType>
void
ConstraintMatrix::
distribute_local_to_global (const FullMatrix<double>        &local_matrix,
                            const std::vector<unsigned int> &local_dof_indices,
                            MatrixType                      &global_matrix) const
{
				   // create a dummy and hand on to the
				   // function actually implementing this
				   // feature further down.
  Vector<double> dummy(0);
  distribute_local_to_global (local_matrix, dummy, local_dof_indices,
			      global_matrix, dummy, 
			      internal::bool2type<IsBlockMatrix<MatrixType>::value>());
}



template <typename MatrixType, typename VectorType>
void
ConstraintMatrix::
distribute_local_to_global (const FullMatrix<double>        &local_matrix,
			    const Vector<double>            &local_vector,
                            const std::vector<unsigned int> &local_dof_indices,
                            MatrixType                      &global_matrix,
			    VectorType                      &global_vector) const
{
				   // enter the internal function with the
				   // respective block information set, the
				   // actual implementation follows further
				   // down.
  distribute_local_to_global (local_matrix, local_vector, local_dof_indices,
			      global_matrix, global_vector, 
			      internal::bool2type<IsBlockMatrix<MatrixType>::value>());
}



template <typename SparsityType>
void
ConstraintMatrix::
add_entries_local_to_global (const std::vector<unsigned int> &local_dof_indices,
			     SparsityType                    &sparsity_pattern,
			     const bool                       keep_constrained_entries,
			     const Table<2,bool>             &dof_mask) const
{
				   // enter the internal function with the
				   // respective block information set, the
				   // actual implementation follows further
				   // down.
  add_entries_local_to_global (local_dof_indices, sparsity_pattern,
			       keep_constrained_entries, dof_mask,
			       internal::bool2type<IsBlockMatrix<SparsityType>::value>());
}



template<class VectorType>
void
ConstraintMatrix::distribute (const VectorType &condensed,
			      VectorType       &uncondensed) const
{
  Assert (sorted == true, ExcMatrixNotClosed());
  Assert (condensed.size()+n_constraints() == uncondensed.size(),
	  ExcDimensionMismatch(condensed.size()+n_constraints(),
			       uncondensed.size()));

				   // store for each line of the new vector
				   // its old line number before
				   // distribution. If the shift is
				   // -1, this line was condensed away
  std::vector<int> old_line;

  old_line.reserve (uncondensed.size());

  std::vector<ConstraintLine>::const_iterator next_constraint = lines.begin();
  unsigned int                                shift           = 0;
  unsigned int n_rows = uncondensed.size();

  if (next_constraint == lines.end()) 
				     // if no constraint is to be handled
    for (unsigned int row=0; row!=n_rows; ++row)
      old_line.push_back (row);
  else
    for (unsigned int row=0; row!=n_rows; ++row)
      if (row == next_constraint->line)
	{
					   // this line is constrained
	  old_line.push_back (-1);
					   // note that @p lines is ordered
	  ++shift;
	  ++next_constraint;
	  if (next_constraint == lines.end())
					     // nothing more to do; finish rest
					     // of loop
	    {
	      for (unsigned int i=row+1; i<n_rows; ++i)
		old_line.push_back (i-shift);
	      break;
	    };
	}
      else
	old_line.push_back (row-shift);


  next_constraint = lines.begin();
				   // note: in this loop we need not check
				   // whether @p next_constraint is a valid
				   // iterator, since @p next_constraint is
				   // only evaluated so often as there are
				   // entries in new_line[*] which tells us
				   // which constraints exist
  for (unsigned int line=0; line<uncondensed.size(); ++line) 
    if (old_line[line] != -1)
				       // line was not condensed away
      uncondensed(line) = condensed(old_line[line]);
    else
      {
					 // line was condensed away,
					 // create it newly. first set
					 // it to zero
	uncondensed(line) = next_constraint->inhomogeneity;
					 // then add the different
					 // contributions
	for (unsigned int i=0; i<next_constraint->entries.size(); ++i)
	  uncondensed(line) += (condensed(old_line[next_constraint->entries[i].first]) *
				next_constraint->entries[i].second);
	++next_constraint;
      };
}



template<class VectorType>
void
ConstraintMatrix::distribute (VectorType &vec) const
{
  Assert (sorted == true, ExcMatrixNotClosed());

  std::vector<ConstraintLine>::const_iterator next_constraint = lines.begin();
  for (; next_constraint != lines.end(); ++next_constraint) 
    {
				       // fill entry in line
				       // next_constraint.line by adding the
				       // different contributions
      double new_value = next_constraint->inhomogeneity;
      for (unsigned int i=0; i<next_constraint->entries.size(); ++i)
	new_value += (vec(next_constraint->entries[i].first) *
                      next_constraint->entries[i].second);
      vec(next_constraint->line) = new_value;
    }
}


				   // Some helper definitions for the
				   // local_to_global functions.
namespace internals
{
  struct distributing 
  {
    distributing (const unsigned int local_row = deal_II_numbers::invalid_unsigned_int);
    unsigned int local_row;
    std::vector<std::pair<unsigned int,double> > constraints;
  };
  distributing::distributing (const unsigned int local_row) :
    local_row (local_row) {}

				   // a version of std::lower_bound for a
				   // pair of unsigned int and distributing,
				   // without taking into account the second
				   // argument of the pair.
  inline
  std::vector<std::pair<unsigned int,distributing> >::iterator
  lower_bound (std::vector<std::pair<unsigned int,distributing> >::iterator begin,
	       std::vector<std::pair<unsigned int,distributing> >::iterator end,
	       unsigned int val)
  {
    unsigned int length = end - begin;
    unsigned int half;
    std::vector<std::pair<unsigned int,distributing> >::iterator middle;

    while (length > 0)
      {
	half = length >> 1;
	middle = begin + half;
	if (middle->first < val)
	  {
	    begin = middle;
	    ++begin;
	    length = length - half - 1;
	  }
	else
	  length = half;
      }
    return begin;
  }

				   // a function that appends an additional
				   // row to the list of values, or appends
				   // a value to an already existing
				   // row. Similar functionality as for
				   // std::map<unsigned int,distributing>,
				   // but here done for a vector of pairs,
				   // and much faster.
  inline
  void
  insert_index (std::vector<std::pair<unsigned int,distributing> > &my_indices,
		const unsigned int row,
		const unsigned int local_row,
		const std::pair<unsigned int,double> constraint 
		  = std::make_pair<unsigned int,double>(0,0))
  {
    typedef std::vector<std::pair<unsigned int,distributing> >::iterator 
      index_iterator;
    index_iterator pos, pos1;

    if (my_indices.size() == 0 || my_indices.back().first < row)
      {
	my_indices.push_back(std::make_pair<unsigned int,distributing>(row,distributing()));
	pos1 = my_indices.end()-1;
      }
    else
      {
	pos = lower_bound (my_indices.begin(),
			   my_indices.end(),
			   row);

	if (pos->first == row)
	  pos1 = pos;
	else
	  pos1 = my_indices.insert(pos,std::make_pair<unsigned int,distributing>
				   (row,distributing()));
      }    

    if (local_row == deal_II_numbers::invalid_unsigned_int)
      pos1->second.constraints.push_back (constraint);
    else
      pos1->second.local_row = local_row;
  }

  inline
  void
  list_shellsort (std::vector<std::pair<unsigned int,distributing> > &my_indices)
  {
				   // now sort the actual dofs using a shell
				   // sort (which is very fast in case the
				   // indices are already sorted (which is
				   // the usual case with DG elements)
    unsigned int i, j, j2, temp, templ, istep;
    unsigned step;

    const unsigned int length = my_indices.size();
    step = length/2;
    while (step > 0)
      {
	for (i=step; i < length; i++)
	  {
	    istep = step;
	    j = i;
	    j2 = j-istep;
	    temp = my_indices[i].first;
	    templ = my_indices[i].second.local_row;
	    if (my_indices[j2].first > temp) 
	      {
		while ((j >= istep) && (my_indices[j2].first > temp))
		  {
		    my_indices[j].first = my_indices[j2].first;
		    my_indices[j].second.local_row = my_indices[j2].second.local_row;
		    j = j2;
		    j2 -= istep;
		  }
		my_indices[j].first = temp;
		my_indices[j].second.local_row = templ;
	      }
	  }
	step = step>>1;
      }
  }

}


				   // internal implementation for
				   // distribute_local_to_global for
				   // standard (non-block) matrices
template <typename MatrixType, typename VectorType>
inline
void
ConstraintMatrix::
distribute_local_to_global (const FullMatrix<double>        &local_matrix,
			    const Vector<double>            &local_vector,
                            const std::vector<unsigned int> &local_dof_indices,
                            MatrixType                      &global_matrix,
			    VectorType                      &global_vector,
			    internal::bool2type<false>) const
{
				   // check whether we work on real vectors
				   // or we just used a dummy when calling
				   // the other function above.
  const bool use_vectors = (local_vector.size() == 0 && 
			    global_vector.size() == 0) ? false : true;

  Assert (local_matrix.n() == local_dof_indices.size(),
          ExcDimensionMismatch(local_matrix.n(), local_dof_indices.size()));
  Assert (local_matrix.m() == local_dof_indices.size(),
          ExcDimensionMismatch(local_matrix.m(), local_dof_indices.size()));
  Assert (global_matrix.m() == global_matrix.n(), ExcNotQuadratic());
  if (use_vectors == true)
    {
      Assert (local_matrix.m() == local_vector.size(),
	      ExcDimensionMismatch(local_matrix.m(), local_vector.size()));
      Assert (global_matrix.m() == global_vector.size(),
	      ExcDimensionMismatch(global_matrix.m(), global_vector.size()));
    }
  Assert (sorted == true, ExcMatrixNotClosed());

  const unsigned int n_local_dofs = local_dof_indices.size();

				   // when distributing the local data to
				   // the global matrix, we can quite
				   // cheaply sort the indices (obviously,
				   // this introduces the need for
				   // allocating some memory on the way, but
				   // we need to do this only for rows,
				   // whereas the distribution process
				   // itself goes over rows and
				   // columns). This has the advantage that
				   // when writing into the global matrix,
				   // we can make use of the sortedness.

				   // so the first step is to create a
				   // sorted list of all row values that are
				   // possible. these values are either the
				   // rows from unconstrained dofs, or some
				   // indices introduced by dofs constrained
				   // to a combination of some other
				   // dofs. regarding the data type, choose
				   // an STL vector of a pair of unsigned
				   // ints (for global columns) and internal
				   // data (containing local columns +
				   // possible jumps from
				   // constraints). Choosing an STL map or
				   // anything else I know of would be much
				   // more expensive here!
  std::vector<std::pair<unsigned int,internals::distributing> > my_indices (n_local_dofs);

  std::vector<std::pair<unsigned int, const ConstraintLine *> > constraint_lines;
  constraint_lines.reserve(n_local_dofs);

				   // cache whether we have to resolve any
				   // indirect rows generated from resolving
				   // constrained dofs.
  bool have_indirect_rows = false;
  {
    unsigned int added_rows = 0;
				   // first add the indices in an unsorted
				   // way and only keep track of the
				   // constraints that appear. They are
				   // resolved in a second step.
    for (unsigned int i = 0; i<n_local_dofs; ++i)
      {
	if (constraint_line_exists.size() <= local_dof_indices[i] ||
	    constraint_line_exists[local_dof_indices[i]] == false)
	  {
	    my_indices[added_rows].first = local_dof_indices[i];
	    my_indices[added_rows].second.local_row = i;
	    ++added_rows;
	    continue;
	  }

	ConstraintLine index_comparison;
	index_comparison.line = local_dof_indices[i];

	const std::vector<ConstraintLine>::const_iterator
	  position = std::lower_bound (lines.begin(),
				       lines.end(),
				       index_comparison);
	Assert (position->line == local_dof_indices[i],
		ExcInternalError());

	constraint_lines.push_back (std::make_pair<unsigned int,
				    const ConstraintLine *>(i,&*position));
      }
    Assert (constraint_lines.size() + added_rows == n_local_dofs,
	    ExcInternalError());
    my_indices.resize (added_rows);
  }
  internals::list_shellsort (my_indices);

				   // now in the second step actually
				   // resolve the constraints
  const unsigned int n_constrained_dofs = constraint_lines.size();
  for (unsigned int i=0; i<n_constrained_dofs; ++i)
    {
      const unsigned int local_row = constraint_lines[i].first;
      const unsigned int global_row = local_dof_indices[local_row];
      const ConstraintLine * position = constraint_lines[i].second;
      for (unsigned int q=0; q<position->entries.size(); ++q)
	{
	  have_indirect_rows = true;
	  internals::insert_index(my_indices, position->entries[q].first,
				  deal_II_numbers::invalid_unsigned_int,
				  std::make_pair<unsigned int,double> 
				  (local_row, position->entries[q].second));
	}

				   // to make sure that the global matrix
				   // remains invertible, we need to do
				   // something with the diagonal
				   // elements. add the absolute value of
				   // the local matrix, so the resulting
				   // entry will always be positive and
				   // furthermore be in the same order of
				   // magnitude as the other elements of the
				   // matrix
				   //
				   // note that this also captures the
				   // special case that a dof is both
				   // constrained and fixed (this can happen
				   // for hanging nodes in 3d that also
				   // happen to be on the boundary). in that
				   // case, following the above program
				   // flow, it is realized that when
				   // distributing the row and column no
				   // elements of the matrix are actually
				   // touched if all the degrees of freedom
				   // to which this dof is constrained are
				   // also constrained (the usual case with
				   // hanging nodes in 3d). however, in the
				   // line below, we do actually do
				   // something with this dof
      Threads::ThreadMutex::ScopedLock lock(mutex);
      global_matrix.add(global_row,global_row,
			std::fabs(local_matrix(local_row,local_row)));
    }

  const unsigned int n_actual_dofs = my_indices.size();

				   // create arrays for the column data
				   // (indices and values) that will then be
				   // written into the matrix.
  std::vector<unsigned int> cols (n_actual_dofs);
  std::vector<double>       vals (n_actual_dofs);

				   // now do the actual job. 
  for (unsigned int i=0; i<n_actual_dofs; ++i)
    {
      unsigned int col_counter = 0;
      const unsigned int row = my_indices[i].first;
      const unsigned int loc_row = my_indices[i].second.local_row;
      double val = 0;

				   // fast function if there are no indirect
				   // references to any of the local rows at
				   // all on this set of dofs (saves a lot
				   // of checks). the only check we actually
				   // need to perform is whether the matrix
				   // element is zero. 
      if (have_indirect_rows == false)
	{
	  Assert(loc_row >= 0 && loc_row < n_local_dofs,
		 ExcInternalError());

	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
	      const unsigned int loc_col = my_indices[j].second.local_row;
	      Assert(loc_col >= 0 && loc_col < n_local_dofs,
		     ExcInternalError());

	      if (local_matrix(loc_row,loc_col) != 0)
		{
		  vals[col_counter] = local_matrix(loc_row,loc_col);
		  cols[col_counter] = my_indices[j].first;
		  col_counter++;
		}
	    }

	  if (use_vectors == true)
	    {
	      val = local_vector(loc_row);

				   // need to account for inhomogeneities
				   // here: thie corresponds to eliminating
				   // the respective column in the local
				   // matrix with value on the right hand
				   // side.
	      for (unsigned int i=0; i<constraint_lines.size(); ++i)
		val -= constraint_lines[i].second->inhomogeneity *
		  local_matrix(loc_row,constraint_lines[i].first);
	    }
	}

				   // slower functions when there are
				   // indirect references and when we need
				   // to do some more checks.
      else
	{
	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
	      double col_val;
	      const unsigned int loc_col = my_indices[j].second.local_row;

				   // case 1: row has direct contribution in
				   // local matrix
	      if (loc_row != deal_II_numbers::invalid_unsigned_int)
		{
		  Assert (loc_row >= 0 && loc_row < n_local_dofs,
			  ExcInternalError());

				   // case 1a: col has direct contribution
				   // in local matrix
		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      col_val = local_matrix(loc_row,loc_col);
		    }
				   // case 1b: col has no direct
				   // contribution in local matrix
		  else
		    col_val = 0;

				   // account for indirect contributions by
				   // constraints
		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    col_val += local_matrix(loc_row,
					    my_indices[j].second.constraints[p].first) 
		               *
		               my_indices[j].second.constraints[p].second;
		}

				   // case 2: row has no direct contribution in
				   // local matrix
	      else
		col_val = 0;

				   // account for indirect contributions by
				   // constraints in row, going trough the
				   // direct and indirect references in the
				   // given column.
	      for (unsigned int q=0; q<my_indices[i].second.constraints.size(); ++q)
		{
		  double add_this;
		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      add_this = local_matrix(my_indices[i].second.constraints[q].first,
					      loc_col);
		    }
		  else
		    add_this = 0;
  
		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    add_this += local_matrix(my_indices[i].second.constraints[q].first,
					     my_indices[j].second.constraints[p].first) 
		                *
			        my_indices[j].second.constraints[p].second;
		  col_val += add_this * my_indices[i].second.constraints[q].second;
		}

		   

				   // if we got some nontrivial value,
				   // append it to the array of values.
	      if (col_val != 0)
		{
		  cols[col_counter] = my_indices[j].first;
		  vals[col_counter] = col_val;
		  col_counter++;
		}
	    }

				   // now to the vectors. besides doing the
				   // same job as we did above (i.e.,
				   // distribute the content of the local
				   // vector into the global one), need to
				   // account for inhomogeneities here: thie
				   // corresponds to eliminating the
				   // respective column in the local matrix
				   // with value on the right hand side.
	  if (use_vectors == true)
	    {
	      if (loc_row != deal_II_numbers::invalid_unsigned_int)
		{
		  Assert (loc_row >= 0 && loc_row < n_local_dofs,
			  ExcInternalError());
		  val = local_vector(loc_row);
		  for (unsigned int i=0; i<constraint_lines.size(); ++i)
		    val -= constraint_lines[i].second->inhomogeneity *
		           local_matrix(loc_row,constraint_lines[i].first);
		}

	      for (unsigned int q=0; q < my_indices[i].second.constraints.size(); ++q)
		{
		  const unsigned int loc_row_q = my_indices[i].second.constraints[q].first;
		  double add_this = local_vector (loc_row_q);
		  for (unsigned int k=0; k<constraint_lines.size(); ++k)
		    add_this -= constraint_lines[k].second->inhomogeneity *
		                local_matrix(loc_row_q,constraint_lines[k].first);
		  val += add_this * my_indices[i].second.constraints[q].second;
		}
	    }
	}

				   // finally, write all the information
				   // that accumulated under the given
				   // process into the global matrix row and
				   // into the vector
      Threads::ThreadMutex::ScopedLock lock(mutex);
      if (col_counter > 0)
	global_matrix.add(row, col_counter, &cols[0], &vals[0], false, true);
      if (val != 0)
	global_vector(row) += val;
    }
}



template <typename MatrixType, typename VectorType>
inline
void
ConstraintMatrix::
distribute_local_to_global (const FullMatrix<double>        &local_matrix,
			    const Vector<double>            &local_vector,
                            const std::vector<unsigned int> &local_dof_indices,
                            MatrixType                      &global_matrix,
			    VectorType                      &global_vector,
			    internal::bool2type<true>) const
{
				   // similar function as above, but now
				   // specialized for block matrices. See
				   // the other function for additional
				   // comments.

  const bool use_vectors = (local_vector.size() == 0 && 
			    global_vector.size() == 0) ? false : true;

  Assert (local_matrix.n() == local_dof_indices.size(),
          ExcDimensionMismatch(local_matrix.n(), local_dof_indices.size()));
  Assert (local_matrix.m() == local_dof_indices.size(),
          ExcDimensionMismatch(local_matrix.m(), local_dof_indices.size()));
  Assert (global_matrix.m() == global_matrix.n(), ExcNotQuadratic());
  Assert (global_matrix.n_block_rows() == global_matrix.n_block_cols(),
	  ExcNotQuadratic());
  if (use_vectors == true)
    {
      Assert (local_matrix.m() == local_vector.size(),
	      ExcDimensionMismatch(local_matrix.m(), local_vector.size()));
      Assert (global_matrix.m() == global_vector.size(),
	      ExcDimensionMismatch(global_matrix.m(), global_vector.size()));
    }
  Assert (sorted == true, ExcMatrixNotClosed());

  const unsigned int n_local_dofs = local_dof_indices.size();
  const unsigned int num_blocks   = global_matrix.n_block_rows();

  std::vector<std::pair<unsigned int,internals::distributing> > my_indices (n_local_dofs);
  std::vector<std::pair<unsigned int, const ConstraintLine *> > constraint_lines;
  constraint_lines.reserve(n_local_dofs);

  bool have_indirect_rows = false;
  {
    unsigned int added_rows = 0;
    for (unsigned int i = 0; i<n_local_dofs; ++i)
      {
	if (constraint_line_exists.size() <= local_dof_indices[i] ||
	    constraint_line_exists[local_dof_indices[i]] == false)
	  {
	    my_indices[added_rows].first = local_dof_indices[i];
	    my_indices[added_rows].second.local_row = i;
	    ++added_rows;
	    continue;
	  }

	ConstraintLine index_comparison;
	index_comparison.line = local_dof_indices[i];

	const std::vector<ConstraintLine>::const_iterator
	  position = std::lower_bound (lines.begin(),
				       lines.end(),
				       index_comparison);
	Assert (position->line == local_dof_indices[i],
		ExcInternalError());

	constraint_lines.push_back (std::make_pair<unsigned int,
				    const ConstraintLine *>(i,&*position));
      }
    Assert (constraint_lines.size() + added_rows == n_local_dofs,
	    ExcInternalError());
    my_indices.resize (added_rows);
  }
  internals::list_shellsort (my_indices);

  const unsigned int n_constrained_dofs = constraint_lines.size();
  for (unsigned int i=0; i<n_constrained_dofs; ++i)
    {
      const unsigned int local_row = constraint_lines[i].first;
      const unsigned int global_row = local_dof_indices[local_row];
      const ConstraintLine * position = constraint_lines[i].second;
      for (unsigned int q=0; q<position->entries.size(); ++q)
	{
	  have_indirect_rows = true;
	  internals::insert_index(my_indices, position->entries[q].first,
				  deal_II_numbers::invalid_unsigned_int,
				  std::make_pair<unsigned int,double> 
				  (local_row, position->entries[q].second));
	}

      Threads::ThreadMutex::ScopedLock lock(mutex);
      global_matrix.add(global_row,global_row,
			std::fabs(local_matrix(local_row,local_row)));
    }

  const unsigned int n_actual_dofs = my_indices.size();

  std::vector<unsigned int> localized_indices (n_actual_dofs);
  for (unsigned int i=0; i<n_actual_dofs; ++i)
    localized_indices[i] = my_indices[i].first;

				   // additional construct that also takes
				   // care of block indices.
  std::vector<unsigned int> block_ends(num_blocks, n_actual_dofs);
  {
    typedef std::vector<unsigned int>::iterator row_iterator;
    row_iterator col_indices = localized_indices.begin();
    unsigned int n_cols = n_actual_dofs;

				   // find end of rows.
    for (unsigned int i=0;i<num_blocks-1;++i)
      {
	row_iterator first_block = std::lower_bound (col_indices,
						     col_indices+n_cols,
						     global_matrix.get_row_indices().block_start(i+1));
	block_ends[i] = first_block - localized_indices.begin();
	n_cols -= (first_block - col_indices); 
	col_indices = first_block;
      }
    block_ends[num_blocks-1] = n_actual_dofs;

				   // transform row indices to local index
				   // space
    for (unsigned int i=block_ends[0]; i<localized_indices.size(); ++i)
      localized_indices[i] = global_matrix.get_row_indices().
	                       global_to_local(localized_indices[i]).second;
  }

  std::vector<unsigned int> actual_block_ends (num_blocks);

  std::vector<unsigned int> cols (n_actual_dofs);
  std::vector<double>       vals (n_actual_dofs);

  for (unsigned int i=0; i<n_actual_dofs; ++i)
    {
      unsigned int col_counter = 0;
      std::vector<unsigned int>::iterator block_it = block_ends.begin();
      const unsigned int row = my_indices[i].first;
      const unsigned int loc_row = my_indices[i].second.local_row;
      double val = 0;

      if (have_indirect_rows == false)
	{
	  Assert(loc_row >= 0 && loc_row < n_local_dofs,
		 ExcInternalError());

	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
				   // now comes the hack that sets the
				   // correct end of block in case we work
				   // with block matrices.
	      while (j == *block_it)
		{
		  actual_block_ends[block_it-block_ends.begin()] = col_counter;
		  ++block_it;
		  Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
			  ExcInternalError());
		}

	      const unsigned int loc_col = my_indices[j].second.local_row;
	      Assert(loc_col >= 0 && loc_col < n_local_dofs,
		     ExcInternalError());

	      if (local_matrix(loc_row,loc_col) != 0)
		{
		  vals[col_counter] = local_matrix(loc_row,loc_col);
		  cols[col_counter] = localized_indices[j];
		  col_counter++;
		}
	    }
				   // now comes the hack that sets the
				   // correct end of block in case we work
				   // with block matrices.
	  while (n_actual_dofs == *block_it && block_it != block_ends.end())
	    {
	      actual_block_ends[block_it-block_ends.begin()] = col_counter;
	      ++block_it;
	      Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
		      ExcInternalError());
	    }

	  if (use_vectors == true)
	    {
	      val = local_vector(loc_row);

	      for (unsigned int i=0; i<constraint_lines.size(); ++i)
		val -= constraint_lines[i].second->inhomogeneity *
		  local_matrix(loc_row,constraint_lines[i].first);
	    }
	}

      else
	{
	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
	      while (j == *block_it)
		{
		  actual_block_ends[block_it-block_ends.begin()] = col_counter;
		  ++block_it;
		  Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
			  ExcInternalError());
		}

	      double col_val;
	      const unsigned int loc_col = my_indices[j].second.local_row;

	      if (loc_row != deal_II_numbers::invalid_unsigned_int)
		{
		  Assert (loc_row >= 0 && loc_row < n_local_dofs,
			  ExcInternalError());

		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      col_val = local_matrix(loc_row,loc_col);
		    }
		  else
		    col_val = 0;

		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    col_val += local_matrix(loc_row,
					    my_indices[j].second.constraints[p].first) 
		               *
		               my_indices[j].second.constraints[p].second;
		}

	      else
		col_val = 0;

	      for (unsigned int q=0; q<my_indices[i].second.constraints.size(); ++q)
		{
		  double add_this;
		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      add_this = local_matrix(my_indices[i].second.constraints[q].first,
					      loc_col);
		    }
		  else
		    add_this = 0;
  
		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    add_this += local_matrix(my_indices[i].second.constraints[q].first,
					     my_indices[j].second.constraints[p].first) 
		                *
			        my_indices[j].second.constraints[p].second;
		  col_val += add_this * my_indices[i].second.constraints[q].second;
		}

		   

	      if (col_val != 0)
		{
		  cols[col_counter] = localized_indices[j];
		  vals[col_counter] = col_val;
		  col_counter++;
		}
	    }
	  while (n_actual_dofs == *block_it && block_it != block_ends.end())
	    {
	      actual_block_ends[block_it-block_ends.begin()] = col_counter;
	      ++block_it;
	      Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
		      ExcInternalError());
	    }

	  if (use_vectors == true)
	    {
	      if (loc_row != deal_II_numbers::invalid_unsigned_int)
		{
		  Assert (loc_row >= 0 && loc_row < n_local_dofs,
			  ExcInternalError());
		  val = local_vector(loc_row);
		  for (unsigned int i=0; i<constraint_lines.size(); ++i)
		    val -= constraint_lines[i].second->inhomogeneity *
		           local_matrix(loc_row,constraint_lines[i].first);
		}

	      for (unsigned int q=0; q < my_indices[i].second.constraints.size(); ++q)
		{
		  const unsigned int loc_row_q = my_indices[i].second.constraints[q].first;
		  double add_this = local_vector (loc_row_q);
		  for (unsigned int k=0; k<constraint_lines.size(); ++k)
		    add_this -= constraint_lines[k].second->inhomogeneity *
		                local_matrix(loc_row_q,constraint_lines[k].first);
		  val += add_this * my_indices[i].second.constraints[q].second;
		}
	    }
	}

				   // finally, write all the information
				   // that accumulated under the given
				   // process into the global matrix row and
				   // into the vector. For the block matrix,
				   // go trough the individual blocks and
				   // look which entries we need to set.
      Threads::ThreadMutex::ScopedLock lock(mutex);
      {
	unsigned int col_begins = 0;
	const std::pair<unsigned int,unsigned int> row_block = 
	  global_matrix.get_row_indices().global_to_local(row);
    
	for (unsigned int k=0; k<num_blocks; ++k)
	  {
	    const int current_length = actual_block_ends[k] - col_begins;
	    if (current_length > 0)
	      global_matrix.block(row_block.first, k).add(row_block.second, current_length,
							  &cols[col_begins], &vals[col_begins],
							  false, true);
	    col_begins = actual_block_ends[k];
	  }
      }
      if (val != 0)
	global_vector(row) += val;
    }
}



template <typename SparsityType>
inline
void
ConstraintMatrix::
add_entries_local_to_global (const std::vector<unsigned int> &local_dof_indices,
			     SparsityType                    &sparsity_pattern,
			     const bool                       keep_constrained_entries,
			     const Table<2,bool>             &dof_mask,
			     internal::bool2type<false> ) const
{
				   // similar to the function for distributing
				   // matrix entries.
  Assert (sparsity_pattern.n_rows() == sparsity_pattern.n_cols(), ExcNotQuadratic());

  const unsigned int n_local_dofs = local_dof_indices.size();
  bool dof_mask_is_active = false;
  if (dof_mask.n_rows() == n_local_dofs)
    {
      dof_mask_is_active = true;
      Assert (dof_mask.n_cols() == n_local_dofs,
	      ExcDimensionMismatch(dof_mask.n_cols(), n_local_dofs));
    }

  std::vector<std::pair<unsigned int,internals::distributing> > my_indices (n_local_dofs);

  std::vector<std::pair<unsigned int, const ConstraintLine *> > constraint_lines;
  constraint_lines.reserve(n_local_dofs);

				   // cache whether we have to resolve any
				   // indirect rows generated from resolving
				   // constrained dofs.
  bool have_indirect_rows = false;
  {
    unsigned int added_rows = 0;
				   // first add the indices in an unsorted
				   // way and only keep track of the
				   // constraints that appear. They are
				   // resolved in a second step.
    for (unsigned int i = 0; i<n_local_dofs; ++i)
      {
	if (constraint_line_exists.size() <= local_dof_indices[i] ||
	    constraint_line_exists[local_dof_indices[i]] == false)
	  {
	    my_indices[added_rows].first = local_dof_indices[i];
	    my_indices[added_rows].second.local_row = i;
	    ++added_rows;
	    continue;
	  }

	ConstraintLine index_comparison;
	index_comparison.line = local_dof_indices[i];

	const std::vector<ConstraintLine>::const_iterator
	  position = std::lower_bound (lines.begin(),
				       lines.end(),
				       index_comparison);
	Assert (position->line == local_dof_indices[i],
		ExcInternalError());

	constraint_lines.push_back (std::make_pair<unsigned int,
				    const ConstraintLine *>(i,&*position));
      }
    Assert (constraint_lines.size() + added_rows == n_local_dofs,
	    ExcInternalError());
    my_indices.resize (added_rows);
  }
  internals::list_shellsort (my_indices);

				   // now in the second step actually
				   // resolve the constraints
  const unsigned int n_constrained_dofs = constraint_lines.size();
  for (unsigned int i=0; i<n_constrained_dofs; ++i)
    {
      const unsigned int local_row = constraint_lines[i].first;
      const unsigned int global_row = local_dof_indices[local_row];
      const ConstraintLine * position = constraint_lines[i].second;
      for (unsigned int q=0; q<position->entries.size(); ++q)
	{
	  have_indirect_rows = true;
	  internals::insert_index(my_indices, position->entries[q].first,
				  deal_II_numbers::invalid_unsigned_int,
				  std::make_pair<unsigned int,double> 
				  (local_row, position->entries[q].second));
	}

      Threads::ThreadMutex::ScopedLock lock(mutex);

                                   // need to add the whole row and column
                                   // structure in case we keep constrained
                                   // entries. Unfortunately, we can't use
                                   // the nice matrix structure we use
                                   // elsewhere, so manually add those
                                   // indices one by one.
      if (keep_constrained_entries == true)
	{
	  for (unsigned int j=0; j<n_local_dofs; ++j)
	    {
	      bool add_this_ij = true, add_this_ji = true;
	      if (dof_mask_is_active == true)
		{
		  if (dof_mask[local_row][j] == false)
		    add_this_ij = false;
		  if (dof_mask[j][local_row] == false)
		    add_this_ji = false;
		}
	      if (add_this_ij == true)
		sparsity_pattern.add(global_row,
				     local_dof_indices[j]);
	      if (add_this_ji == true)
		sparsity_pattern.add(local_dof_indices[j],
				     global_row);
	    }
	}
      else
				   // don't keep constrained entries - just
				   // add the diagonal.
	sparsity_pattern.add(global_row,global_row);
    }

  const unsigned int n_actual_dofs = my_indices.size();

				   // create arrays for the column indices
				   // that will then be written into the
				   // sparsity pattern.
  std::vector<unsigned int> cols (n_actual_dofs);

				   // now do the actual job. 
  for (unsigned int i=0; i<n_actual_dofs; ++i)
    {
      unsigned int col_counter = 0;
      const unsigned int row = my_indices[i].first;
      const unsigned int loc_row = my_indices[i].second.local_row;

				   // fast function if there are no indirect
				   // references to any of the local rows at
				   // all on this set of dofs
      if (have_indirect_rows == false)
	{
	  Assert(loc_row >= 0 && loc_row < n_local_dofs,
		 ExcInternalError());

	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
	      const unsigned int loc_col = my_indices[j].second.local_row;
	      Assert(loc_col >= 0 && loc_col < n_local_dofs,
		     ExcInternalError());

	      bool add_this = true;
	      if (dof_mask_is_active == true)
		if (dof_mask[loc_row][loc_col] == false)
		  add_this = false;

	      if (add_this == true)
		cols[col_counter++] = my_indices[j].first;

	    }
	}

				   // slower functions when there are
				   // indirect references and when we need
				   // to do some more checks.
      else
	{
	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
	      const unsigned int loc_col = my_indices[j].second.local_row;

	      bool add_this = false;

				   // case 1: row has direct contribution in
				   // local matrix
	      if (loc_row != deal_II_numbers::invalid_unsigned_int)
		{
		  Assert (loc_row >= 0 && loc_row < n_local_dofs,
			  ExcInternalError());

				   // case 1a: col has direct contribution
				   // in local matrix
		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      if (dof_mask_is_active == true)
			{
			  if (dof_mask[loc_row][loc_col] == true)
			    goto add_this_index;
			}
		      else
			goto add_this_index;
		    }

				   // account for indirect contributions by
				   // constraints
		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    {
		      if (dof_mask_is_active == true)
			{
			  if (dof_mask[loc_row][my_indices[j].second.constraints[p].first] == true)
			    goto add_this_index;
			}
		      else
			goto add_this_index;
		    }
		}

				   // account for indirect contributions by
				   // constraints in row, going trough the
				   // direct and indirect references in the
				   // given column.
	      for (unsigned int q=0; q<my_indices[i].second.constraints.size(); ++q)
		{
		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      if (dof_mask_is_active == true)
			{
			  if (dof_mask[my_indices[i].second.constraints[q].first][loc_col] == true)
			    goto add_this_index;
			}
		      else
			goto add_this_index;
		    }
  
		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    if (dof_mask_is_active == true)
		      {
			if (dof_mask[my_indices[i].second.constraints[q].first]
			            [my_indices[j].second.constraints[p].first] == true)
			  goto add_this_index;
		      }
		    else
		      goto add_this_index;
		  }

				   // if we got some nontrivial value,
				   // append it to the array of values.
	      if (add_this == true)
		{
		  add_this_index:
		  cols[col_counter++] = my_indices[j].first;
		}
	    }
	}

				   // finally, write all the information
				   // that accumulated under the given
				   // process into the global matrix row and
				   // into the vector
      Threads::ThreadMutex::ScopedLock lock(mutex);
      if (col_counter > 0)
	sparsity_pattern.add_entries(row, cols.begin(), cols.begin()+col_counter);
    }
}




template <typename SparsityType>
inline
void
ConstraintMatrix::
add_entries_local_to_global (const std::vector<unsigned int> &local_dof_indices,
			     SparsityType                    &sparsity_pattern,
			     const bool                       keep_constrained_entries,
			     const Table<2,bool>             &dof_mask,
			     internal::bool2type<true> ) const
{
				   // just as the other
				   // add_entries_local_to_global function,
				   // but now specialized for block
				   // matrices.
  Assert (sparsity_pattern.n_rows() == sparsity_pattern.n_cols(), ExcNotQuadratic());
  Assert (sparsity_pattern.n_block_rows() == sparsity_pattern.n_block_cols(),
	  ExcNotQuadratic());

  const unsigned int n_local_dofs = local_dof_indices.size();
  const unsigned int num_blocks = sparsity_pattern.n_block_rows();

  bool dof_mask_is_active = false;
  if (dof_mask.n_rows() == n_local_dofs)
    {
      dof_mask_is_active = true;
      Assert (dof_mask.n_cols() == n_local_dofs,
	      ExcDimensionMismatch(dof_mask.n_cols(), n_local_dofs));
    }

  std::vector<std::pair<unsigned int,internals::distributing> > my_indices (n_local_dofs);

  std::vector<std::pair<unsigned int, const ConstraintLine *> > constraint_lines;
  constraint_lines.reserve(n_local_dofs);

				   // cache whether we have to resolve any
				   // indirect rows generated from resolving
				   // constrained dofs.
  bool have_indirect_rows = false;
  {
    unsigned int added_rows = 0;
				   // first add the indices in an unsorted
				   // way and only keep track of the
				   // constraints that appear. They are
				   // resolved in a second step.
    for (unsigned int i = 0; i<n_local_dofs; ++i)
      {
	if (constraint_line_exists.size() <= local_dof_indices[i] ||
	    constraint_line_exists[local_dof_indices[i]] == false)
	  {
	    my_indices[added_rows].first = local_dof_indices[i];
	    my_indices[added_rows].second.local_row = i;
	    ++added_rows;
	    continue;
	  }

	ConstraintLine index_comparison;
	index_comparison.line = local_dof_indices[i];

	const std::vector<ConstraintLine>::const_iterator
	  position = std::lower_bound (lines.begin(),
				       lines.end(),
				       index_comparison);
	Assert (position->line == local_dof_indices[i],
		ExcInternalError());

	constraint_lines.push_back (std::make_pair<unsigned int,
				    const ConstraintLine *>(i,&*position));
      }
    Assert (constraint_lines.size() + added_rows == n_local_dofs,
	    ExcInternalError());
    my_indices.resize (added_rows);
  }
  internals::list_shellsort (my_indices);

				   // now in the second step actually
				   // resolve the constraints
  const unsigned int n_constrained_dofs = constraint_lines.size();
  for (unsigned int i=0; i<n_constrained_dofs; ++i)
    {
      const unsigned int local_row = constraint_lines[i].first;
      const unsigned int global_row = local_dof_indices[local_row];
      const ConstraintLine * position = constraint_lines[i].second;
      for (unsigned int q=0; q<position->entries.size(); ++q)
	{
	  have_indirect_rows = true;
	  internals::insert_index(my_indices, position->entries[q].first,
				  deal_II_numbers::invalid_unsigned_int,
				  std::make_pair<unsigned int,double> 
				  (local_row, position->entries[q].second));
	}

      Threads::ThreadMutex::ScopedLock lock(mutex);

                                   // need to add the whole row and column
                                   // structure in case we keep constrained
                                   // entries. Unfortunately, we can't use
                                   // the nice matrix structure we use
                                   // elsewhere, so manually add those
                                   // indices one by one.
      if (keep_constrained_entries == true)
	{
	  for (unsigned int j=0; j<n_local_dofs; ++j)
	    {
	      bool add_this_ij = true, add_this_ji = true;
	      if (dof_mask_is_active == true)
		{
		  if (dof_mask[local_row][j] == false)
		    add_this_ij = false;
		  if (dof_mask[j][local_row] == false)
		    add_this_ji = false;
		}
	      if (add_this_ij == true)
		sparsity_pattern.add(global_row,
				     local_dof_indices[j]);
	      if (add_this_ji == true)
		sparsity_pattern.add(local_dof_indices[j],
				     global_row);
	    }
	}
      else
				   // don't keep constrained entries - just
				   // add the diagonal.
	sparsity_pattern.add(global_row,global_row);
    }


  const unsigned int n_actual_dofs = my_indices.size();

  std::vector<unsigned int> localized_indices (n_actual_dofs);
  for (unsigned int i=0; i<n_actual_dofs; ++i)
    localized_indices[i] = my_indices[i].first;

				   // additional construct that also takes
				   // care of block indices.
  std::vector<unsigned int> block_ends(num_blocks, n_actual_dofs);
  {
    typedef std::vector<unsigned int>::iterator row_iterator;
    row_iterator col_indices = localized_indices.begin();
    unsigned int n_cols = n_actual_dofs;

				   // find end of rows.
    for (unsigned int i=0;i<num_blocks-1;++i)
      {
	row_iterator first_block = std::lower_bound (col_indices,
						     col_indices+n_cols,
						     sparsity_pattern.get_row_indices().block_start(i+1));
	block_ends[i] = first_block - localized_indices.begin();
	n_cols -= (first_block - col_indices); 
	col_indices = first_block;
      }
    block_ends[num_blocks-1] = localized_indices.size();

				   // transform row indices to local index
				   // space
    for (unsigned int i=block_ends[0]; i<localized_indices.size(); ++i)
      localized_indices[i] = sparsity_pattern.get_row_indices().
	                       global_to_local(localized_indices[i]).second;
  }
  std::vector<unsigned int> actual_block_ends (num_blocks);

  std::vector<unsigned int> cols (n_actual_dofs);

  for (unsigned int i=0; i<n_actual_dofs; ++i)
    {
      unsigned int col_counter = 0;
      std::vector<unsigned int>::iterator block_it = block_ends.begin();
      const unsigned int row = my_indices[i].first;
      const unsigned int loc_row = my_indices[i].second.local_row;

      if (have_indirect_rows == false)
	{
	  Assert(loc_row >= 0 && loc_row < n_local_dofs,
		 ExcInternalError());

	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {
				   // now comes the hack that sets the
				   // correct end of block in case we work
				   // with block matrices.
	      while (j == *block_it)
		{
		  actual_block_ends[block_it-block_ends.begin()] = col_counter;
		  ++block_it;
		  Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
			  ExcInternalError());
		}

	      const unsigned int loc_col = my_indices[j].second.local_row;
	      Assert(loc_col >= 0 && loc_col < n_local_dofs,
		     ExcInternalError());

	      bool add_this = true;
	      if (dof_mask_is_active == true)
		if (dof_mask[loc_row][loc_col] == false)
		  add_this = false;

	      if (add_this == true)
		cols[col_counter++] = localized_indices[j];
	    }

				   // now comes the hack that sets the
				   // correct end of block in case we work
				   // with block matrices.
	  while (n_actual_dofs == *block_it && block_it != block_ends.end())
	    {
	      actual_block_ends[block_it-block_ends.begin()] = col_counter;
	      ++block_it;
	      Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
		      ExcInternalError());
	    }
	}

				   // have indirect references by
				   // constraints, resolve them
      else
	{
	  for (unsigned int j=0; j < n_actual_dofs; ++j)
	    {

				   // now comes the hack that sets the
				   // correct end of block in case we work
				   // with block matrices.
	      while (j == *block_it)
		{
		  actual_block_ends[block_it-block_ends.begin()] = col_counter;
		  ++block_it;
		  Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
			  ExcInternalError());
		}

	      const unsigned int loc_col = my_indices[j].second.local_row;

	      bool add_this = false;

	      if (loc_row != deal_II_numbers::invalid_unsigned_int)
		{
		  Assert (loc_row >= 0 && loc_row < n_local_dofs,
			  ExcInternalError());

		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      if (dof_mask_is_active == true)
			{
			  if (dof_mask[loc_row][loc_col] == true)
			    goto add_this_index;
			}
		      else
			goto add_this_index;
		    }

		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    {
		      if (dof_mask_is_active == true)
			{
			  if (dof_mask[loc_row][my_indices[j].second.constraints[p].first] == true)
			    goto add_this_index;
			}
		      else
			goto add_this_index;
		    }
		}

	      for (unsigned int q=0; q<my_indices[i].second.constraints.size(); ++q)
		{
		  if (loc_col != deal_II_numbers::invalid_unsigned_int)
		    {
		      Assert (loc_col >= 0 && loc_col < n_local_dofs,
			      ExcInternalError());
		      if (dof_mask_is_active == true)
			{
			  if (dof_mask[my_indices[i].second.constraints[q].first][loc_col] == true)
			    goto add_this_index;
			}
		      else
			goto add_this_index;
		    }
  
		  for (unsigned int p=0; p<my_indices[j].second.constraints.size(); ++p)
		    if (dof_mask_is_active == true)
		      {
			if (dof_mask[my_indices[i].second.constraints[q].first]
			            [my_indices[j].second.constraints[p].first] == true)
			  goto add_this_index;
		      }
		    else
		      goto add_this_index;
		}

	      if (add_this == true)
		{
		  add_this_index:
		  cols[col_counter++] = localized_indices[j];
		}
	    }
				   // now comes the hack that sets the
				   // correct end of block in case we work
				   // with block matrices.
	  while (n_actual_dofs == *block_it && block_it != block_ends.end())
	    {
	      actual_block_ends[block_it-block_ends.begin()] = col_counter;
	      ++block_it;
	      Assert ((unsigned int)(block_it - block_ends.begin()) <= block_ends.size(),
		      ExcInternalError());
	    }
	}

				   // finally, write all the information
				   // that accumulated under the given
				   // process into the global matrix row and
				   // into the vector
      Threads::ThreadMutex::ScopedLock lock(mutex);
      {
	unsigned int col_begins = 0;
	const std::pair<unsigned int,unsigned int> row_block = 
	  sparsity_pattern.get_row_indices().global_to_local(row);
    
	for (unsigned int i=0; i<num_blocks; ++i)
	  {
	    const int current_length = actual_block_ends[i] - col_begins;
	    if (current_length > 0)
	      sparsity_pattern.block(row_block.first, i).add_entries(row_block.second, 
								     cols.begin()+col_begins, 
								     cols.begin()+col_begins+current_length);
	    col_begins = actual_block_ends[i];
	  }
      }
    }
}


DEAL_II_NAMESPACE_CLOSE

#endif
