#pragma once

#include "blas.h"
#include "enums.h"
#include "backend/memory.h"
#include "geometry/evaluation.h"
#include "geometry/derivatives.h"
#ifdef MPI_VERSION
#include "geometry/mpi_derivatives.h"
#include "geometry/mpi_evaluation.h"
#endif
#include "geometry/geometry.h"

/*! @file

  @brief General negative elliptic operators
  */
namespace dg
{

/**
 * @brief A 2d negative elliptic differential operator
 *
 * @ingroup matrixoperators
 *
 * The term discretized is \f[ -\nabla \cdot ( \chi \nabla_\perp ) \f]
 * where \f$ \nabla_\perp \f$ is the two-dimensional gradient and \f$\chi\f$ is a
 * (possibly spatially dependent) tensor.
 * In general coordinates that means
 * \f[ -\frac{1}{\sqrt{g}}\left(
 * \partial_x\left(\sqrt{g}\left(\chi^{xx}\partial_x + \chi^{xy}\partial_y \right)\right)
 + \partial_y\left(\sqrt{g} \left(\chi^{yx}\partial_x + \chi^{yy}\partial_y \right)\right) \right)\f]
 is discretized.
 Per default, \f$ \chi\f$ is the metric tensor but you can set it to any tensor
 you like (in order for the operator to be invertible \f$\chi\f$ should be
 symmetric and positive definite though).
 Note that the local discontinuous Galerkin discretization adds so-called jump terms
 \f[ D^\dagger \chi D + \alpha J \f]
 where \f$\alpha\f$  is a scale factor ( = jfactor), \f$ D \f$ contains the discretizations of the above derivatives, and \f$ J\f$ is a self-adjoint matrix.
 (The symmetric part of \f$J\f$ is added @b before the volume element is divided). The adjoint of a matrix is defined with respect to the volume element including dG weights.
 Usually, the default \f$ \alpha=1 \f$ is a good choice.
 However, in some cases, e.g. when \f$ \chi \f$ exhibits very large variations
 \f$ \alpha=0.1\f$ or \f$ \alpha=0.01\f$ might be better values.
 In a time dependent problem the value of \f$\alpha\f$ determines the
 numerical diffusion, i.e. for too low values numerical oscillations may appear.
 Also note that a forward discretization has more diffusion than a centered discretization.

 The following code snippet demonstrates the use of \c Elliptic in an inversion problem
 * @snippet elliptic2d_b.cu invert
 * @copydoc hide_geometry_matrix_container
 * This class has the \c SelfMadeMatrixTag so it can be used in blas2::symv functions
 * and thus in a conjugate gradient solver.
 * @note The constructors initialize \f$ \chi=1\f$ so that a negative laplacian operator
 * results
 * @note The inverse of \f$ \chi\f$ makes a good general purpose preconditioner
 * @note the jump term \f$ \alpha J\f$  adds artificial numerical diffusion as discussed above
 * @attention Pay attention to the negative sign which is necessary to make the matrix @b positive @b definite
 *
 */
template <class Geometry, class Matrix, class container>
class Elliptic
{
    public:
    using value_type = get_value_type<container>;
    ///@brief empty object ( no memory allocation, call \c construct before using the object)
    Elliptic(){}
    /**
     * @brief Construct from Grid
     *
     * @param g The Grid, boundary conditions are taken from here
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative

     * @param jfactor (\f$ = \alpha \f$ ) scale jump terms (1 is a good value but in some cases 0.1 or 0.01 might be better)
     * @note chi is assumed 1 per default
     */
    Elliptic( const Geometry& g, norm no = not_normed, direction dir = forward, value_type jfactor=1.)
    {
        construct( g, g.bcx(), g.bcy(), no, dir, jfactor);
    }

    ///@copydoc Elliptic::construct()
    Elliptic( const Geometry& g, bc bcx, bc bcy, norm no = not_normed, direction dir = forward, value_type jfactor=1.)
    {
        construct( g, bcx, bcy, no, dir, jfactor);
    }

