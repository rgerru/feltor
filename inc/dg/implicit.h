#pragma once
#include "cg.h"

namespace dg{
///@cond
namespace detail{

//compute: y + alpha f(y,t)
template< class LinearOp, class ContainerType>
struct Implicit
{
    using value_type = get_value_type<ContainerType>;
    Implicit( value_type alpha, value_type t, LinearOp& f): f_(f), alpha_(alpha), t_(t){}
    void symv( const ContainerType& x, ContainerType& y)
    {
        if( alpha_ != 0)
            f_(t_,x,y);
        blas1::axpby( 1., x, alpha_, y, y);
        blas2::symv( f_.weights(), y, y);
    }
    //compute without weights
    void operator()( const ContainerType& x, ContainerType& y)
    {
        if( alpha_ != 0)
            f_(t_,x,y);
        blas1::axpby( 1., x, alpha_, y, y);
    }
    value_type& alpha( ){  return alpha_;}
    value_type alpha( ) const  {return alpha_;}
    value_type& time( ){  return t_;}
    value_type time( ) const  {return t_;}
  private:
    LinearOp& f_;
    value_type alpha_;
    value_type t_;
};

}//namespace detail
template< class M, class V>
struct TensorTraits< detail::Implicit<M, V> >
{
    using value_type = get_value_type<V>;
    using tensor_category = SelfMadeMatrixTag;
};
///@endcond

/*! @class hide_SolverType
 *
 * @tparam SolverType
    The task of this class is to solve the equation \f$ (y+\alpha\hat I(t,y)) = \rho\f$
    for the given implicit part I, parameter alpha, time t and
    right hand side rho. For example \c dg::DefaultSolver
    If you write your own class:
 * it must have a solve method of type:
    \c void \c solve( value_type alpha, Implicit im, value_type t, ContainerType& y, const ContainerType& rhs);
  The <tt> const ContainerType& copyable() const; </tt> member must return a container of the size that is later used in \c solve
  (it does not matter what values \c copyable contains, but its size is important;
  the \c solve method will be called with vectors of this size)
 */

/*!@brief Default Solver class for solving \f[ (y+\alpha\hat I(t,y)) = \rho\f]
 *
 * works only for linear positive definite operators as it uses a conjugate
 * gradient solver to invert the equation
 * @copydoc hide_ContainerType
 * @sa Karniadakis ARKStep
 */
template<class ContainerType>
struct DefaultSolver
{
    using container_type = ContainerType;
    using value_type = get_value_type<ContainerType>;//!< value type of vectors
    ///No memory allocation
    DefaultSolver(){}
    /*!
    * @param copyable vector of the size that is later used in \c solve (
     it does not matter what values \c copyable contains, but its size is important;
     the \c solve method can only be called with vectors of the same size)
    * @param max_iter maimum iteration number in cg
    * @param eps accuracy parameter for cg
    */
    DefaultSolver( const ContainerType& copyable, unsigned max_iter, value_type eps):
        m_pcg(copyable, max_iter), m_rhs( copyable), m_eps(eps)
        {}
    ///@brief Return an object of same size as the object used for construction
    ///@return A copyable object; what it contains is undefined, its size is important
    const ContainerType& copyable()const{ return m_rhs;}

    template< class Implicit>
    void solve( value_type alpha, Implicit im, value_type t, ContainerType& y, const ContainerType& rhs)
    {
        detail::Implicit<Implicit, ContainerType> implicit( alpha, t, im);
        blas2::symv( im.weights(), rhs, m_rhs);
#ifdef DG_BENCHMARK
#ifdef MPI_VERSION
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif//MPI
        Timer ti;
        ti.tic();
        unsigned number = m_pcg( implicit, y, m_rhs, im.precond(), im.inv_weights(), m_eps);
        ti.toc();
#ifdef MPI_VERSION
        if(rank==0)
#endif//MPI
        std::cout << "# of pcg iterations time solver: "<<number<<"/"<<m_pcg.get_max()<<" took "<<ti.diff()<<"s\n";
#else
        m_pcg( implicit, y, m_rhs, im.precond(), im.inv_weights(), m_eps);
#endif //DG_BENCHMARK
    }
    private:
    CG< ContainerType> m_pcg;
    ContainerType m_rhs;
    value_type m_eps;
};
}//namespace dg
