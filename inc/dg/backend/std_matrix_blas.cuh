#pragma once


#include "std_vector_blas.cuh"
#include "matrix_categories.h"
///@cond
namespace dg
{
namespace blas2
{
namespace detail
{

template< class Matrix, class Vector>
inline void doSymv( 
              Matrix& m,
              const std::vector<Vector>& x, 
              std::vector<Vector>& y, 
              AnyMatrixTag,
              StdVectorTag,
              StdVectorTag)
{
#ifdef DG_DEBUG
    assert( x.size() == y.size() );
    //assert( m.size() == y.size() );
#endif //DG_DEBUG
    for( unsigned i=0; i<x.size(); i++)
        doSymv( m, x[i], y[i], 
                       typename dg::MatrixTraits<Matrix>::matrix_category(),
                       typename dg::VectorTraits<Vector>::vector_category(),
                       typename dg::VectorTraits<Vector>::vector_category() );
        
}

template< class Precon, class Vector>
inline void doSymv( 
              typename MatrixTraits<Precon>::value_type alpha,
              const Precon& m,
              const std::vector<Vector>& x, 
              typename MatrixTraits<Precon>::value_type beta,
              std::vector<Vector>& y, 
              AnyMatrixTag,
              StdVectorTag)
{
#ifdef DG_DEBUG
    assert( x.size() == y.size() );
    //assert( m.size() == y.size() );
#endif //DG_DEBUG
    for( unsigned i=0; i<x.size(); i++)
        doSymv( alpha, m, x[i], beta, y[i],
                       typename dg::MatrixTraits<Precon>::matrix_category(),
                       typename dg::VectorTraits<Vector>::vector_category() );
}

template< class Matrix, class Vector>
inline typename MatrixTraits<Matrix>::value_type  doDot( 
              const std::vector<Vector>& x, 
              const Matrix& m,
              const std::vector<Vector>& y, 
              AnyMatrixTag,
              StdVectorTag)
{
#ifdef DG_DEBUG
    assert( x.size() == y.size() );
#endif //DG_DEBUG
    std::vector<exblas::Superaccumulator> acc( x.size());
    for( unsigned i=0; i<x.size(); i++)
        acc[i] = doDot_dispatch( x[i], m, y[i],
                       typename dg::MatrixTraits<Matrix>::matrix_category(),
                       typename dg::VectorTraits<Vector>::vector_category() );
    for( unsigned i=1; i<x.size(); i++)
        acc[0].Accumulate( acc[i]);
    return acc[0].Round();
}
template< class Matrix, class Vector>
inline typename VectorTraits<Vector>::value_type  doDot( 
              const Matrix& m,
              const std::vector<Vector>& y, 
              AnyMatrixTag,
              StdVectorTag)
{
    return doDot( y,m,y,AnyMatrixTag(),StdVectorTag());
}


} //namespace detail
} //namespace blas1
} //namespace dg
///@endcond