    /**
     * @brief Construct from grid and boundary conditions
     * @param g The Grid
     * @param bcx boundary condition in x
     * @param bcy boundary contition in y
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative (i.e. forward, backward or centered)
     * @param jfactor scale jump terms (1 is a good value but in some cases 0.1 or 0.01 might be better)
     */
    void construct( const Geometry& g, bc bcx, bc bcy, norm no = not_normed, direction dir = forward, value_type jfactor = 1.)
    {
        m_no=no, m_jfactor=jfactor;
        dg::blas2::transfer( dg::create::dx( g, inverse( bcx), inverse(dir)), m_leftx);
        dg::blas2::transfer( dg::create::dy( g, inverse( bcy), inverse(dir)), m_lefty);
        dg::blas2::transfer( dg::create::dx( g, bcx, dir), m_rightx);
        dg::blas2::transfer( dg::create::dy( g, bcy, dir), m_righty);
        dg::blas2::transfer( dg::create::jumpX( g, bcx),   m_jumpX);
        dg::blas2::transfer( dg::create::jumpY( g, bcy),   m_jumpY);

        dg::assign( dg::create::inv_volume(g),    m_inv_weights);
        dg::assign( dg::create::volume(g),        m_weights);
        dg::assign( dg::create::inv_weights(g),   m_precond);
        m_temp = m_tempx = m_tempy = m_inv_weights;
        m_chi=g.metric();
        m_vol=dg::tensor::volume(m_chi);
        dg::tensor::scal( m_chi, m_vol);
        dg::assign( dg::create::weights(g), m_weights_wo_vol);
        dg::assign( dg::evaluate(dg::one, g), m_sigma);
    }

    ///@copydoc  Elliptic::Elliptic(const Geometry&,norm,direction,value_type)
    void construct( const Geometry& g, norm no = not_normed, direction dir = forward, value_type jfactor = 1.){
        construct( g, g.bcx(), g.bcy(), no, dir, jfactor);
    }

    /**
     * @brief Change scalar part in Chi tensor
     *
     * Internally, we split the tensor \f$\chi = \sigma\mathbf{\tau}\f$ into
     * a scalar part \f$ \sigma\f$ and a tensor part \f$ \tau\f$ and you can
     * set each part seperately. This functions sets the scalar part.
     *
     * @param sigma The new scalar part in \f$\chi\f$ (all elements must be >0)
     * @tparam ContainerType0 must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_chi( const ContainerType0& sigma)
    {
        dg::blas1::pointwiseDivide( sigma, m_sigma, m_tempx);
        //update preconditioner
        dg::blas1::pointwiseDivide( m_precond, m_tempx, m_precond);
        dg::tensor::scal( m_chi, m_tempx);
        dg::blas1::copy( sigma, m_sigma);
    }
    /**
     * @brief Change tensor part in Chi tensor
     *
     * Internally, we split the tensor \f$\chi = \sigma\mathbf{\tau}\f$ into
     * a scalar part \f$ \sigma\f$ and a tensor part \f$ \tau\f$ and you can
     * set each part seperately. This functions sets the tensor part.
     *
     * @param tau The new tensor part in \f$\chi\f$ (must be positive definite)
     * @tparam ContainerType0 must be usable in \c dg::assign to \c container
     * @note the 3d parts in \c tau will be ignored
     */
    template<class ContainerType0>
    void set_chi( const SparseTensor<ContainerType0>& tau)
    {
        m_chi = SparseTensor<container>(tau);
        dg::tensor::scal( m_chi, m_sigma);
        dg::tensor::scal( m_chi, m_vol);
    }

    /**
     * @brief Return the vector missing in the un-normed symmetric matrix
     *
     * i.e. the inverse of the weights() function
     * @return inverse volume form including inverse weights
     */
    const container& inv_weights()const {
        return m_inv_weights;
    }
    /**
     * @brief Return the vector making the matrix symmetric
     *
     * i.e. the volume form
     * @return volume form including weights
     */
    const container& weights()const {
        return m_weights;
    }
    /**
     * @brief Return the default preconditioner to use in conjugate gradient
     *
     * Currently returns the inverse weights without volume elment divided by the scalar part of \f$ \chi\f$.
     * This is especially good when \f$ \chi\f$ exhibits large amplitudes or variations
     * @return the inverse of \f$\chi\f$.
     */
    const container& precond()const {
        return m_precond;
    }
    /**
     * @brief Set the currently used jfactor
     *
     * @param new_jfactor The new scale factor for jump terms
     */
    void set_jfactor( value_type new_jfactor) {m_jfactor = new_jfactor;}
    /**
     * @brief Get the currently used jfactor
     *
     * @return  The current scale factor for jump terms
     */
    value_type get_jfactor() const {return m_jfactor;}

