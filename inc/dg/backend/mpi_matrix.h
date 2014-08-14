#pragma once

#include <vector>

//#include "derivatives.cuh"
#include "mpi_grid.h"
#include "mpi_vector.h"
#include "mpi_precon_blas.h"
#include "operator.h"


namespace dg
{
struct BoundaryTerms
{
    std::vector<std::vector<double> > data_;
    std::vector<int> row_; //row of data in 1D without block
    std::vector<int> col_;
    void applyX( const MPI_Vector& x, MPI_Vector& y) const
    {
        if(data_.empty()) return;
        unsigned rows = x.Ny(), cols = x.Nx(), n = x.n();
        for( unsigned m=0; m<data_.size(); m++) //all blocks
        {
            for( unsigned s=0; s<x.Nz(); s++) //z-loop
            for( unsigned i=1; i<rows-1; i++) //y-loop
            for( unsigned k=0; k<n; k++)
            for( unsigned l=0; l<n; l++)
            for( unsigned q=0; q<n; q++) //multiplication-loop
            {
                y.data()[(((s*rows+i)*n+k)*cols + row_[m]+1)*n +l] = 0;
            }
        }
        for( unsigned m=0; m<data_.size(); m++) //all blocks
        {
            for( unsigned s=0; s<x.Nz(); s++) //z-loop
            for( unsigned i=1; i<rows-1; i++) //y-loop
            for( unsigned k=0; k<n; k++)
            for( unsigned l=0; l<n; l++)
            for( unsigned q=0; q<n; q++) //multiplication-loop
            {
                y.data()[(((s*rows+i)*n+k)*cols + row_[m]+1)*n +l] += 
                    data_[m][l*n+q]
                    *x.data()[(((s*rows+i)*n+k)*cols + col_[m]+1)*n + q ];
            }
        }
    }

    void applyY( const MPI_Vector& x, MPI_Vector& y) const
    {
        if(data_.empty()) return;
        unsigned rows = x.Ny(), cols = x.Nx(), n = x.n();
        for( unsigned m=0; m<data_.size(); m++) //all blocks
        {
            for( unsigned s=0; s<x.Nz(); s++)//z-loop
            for( unsigned k=0; k<n; k++)
            for( unsigned j=1; j<cols-1; j++) //x-loop
            for( unsigned l=0; l<n; l++)
            for( unsigned p=0; p<n; p++)
            {
                y.data()[(((s*rows+row_[m]+1)*n+k)*cols + j)*n +l] = 0;
            }
        }
        for( unsigned m=0; m<data_.size(); m++) //all blocks
        {
            for( unsigned s=0; s<x.Nz(); s++)//z-loop
            for( unsigned k=0; k<n; k++)
            for( unsigned j=1; j<cols-1; j++) //x-loop
            for( unsigned l=0; l<n; l++)
            for( unsigned p=0; p<n; p++)
            {
                y.data()[(((s*rows+row_[m]+1)*n+k)*cols + j)*n +l] += 
                     data_[m][k*n+p]
                    *x.data()[(((s*rows+col_[m]+1)*n+p)*cols + j)*n + l];
            }
        }
    }
};

/**
 * @brief Matrix class for block matrices for 2D and 3D derivatives in X and Y direction
 *
 * Stores only one line of blocks and takes care of updating
 * ghost cells before being applied to vectors.
 */
struct MPI_Matrix
{
    MPI_Matrix( bc bcx, MPI_Comm comm, unsigned number): 
        dataY_(number), dataX_(number), offset_(number, 0), 
        bcx_( bcx), bcy_( dg::PER), comm_(comm){ }
    MPI_Matrix( bc bcx, bc bcy, MPI_Comm comm, unsigned number): 
        dataY_(number), dataX_(number), offset_(number, 0), 
        bcx_( bcx), bcy_( bcy), comm_(comm){ }
    bc& bcx(){return bcx_;}
    bc& bcy(){return bcy_;}
    const bc& bcx()const{return bcx_;}
    const bc& bcy()const{return bcy_;}

    MPI_Comm communicator()const{return comm_;}

    void update_boundaryX( MPI_Vector& v) const;
    void update_boundaryY( MPI_Vector& v) const;

    std::vector<std::vector<double> >& dataY()    {return dataY_;}
    std::vector<std::vector<double> >& dataX()    {return dataX_;}
    std::vector<int>&                  offset()  {return offset_;}
    BoundaryTerms& xterm() {return xterm_;}
    BoundaryTerms& yterm() {return yterm_;}
    MPI_Precon& precond() {return p_;}
    //const std::vector<std::vector<double> >& dataY()const {return dataY_;}
    //const std::vector<std::vector<double> >& dataX()const {return dataX_;}
    //const std::vector<int>& offset()const {return offset_;}
    //const MPI_Precon precond()const {return p_;}

