#include <iostream>
#include <iomanip>
#include "mpi_evaluation.h"
#include "mpi_derivatives.h"
#include "blas.h"
#include "mpi_matrix.h"
#include "mpi_precon.h"
#include "mpi_init.h"

const double lx = 2*M_PI;
/*
double function( double x, double y, double z) { return sin(3./4.*z);}
double derivative( double x, double y, double z) { return 3./4.*cos(3./4.*z);}
dg::bc bcz = dg::DIR_NEU;
*/
double function( double x, double y) { return sin(y);}
double derivative( double x, double y) { return cos(y);}

dg::bc bcx = dg::PER, bcy = dg::PER;

int main(int argc, char* argv[])
{
    MPI_Init( &argc, &argv);
    int np[2], rank;
    unsigned n, Nx, Ny; 
    MPI_Comm comm;
    mpi_init2d( bcx, bcy, np, n, Nx, Ny, comm);
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);

    if(rank==0)std::cout <<"Nx " <<Nx << " and Ny "<<Ny<<std::endl;
    dg::MPI_Grid2d g( 0, lx, 0, lx, n, Nx, Ny, bcx, bcy, comm);


    dg::MMatrix dx = dg::create::dy( g, bcx, dg::normed, dg::symmetric);
    //dg::MMatrix lzM = dg::create::laplacianM( g, bcx, bcy, dg::normed, dg::symmetric);

    dg::MVec func = dg::evaluate( function, g);
    dg::MVec result = func, result2(result);
    dg::MVec deriv = dg::evaluate( derivative, g);

    dg::blas1::axpby( 1., func, -1., result2);
    double error = sqrt(dg::blas2::dot(result2, dg::create::weights(g), result2));
    if(rank==0) std::cout << "Distance to true solution: "<<error<<"\n";

    dg::blas2::symv( dx, func, result);
    //double norm = sqrt(dg::blas2::dot(result, dg::create::weights(g), result));
    //if(rank==0) std::cout << "Norm of result             "<<norm<<"\n";

    dg::blas1::axpby( 1., deriv, -1., result);
    error = sqrt(dg::blas2::dot(result, dg::create::weights(g), result));
    if(rank==0) std::cout << "Distance to true solution: "<<error<<"\n";

    //std::cout << std::fixed<<std::setprecision(2);
    //dg::blas2::symv( lzM, func, result);
    //if(rank==0) std::cout<<result<<std::endl;
    //MPI_Barrier(comm);
    //if(rank==1) std::cout<<result<<std::endl;
    //MPI_Barrier(comm);

    //dg::blas1::axpby( 1., func, -1., result);
    //error = sqrt(dg::blas2::dot(result, dg::create::weights(g), result));
    //if(rank==0) std::cout << "Distance to true solution: "<<error<<"\n";
    MPI_Finalize();
    return 0;
}