    /**
     * @brief Computes the polarisation term
     *
     * @param x left-hand-side
     * @param y result
     * @note memops required:
            - 19 reads + 9 writes
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0, class ContainerType1>
    void symv( const ContainerType0& x, ContainerType1& y){
        symv( 1, x, 0, y);
    }
    /**
     * @brief Computes the polarisation term
     *
     * @param alpha a scalar
     * @param x left-hand-side
     * @param beta a scalar
     * @param y result
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0, class ContainerType1>
    void symv( value_type alpha, const ContainerType0& x, value_type beta, ContainerType1& y)
    {
        //compute gradient
        dg::blas2::gemv( m_rightx, x, m_tempx); //R_x*f
        dg::blas2::gemv( m_righty, x, m_tempy); //R_y*f

        //multiply with tensor (note the alias)
        dg::tensor::multiply2d(m_chi, m_tempx, m_tempy, m_tempx, m_tempy);

        //now take divergence
        dg::blas2::symv( m_lefty, m_tempy, m_temp);
        dg::blas2::symv( -1., m_leftx, m_tempx, -1., m_temp);

        //add jump terms
        dg::blas2::symv( m_jfactor, m_jumpX, x, 1., m_temp);
        dg::blas2::symv( m_jfactor, m_jumpY, x, 1., m_temp);
        if( m_no == normed)
            dg::blas1::pointwiseDivide( alpha, m_temp, m_vol, beta, y);
        if( m_no == not_normed)//multiply weights without volume
            dg::blas1::pointwiseDot( alpha, m_weights_wo_vol, m_temp, beta, y);
    }

    private:
    bc inverse( bc bound)
    {
        if( bound == DIR) return NEU;
        if( bound == NEU) return DIR;
        if( bound == DIR_NEU) return NEU_DIR;
        if( bound == NEU_DIR) return DIR_NEU;
        return PER;
    }
    direction inverse( direction dir)
    {
        if( dir == forward) return backward;
        if( dir == backward) return forward;
        return centered;
    }
    Matrix m_leftx, m_lefty, m_rightx, m_righty, m_jumpX, m_jumpY;
    container m_weights, m_inv_weights, m_precond, m_weights_wo_vol;
    container m_tempx, m_tempy, m_temp;
    norm m_no;
    SparseTensor<container> m_chi;
    container m_sigma, m_vol;
    value_type m_jfactor;
};

template <class Geometry, class Matrix, class container>
using Elliptic2d = Elliptic<Geometry, Matrix, container>;

/**
 * @brief %Operator that acts as a 3d negative elliptic differential operator
 *
 * @ingroup matrixoperators
 *
 * The term discretized is
 * \f[ -\nabla \cdot ( \mathbf b  \mathbf b \cdot \nabla ) \f]
  In general that means
 * \f[
 * \begin{align}
 * v = b^x \partial_x f + b^y\partial_y f + b^z \partial_z f \\
 * -\frac{1}{\sqrt{g}} \left(\partial_x(\sqrt{g} b^x v ) + \partial_y(\sqrt{g}b^y v) + \partial_z(\sqrt{g} b^z v)\right)
 *  \end{align}
 *  \f]
 * is discretized, with \f$ b^i\f$ being the contravariant components of \f$\mathbf b\f$ .
 * @copydoc hide_geometry_matrix_container
 * This class has the \c SelfMadeMatrixTag so it can be used in blas2::symv functions
 * and thus in a conjugate gradient solver.
 * @note The constructors initialize \f$ b^x = b^y = b^z=1\f$
 * @attention Pay attention to the negative sign which is necessary to make the matrix @b positive @b definite
 */
template< class Geometry, class Matrix, class container>
struct GeneralElliptic
{
    /**
     * @brief Construct from Grid
     *
     * @param g The Grid, boundary conditions are taken from here
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative
     */
    GeneralElliptic( const Geometry& g, norm no = not_normed, direction dir = forward): GeneralElliptic( g, g.bcx(), g.bcy(), g.bcz(), no, dir){}
    /**
     * @brief Construct from Grid and bc
     *
     * @param g The Grid
     * @param bcx boundary condition in x
     * @param bcy boundary contition in y
     * @param bcz boundary contition in z
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative
     */
    GeneralElliptic( const Geometry& g, bc bcx, bc bcy, bc bcz, norm no = not_normed, direction dir = forward):
        leftx ( dg::create::dx( g, inverse( bcx), inverse(dir))),
        lefty ( dg::create::dy( g, inverse( bcy), inverse(dir))),
        leftz ( dg::create::dz( g, inverse( bcz), inverse(dir))),
        rightx( dg::create::dx( g, bcx, dir)),
        righty( dg::create::dy( g, bcy, dir)),
        rightz( dg::create::dz( g, bcz, dir)),
        jumpX ( dg::create::jumpX( g, bcx)),
        jumpY ( dg::create::jumpY( g, bcy)),
        weights_(dg::create::volume(g)), inv_weights_(dg::create::inv_volume(g)), precond_(dg::create::inv_weights(g)),
        xchi( dg::evaluate( one, g) ), ychi( xchi), zchi( xchi),
        xx(xchi), temp0( xx), temp1(temp0),
        no_(no)
    {
        vol_=dg::tensor::volume(g.metric());
    }
    /**
     * @brief Set x-component of \f$ \chi\f$
     *
     * @param chi new x-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_x( const ContainerType0& chi)
    {
        dg::blas1::copy( chi, xchi);
    }
    /**
     * @brief Set y-component of \f$ \chi\f$
     *
     * @param chi new y-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_y( const ContainerType0& chi)
    {
        dg::blas1::copy( chi, ychi);
    }
    /**
     * @brief Set z-component of \f$ \chi\f$
     *
     * @param chi new z-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_z( const ContainerType0& chi)
    {
        dg::blas1::copy( chi, zchi);
    }

    /**
     * @brief Set new components for \f$ \chi\f$
     *
     * @param chi chi[0] is new x-component, chi[1] the new y-component, chi[2] z-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set( const std::vector<ContainerType0>& chi)
    {
        dg::blas1::copy( chi[0], xchi);
        dg::blas1::copy( chi[1], ychi);
        dg::blas1::copy( chi[2], zchi);
    }

    ///@copydoc Elliptic::weights()
    const container& weights()const {return weights_;}
    ///@copydoc Elliptic::inv_weights()
    const container& inv_weights()const {return inv_weights_;}
    /**
     * @brief Returns the preconditioner to use in conjugate gradient
     *
     * In this case inverse weights (without volume element) are returned
     * @return inverse weights
     */
    const container& precond()const {return precond_;}

