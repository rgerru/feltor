#include <iostream>
#include <iomanip>
#include <vector>


#include "toeflI.cuh"
#include "../galerkin/parameters.h"
#include "dg/rk.cuh"
#include "file/file.h"
#include "file/read_input.h"

#include "dg/timer.cuh"


/*
   - reads parameters from input.txt or any other given file, 
   - integrates the ToeflR - functor and 
   - writes outputs to a given outputfile using hdf5. 
        density fields are the real densities in XSPACE ( not logarithmic values)
*/

const unsigned k = 3;//!< a change in k needs a recompilation

int main( int argc, char* argv[])
{
    //Parameter initialisation
    std::vector<double> v;
    std::string input;
    if( argc != 3)
    {
        std::cerr << "ERROR: Wrong number of arguments!\nUsage: "<< argv[0]<<" [inputfile] [outputfile]\n";
        return -1;
    }
    else 
    {
        v = file::read_input( argv[1]);
        input = file::read_file( argv[1]);
    }
    const Parameters p( v, 2);
    p.display( std::cout);
    if( p.k != k)
    {
        std::cerr << "ERROR: k doesn't match: "<<k<<" (code) vs. "<<p.k<<" (input)\n";
        return -1;
    }

    ////////////////////////////////set up computations///////////////////////////
    dg::Grid<double > grid( 0, p.lx, 0, p.ly, p.n, p.Nx, p.Ny, p.bc_x, p.bc_y);
    //create RHS 
    dg::ToeflI<dg::DVec > test( grid, p.kappa, p.nu, p.tau, p.a_z, p.mu_z, p.tau_z, p.eps_pol, p.eps_gamma); 
    //create initial vector
    dg::Gaussian g( p.posX*grid.lx(), p.posY*grid.ly(), p.sigma, p.sigma, p.n0); 
    std::vector<dg::DVec> y0(3, dg::evaluate( g, grid)), y1(y0); // n_e' = gaussian
    y0[2] = dg::evaluate( dg::one, grid);
    dg::blas2::symv( test.gamma(), y0[0], y0[1]); // n_e = \Gamma_i n_i -> n_i = ( 1+alphaDelta) n_e' + 1
    dg::blas2::symv( (dg::DVec)dg::create::v2d( grid), y0[1], y0[1]);
    dg::blas1::axpby( 1./(1.-p.a_z), y0[1], 0., y0[1]); //n_i ~ 1./a_i n_e
    thrust::transform( y0[0].begin(), y0[0].end(), y0[0].begin(), dg::PLUS<double>(+1));
    thrust::transform( y0[1].begin(), y0[1].end(), y0[1].begin(), dg::PLUS<double>(+1));
    test.log( y0, y0); //transform to logarithmic values
    //////////////////initialisation of timestepper and first step///////////////////
    double time = 0;
    dg::AB< k, std::vector<dg::DVec> > ab( y0);
    ab.init( test, y0, p.dt);
    ab( test, y0, y1, p.dt);
    y0.swap( y1); //y1 now contains value at zero time
    /////////////////////////////set up hdf5/////////////////////////////////
    file::T5trunc t5file( argv[2], input);
    dg::HVec output[4] = { y1[0], y1[0], y1[0], y1[0]}; //intermediate transport locations
    if( p.global)
        test.exp( y1,y1); //transform to correct values
    output[0] = y1[0], output[1] = y1[1], output[2] = y1[2], output[3] = test.potential()[0]; //electrons
    t5file.write( output[0], output[1], output[2], output[3], time, grid.n()*grid.Nx(), grid.n()*grid.Ny());
    t5file.append( test.mass(), test.mass_diffusion(), test.energy(), test.energy_diffusion());
    ///////////////////////////////////////Timeloop/////////////////////////////////
    dg::Timer t;
    t.tic();
    try
    {
#ifdef DG_BENCHMARK
    unsigned step = 0;
#endif //DG_BENCHMARK
    for( unsigned i=0; i<p.maxout; i++)
    {

#ifdef DG_BENCHMARK
        dg::Timer ti;
        ti.tic();
#endif//DG_BENCHMARK
        for( unsigned j=0; j<p.itstp; j++)
        {
            ab( test, y0, y1, p.dt);
            y0.swap( y1); //attention on -O3 ?
            //store accuracy details
            t5file.append( test.mass(), test.mass_diffusion(), test.energy(), test.energy_diffusion());
        }
        time += p.itstp*p.dt;
        //output all three fields
        test.exp( y1,y1); //transform to correct values
        output[0] = y1[0], output[1] = y1[1], output[2] = y1[2], output[3] = test.potential()[0]; //electrons
        t5file.write( output[0], output[1], output[2], output[3], time, grid.n()*grid.Nx(), grid.n()*grid.Ny());
#ifdef DG_BENCHMARK
        ti.toc();
        step+=p.itstp;
        std::cout << "\n\t Step "<<step <<" of "<<p.itstp*p.maxout <<" at time "<<time;
        std::cout << "\n\t Average time for one step: "<<ti.diff()/(double)p.itstp<<"s\n\n"<<std::flush;
#endif//DG_BENCHMARK
    }
    }
    catch( dg::Fail& fail) { 
        std::cerr << "CG failed to converge to "<<fail.epsilon()<<"\n";
        std::cerr << "Does Simulation respect CFL condition?\n";
    }
    t.toc(); 
    unsigned hour = (unsigned)floor(t.diff()/3600);
    unsigned minute = (unsigned)floor( (t.diff() - hour*3600)/60);
    double second = t.diff() - hour*3600 - minute*60;
    std::cout << std::fixed << std::setprecision(2) <<std::setfill('0');
    std::cout <<"Computation Time \t"<<hour<<":"<<std::setw(2)<<minute<<":"<<second<<"\n";
    std::cout <<"which is         \t"<<t.diff()/p.itstp/p.maxout<<"s/step\n";

    return 0;

}