    void symv( MPI_Vector& x, MPI_Vector& y) const;
  private:
    MPI_Precon p_;
    std::vector<std::vector<double> > dataY_;
    std::vector<std::vector<double> > dataX_;
    std::vector<int> offset_;
    BoundaryTerms xterm_;
    BoundaryTerms yterm_;
    bc bcx_, bcy_;
    MPI_Comm comm_;
};


typedef MPI_Matrix MMatrix;
void MPI_Matrix::symv( MPI_Vector& x, MPI_Vector& y) const
{
    int rank;
    MPI_Comm_rank(comm_, &rank);
    bool updateX = false, updateY = false;
    for( unsigned k=0; k<dataX_.size(); k++)
    {
        if( !dataY_[k].empty() )
            updateY = true;
        if( !dataX_[k].empty() )
            updateX = true;
    }
    if( updateX )
        x.x_col(comm_); 
    if( updateY) 
        x.x_row(comm_);
#ifdef DG_DEBUG
    assert( x.data().size() == y.data().size() );
#endif //DG_DEBUG
    unsigned rows = x.Ny(), cols = x.Nx(), n = x.n();
    for( unsigned i=0; i<y.data().size(); i++)
        y.data()[i] = 0;
    for( unsigned m=0; m<dataX_.size(); m++) //all blocks
    {
        if( !dataX_[m].empty())
            for( unsigned s=0; s<x.Nz(); s++) //z-loop
            for( unsigned i=1; i<rows-1; i++) //y-loop
            for( unsigned k=0; k<n; k++)
            for( unsigned j=1; j<cols-1; j++) //x-loop
            for( unsigned l=0; l<n; l++)
            for( unsigned q=0; q<n; q++) //multiplication-loop
            {
                y.data()[(((s*rows+i)*n+k)*cols + j)*n +l] += 
                    dataX_[m][l*n+q]
                    *x.data()[(((s*rows+i)*n+k)*cols + j)*n + q + offset_[m]];
            }
        if( !dataY_[m].empty())
            for( unsigned s=0; s<x.Nz(); s++)
            for( unsigned i=1; i<rows-1; i++)
            for( unsigned k=0; k<n; k++)
            for( unsigned j=1; j<cols-1; j++)
            for( unsigned l=0; l<n; l++)
            for( unsigned p=0; p<n; p++)
            {
                y.data()[(((s*rows+i)*n+k)*cols + j)*n +l] += 
                     dataY_[m][k*n+p]
                    *x.data()[(((s*rows+i)*n+p)*cols + j)*n + l + offset_[m]];
            }
    }
    xterm_.applyX( x,y);
    yterm_.applyY( x,y);
    if( !p_.data.empty())
        dg::blas2::detail::doSymv( p_, y, y, MPIPreconTag(), MPIVectorTag(), MPIVectorTag());

}

void MPI_Matrix::update_boundaryX( MPI_Vector& v)const
{
    v.x_col(comm_); //update data in overlapping cells
    //int rank;
    //MPI_Comm_rank(comm_, &rank);
    if( bcx_ == PER) return;
    int low_sign(0), upp_sign(0);
    //if( bcx_ == DIR)
    //    low_sign=upp_sign=-0;
    //    //low_sign=upp_sign=-1;
    //else if( bcx_ == NEU)
    //    low_sign=upp_sign=+1;
    //else if( bcx_ == DIR_NEU)
    //    low_sign=-1, upp_sign=+1;
    //else if( bcx_ == NEU_DIR)
    //    low_sign=+1, upp_sign=-1;
    int ndims;
    MPI_Cartdim_get( comm_, &ndims);
    int dims[ndims], periods[ndims], coords[ndims];
    MPI_Cart_get( comm_, ndims, dims, periods, coords);
    unsigned rows = v.Nz()*v.Ny()*v.n(), cols =v.Nx(), n = v.n();
    if( coords[0] == dims[0]-1)
        for( unsigned i=0; i<rows; i++)
            for( unsigned j=0; j<n; j++)
                v.data()[(i*cols + cols-1)*n+j] = 1e6;
                    //(double)upp_sign*v.data()[(i*cols + cols-2)*n + n-j-1];
    if( coords[0] == 0) //both ifs may be true
        for( unsigned i=0; i<rows; i++)
            for( unsigned j=0; j<n; j++)
                v.data()[(i*cols + 0)*n+j] = 1e6;
                    //(double)low_sign*v.data()[(i*cols+1)*n+ n-j-1];
    return;
}
void MPI_Matrix::update_boundaryY( MPI_Vector& v)const
{
    v.x_row(comm_);
    if( bcy_ == PER) return;
    int low_sign(0), upp_sign(0);
    //if( bcy_ == DIR)
    //    low_sign=upp_sign=-0;
    //    //low_sign=upp_sign=-1;
    //else if( bcy_ == NEU)
    //    low_sign=upp_sign=+1;
    //else if( bcy_ == DIR_NEU)
    //    low_sign=-1, upp_sign=+1;
    //else if( bcy_ == NEU_DIR)
    //    low_sign=+1, upp_sign=-1;
    int ndims;
    MPI_Cartdim_get( comm_, &ndims);
    int dims[ndims], periods[ndims], coords[ndims];
    MPI_Cart_get( comm_, ndims, dims, periods, coords);
    unsigned cols =v.Nx()*v.n(), n = v.n();
    if( coords[1] == dims[1]-1)
        for( unsigned s=0; s<v.Nz(); s++)
            for( unsigned k=0; k<n; k++)
                for( unsigned j=0; j<cols; j++)
                    v.data()[((s*v.Ny() + v.Ny()-1)*n+k)*cols + j] = 1e6;
                        //upp_sign*v.data()[((s*v.Ny() + v.Ny() -2)*n+n-k-1)*cols + j];
    if( coords[1] == 0) //both ifs may be true
        for( unsigned s=0; s<v.Nz(); s++)
            for( unsigned k=0; k<n; k++)
                for( unsigned j=0; j<cols; j++)
                    v.data()[((s*v.Ny() + 0)*n + k)*cols + j] = 1e5;
                        //low_sign*v.data()[((s*v.Ny() + 1)*n + n-k-1)*cols+j];
    return;
}


template <>
struct MatrixTraits<MPI_Matrix>
{
    typedef double value_type;
    typedef MPIMatrixTag matrix_category;
};
template <>
struct MatrixTraits<const MPI_Matrix>
{
    typedef double value_type;
    typedef MPIMatrixTag matrix_category;
};


} //namespace dg