    ///@copydoc Elliptic::symv()
    template<class ContainerType0, class ContainerType1>
    void symv( const ContainerType0& x, ContainerType1& y)
    {
        //can be faster with blas1::subroutine
        dg::blas2::gemv( rightx, x, temp0); //R_x*x
        dg::blas1::pointwiseDot( 1., xchi, temp0, 0., xx);//Chi_x*R_x*x

        dg::blas2::gemv( righty, x, temp0);//R_y*x
        dg::blas1::pointwiseDot( 1., ychi, temp0, 1., xx);//Chi_y*R_y*x

        dg::blas2::gemv( rightz, x, temp0); // R_z*x
        dg::blas1::pointwiseDot( 1., zchi, temp0, 1., xx);//Chi_z*R_z*x

        dg::blas1::pointwiseDot( vol_, xx, temp0);

        dg::blas1::pointwiseDot( xchi, temp0, temp1);
        dg::blas2::gemv( -1., leftx, temp1, 0., y);

        dg::blas1::pointwiseDot( ychi, temp0, temp1);
        dg::blas2::gemv( -1., lefty, temp1, 1., y);

        dg::blas1::pointwiseDot( zchi, temp0, temp1);
        dg::blas2::gemv( -1., leftz, temp1, 1., y);

        dg::blas2::symv( +1., jumpX, x, 1., y);
        dg::blas2::symv( +1., jumpY, x, 1., y);
        dg::blas1::pointwiseDivide( y, vol_, y);
        if( no_==not_normed)//multiply weights
        {
            dg::blas1::pointwiseDot( y, weights_, y);
        }
    }
    private:
    bc inverse( bc bound)
    {
        if( bound == DIR) return NEU;
        if( bound == NEU) return DIR;
        if( bound == DIR_NEU) return NEU_DIR;
        if( bound == NEU_DIR) return DIR_NEU;
        return PER;
    }
    direction inverse( direction dir)
    {
        if( dir == forward) return backward;
        if( dir == backward) return forward;
        return centered;
    }
    Matrix leftx, lefty, leftz, rightx, righty, rightz, jumpX, jumpY;
    container weights_, inv_weights_, precond_; //contain coeffs for chi multiplication
    container xchi, ychi, zchi, xx, temp0, temp1;
    norm no_;
    container vol_;
};

/**
 * @brief %Operator that acts as a 3d negative elliptic differential operator. Is the symmetric of the GeneralElliptic with
 * 0.5(D_+ + D_-) or vice versa
 *
 * @ingroup matrixoperators
 *
 * The term discretized is
 * \f[ -\nabla \cdot ( \mathbf b  \mathbf b \cdot \nabla ) \f]
  In general that means
 * \f[
 * \begin{align}
 * v = b^x \partial_x f + b^y\partial_y f + b^z \partial_z f \\
 * -\frac{1}{\sqrt{g}} \left(\partial_x(\sqrt{g} b^x v ) + \partial_y(\sqrt{g}b^y v) + \partial_z(\sqrt{g} b^z v)\right)
 *  \end{align}
 *  \f]
 * is discretized, with \f$ b^i\f$ being the contravariant components of \f$\mathbf b\f$ .
 * @copydoc hide_geometry_matrix_container
 * This class has the \c SelfMadeMatrixTag so it can be used in blas2::symv functions
 * and thus in a conjugate gradient solver.
 * @note The constructors initialize \f$ \chi_x = \chi_y = \chi_z=1\f$
 * @attention Pay attention to the negative sign which is necessary to make the matrix @b positive @b definite
 */
template<class Geometry, class Matrix, class container>
struct GeneralEllipticSym
{
    /**
     * @brief Construct from Grid
     *
     * @param g The Grid, boundary conditions are taken from here
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative
     */
    GeneralEllipticSym( const Geometry& g, norm no = not_normed, direction dir = forward): GeneralEllipticSym( g, g.bcx(), g.bcy(), g.bcz(), no, dir)
    { }

        /**
     * @brief Construct from Grid and bc
     *
     * @param g The Grid
     * @param bcx boundary condition in x
     * @param bcy boundary contition in y
     * @param bcz boundary contition in z
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative
     */
    GeneralEllipticSym( const Geometry& g, bc bcx, bc bcy,bc bcz, norm no = not_normed, direction dir = forward):
        ellipticForward_( g, bcx, bcy, bcz, no, dir), ellipticBackward_(g,bcx,bcy,bcz,no,inverse(dir)),
        temp_( dg::evaluate( one, g) )
    {
    }
    /**
     * @brief Set x-component of \f$ \chi\f$
     *
     * @param chi new x-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_x( const ContainerType0& chi)
    {
        ellipticForward_.set_x( chi);
        ellipticBackward_.set_x( chi);
    }
    /**
     * @brief Set y-component of \f$ \chi\f$
     *
     * @param chi new y-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_y( const ContainerType0& chi)
    {
        ellipticForward_.set_y( chi);
        ellipticBackward_.set_y( chi);
    }
    /**
     * @brief Set z-component of \f$ \chi\f$
     *
     * @param chi new z-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_z( const ContainerType0& chi)
    {
        ellipticForward_.set_z( chi);
        ellipticBackward_.set_z( chi);
    }

    /**
     * @brief Set new components for \f$ chi\f$
     *
     * @param chi chi[0] is new x-component, chi[1] the new y-component, chi[2] z-component
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set( const std::vector<ContainerType0>& chi)
    {
        ellipticForward_.set( chi);
        ellipticBackward_.set( chi);
    }

    ///@copydoc Elliptic::weights()
    const container& weights()const {return ellipticForward_.weights();}
    ///@copydoc Elliptic::inv_weights()
    const container& inv_weights()const {return ellipticForward_.inv_weights();}
    ///@copydoc GeneralElliptic::precond()
    const container& precond()const {return ellipticForward_.precond();}

    ///@copydoc Elliptic::symv()
    template<class ContainerType0, class ContainerType1>
    void symv( const ContainerType0& x, ContainerType1& y)
    {
        ellipticForward_.symv( x,y);
        ellipticBackward_.symv( x,temp_);
        dg::blas1::axpby( 0.5, temp_, 0.5, y);
    }
    private:
    direction inverse( direction dir)
    {
        if( dir == forward) return backward;
        if( dir == backward) return forward;
        return centered;
    }
    dg::GeneralElliptic<Geometry, Matrix, container> ellipticForward_, ellipticBackward_;
    container temp_;
};

/**
 * @brief %Operator that acts as a 3d negative elliptic differential operator
 *
 * @ingroup matrixoperators
 *
 * The term discretized is \f[ -\nabla \cdot ( \mathbf \chi\cdot \nabla ) \f]
 * where \f$ \mathbf \chi \f$ is a positive semi-definit tensor.
 * In general coordinates that means
 * \f[ -\frac{1}{\sqrt{g}}\left(
 * \partial_x\left(\sqrt{g}\left(\chi^{xx}\partial_x + \chi^{xy}\partial_y + \chi^{xz}\partial_z \right)\right)
 + \partial_y\left(\sqrt{g}\left(\chi^{yx}\partial_x + \chi^{yy}\partial_y + \chi^{yz}\partial_z \right)\right)
 + \partial_z\left(\sqrt{g}\left(\chi^{zx}\partial_x + \chi^{zy}\partial_y + \chi^{zz}\partial_z \right)\right)
 \right)\f]
 is discretized. Note that the local discontinuous Galerkin discretization adds so-called
 jump terms
 \f[ D^\dagger \chi D + \alpha J \f]
 where \f$\alpha\f$  is a scale factor ( = jfactor), \f$ D \f$ contains the discretizations of the above derivatives, and \f$ J\f$ is a self-adjoint matrix.
 (The symmetric part of \f$J\f$ is added @b before the volume element is divided). The adjoint of a matrix is defined with respect to the volume element including dG weights.
 Usually the default \f$ \alpha=1 \f$ is a good choice.
 However, in some cases, e.g. when \f$ \chi \f$ exhibits very large variations
 \f$ \alpha=0.1\f$ or \f$ \alpha=0.01\f$ might be better values.
 In a time dependent problem the value of \f$\alpha\f$ determines the
 numerical diffusion, i.e. for too low values numerical oscillations may appear.
 Also note that a forward discretization has more diffusion than a centered discretization.

 The following code snippet demonstrates the use of \c Elliptic in an inversion problem
 * @snippet elliptic_b.cu invert
 * @copydoc hide_geometry_matrix_container
 * This class has the \c SelfMadeMatrixTag so it can be used in \c blas2::symv functions
 * and thus in a conjugate gradient solver.
 * @note The constructors initialize \f$ \chi=1\f$ so that a negative laplacian operator
 * results
 * @note the jump term \f$ \alpha J\f$  adds artificial numerical diffusion as discussed above
 * @attention Pay attention to the negative sign which is necessary to make the matrix @b positive @b definite
 *
 */
template <class Geometry, class Matrix, class container>
class Elliptic3d
{
    public:
    using value_type = get_value_type<container>;
    ///@brief empty object ( no memory allocation, call \c construct before using the object)
    Elliptic3d(){}
    /**
     * @brief Construct from Grid
     *
     * @param g The Grid, boundary conditions are taken from here
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative

     * @param jfactor (\f$ = \alpha \f$ ) scale jump terms (1 is a good value but in some cases 0.1 or 0.01 might be better)
     * @note chi is assumed 1 per default
     */
    Elliptic3d( const Geometry& g, norm no = not_normed, direction dir = forward, value_type jfactor=1.)
    {
        construct( g, g.bcx(), g.bcy(), g.bcz(), no, dir, jfactor);
    }

    ///@copydoc Elliptic3d::construct()
    Elliptic3d( const Geometry& g, bc bcx, bc bcy, bc bcz, norm no = not_normed, direction dir = forward, value_type jfactor=1.)
    {
        construct( g, bcx, bcy, no, dir, jfactor);
    }

    /**
     * @brief Construct from grid and boundary conditions
     * @param g The Grid
     * @param bcx boundary condition in x
     * @param bcy boundary contition in y
     * @param bcz boundary contition in z
     * @param no Not normed for elliptic equations, normed else
     * @param dir Direction of the right first derivative (i.e. forward, backward or centered)
     * @param jfactor scale jump terms (1 is a good value but in some cases 0.1 or 0.01 might be better)
     */
    void construct( const Geometry& g, bc bcx, bc bcy, bc bcz, norm no = not_normed, direction dir = forward, value_type jfactor = 1.)
    {
        m_no=no, m_jfactor=jfactor;
        dg::blas2::transfer( dg::create::dx( g, inverse( bcx), inverse(dir)), m_leftx);
        dg::blas2::transfer( dg::create::dy( g, inverse( bcy), inverse(dir)), m_lefty);
        dg::blas2::transfer( dg::create::dz( g, inverse( bcz), inverse(dir)), m_leftz);
        dg::blas2::transfer( dg::create::dx( g, bcx, dir), m_rightx);
        dg::blas2::transfer( dg::create::dy( g, bcy, dir), m_righty);
        dg::blas2::transfer( dg::create::dz( g, bcz, dir), m_rightz);
        dg::blas2::transfer( dg::create::jumpX( g, bcx),   m_jumpX);
        dg::blas2::transfer( dg::create::jumpY( g, bcy),   m_jumpY);

        dg::assign( dg::create::inv_volume(g),    m_inv_weights);
        dg::assign( dg::create::volume(g),        m_weights);
        dg::assign( dg::create::inv_weights(g),   m_precond);
        m_temp = m_tempx = m_tempy = m_tempz = m_inv_weights;
        m_chi=g.metric();
        m_vol=dg::tensor::volume(m_chi);
        dg::tensor::scal( m_chi, m_vol);
        dg::assign( dg::create::weights(g), m_weights_wo_vol);
        dg::assign( dg::evaluate(dg::one, g), m_sigma);
    }

    ///@copydoc  Elliptic3d::Elliptic3d(const Geometry&,norm,direction,value_type)
    void construct( const Geometry& g, norm no = not_normed, direction dir = forward, value_type jfactor = 1.){
        construct( g, g.bcx(), g.bcy(), g.bcz(), no, dir, jfactor);
    }

    /**
     * @brief Change scalar part in Chi tensor
     *
     * Internally, we split the tensor \f$\chi = \sigma\mathbf{\tau}\f$ into
     * a scalar part \f$ \sigma\f$ and a tensor part \f$ \tau\f$ and you can
     * set each part seperately. This functions sets the scalar part.
     *
     * @param sigma The new scalar part in \f$\chi\f$ (all elements must be >0)
     * @tparam ContainerType0 must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0>
    void set_chi( const ContainerType0& sigma)
    {
        dg::blas1::pointwiseDivide( sigma, m_sigma, m_tempx);
        //update preconditioner
        dg::blas1::pointwiseDivide( m_precond, m_tempx, m_precond);
        dg::tensor::scal( m_chi, m_tempx);
        dg::blas1::copy( sigma, m_sigma);
    }
    /**
     * @brief Change tensor part in Chi tensor
     *
     * Internally, we split the tensor \f$\chi = \sigma\mathbf{\tau}\f$ into
     * a scalar part \f$ \sigma\f$ and a tensor part \f$ \tau\f$ and you can
     * set each part seperately. This functions sets the tensor part.
     *
     * @param tau The new tensor part in \f$\chi\f$ (must be positive definite)
     * @tparam ContainerType0 must be usable in \c dg::assign to \c container
     */
    template<class ContainerType0>
    void set_chi( const SparseTensor<ContainerType0>& tau)
    {
        m_chi = SparseTensor<container>(tau);
        dg::tensor::scal( m_chi, m_sigma);
        dg::tensor::scal( m_chi, m_vol);
    }

    /**
     * @brief Return the vector missing in the un-normed symmetric matrix
     *
     * i.e. the inverse of the weights() function
     * @return inverse volume form including inverse weights
     */
    const container& inv_weights()const {
        return m_inv_weights;
    }
    /**
     * @brief Return the vector making the matrix symmetric
     *
     * i.e. the volume form
     * @return volume form including weights
     */
    const container& weights()const {
        return m_weights;
    }
    /**
     * @brief Return the default preconditioner to use in conjugate gradient
     *
     * Currently returns the inverse weights without volume elment divided by the current \f$ \chi\f$.
     * This is especially good when \f$ \chi\f$ exhibits large amplitudes or variations
     * @return the inverse of \f$\chi\f$.
     */
    const container& precond()const {
        return m_precond;
    }
    /**
     * @brief Set the currently used jfactor
     *
     * @param new_jfactor The new scale factor for jump terms
     */
    void set_jfactor( value_type new_jfactor) {m_jfactor = new_jfactor;}
    /**
     * @brief Get the currently used jfactor
     *
     * @return  The current scale factor for jump terms
     */
    value_type get_jfactor() const {return m_jfactor;}

    /**
     * @brief Computes the polarisation term
     *
     * @param x left-hand-side
     * @param y result
     * @note memops required:
            - 19 reads + 9 writes
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0, class ContainerType1>
    void symv( const ContainerType0& x, ContainerType1& y){
        symv( 1, x, 0, y);
    }
    /**
     * @brief Computes the polarisation term
     *
     * @param alpha a scalar
     * @param x left-hand-side
     * @param beta a scalar
     * @param y result
     * @tparam ContainerTypes must be usable with \c container in \ref dispatch
     */
    template<class ContainerType0, class ContainerType1>
    void symv( value_type alpha, const ContainerType0& x, value_type beta, ContainerType1& y)
    {
        //compute gradient
        dg::blas2::gemv( m_rightx, x, m_tempx); //R_x*f
        dg::blas2::gemv( m_righty, x, m_tempy); //R_y*f
        dg::blas2::gemv( m_rightz, x, m_tempz); //R_z*f

        //multiply with tensor (note the alias)
        dg::tensor::multiply3d(m_chi, m_tempx, m_tempy, m_tempz, m_tempx, m_tempy, m_tempz);

        //now take divergence
        dg::blas2::symv( -1., m_leftz, m_tempz, 0., m_temp);
        dg::blas2::symv( -1., m_lefty, m_tempy, 1., m_temp);
        dg::blas2::symv( -1., m_leftx, m_tempx, 1., m_temp);

        //add jump terms
        dg::blas2::symv( m_jfactor, m_jumpX, x, 1., m_temp);
        dg::blas2::symv( m_jfactor, m_jumpY, x, 1., m_temp);
        if( m_no == normed)
            dg::blas1::pointwiseDivide( alpha, m_temp, m_vol, beta, y);
        if( m_no == not_normed)//multiply weights without volume
            dg::blas1::pointwiseDot( alpha, m_weights_wo_vol, m_temp, beta, y);
    }

    private:
    bc inverse( bc bound)
    {
        if( bound == DIR) return NEU;
        if( bound == NEU) return DIR;
        if( bound == DIR_NEU) return NEU_DIR;
        if( bound == NEU_DIR) return DIR_NEU;
        return PER;
    }
    direction inverse( direction dir)
    {
        if( dir == forward) return backward;
        if( dir == backward) return forward;
        return centered;
    }
    Matrix m_leftx, m_lefty, m_leftz, m_rightx, m_righty, m_rightz, m_jumpX, m_jumpY;
    container m_weights, m_inv_weights, m_precond, m_weights_wo_vol;
    container m_tempx, m_tempy, m_tempz, m_temp;
    norm m_no;
    SparseTensor<container> m_chi;
    container m_sigma, m_vol;
    value_type m_jfactor;
};
///@cond
template< class G, class M, class V>
struct TensorTraits< Elliptic<G, M, V> >
{
    using value_type      = get_value_type<V>;
    using tensor_category = SelfMadeMatrixTag;
};

template< class G, class M, class V>
struct TensorTraits< GeneralElliptic<G, M, V> >
{
    using value_type      = get_value_type<V>;
    using tensor_category = SelfMadeMatrixTag;
};
template< class G, class M, class V>
struct TensorTraits< GeneralEllipticSym<G, M, V> >
{
    using value_type      = get_value_type<V>;
    using tensor_category = SelfMadeMatrixTag;
};
template< class G, class M, class V>
struct TensorTraits< Elliptic3d<G, M, V> >
{
    using value_type      = get_value_type<V>;
    using tensor_category = SelfMadeMatrixTag;
};
///@endcond

} //namespace dg
