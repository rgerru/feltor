#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

#include "draw/host_window.h"
//#include "draw/device_window.cuh"

#include "toeflR.cuh"
#include "dg/rk.cuh"
#include "dg/timer.cuh"
#include "file/read_input.h"
#include "parameters.h"

/*
   - reads parameters from input.txt or any other given file, 
   - integrates the ToeflR - functor and 
   - directly visualizes results on the screen using parameters in window_params.txt
*/

const unsigned k = 3; //!< a change of k needs a recompilation!

int main( int argc, char* argv[])
{
    //Parameter initialisation
    std::vector<double> v, v2;
    std::stringstream title;
    if( argc == 1)
    {
        v = file::read_input("input.txt");
    }
    else if( argc == 2)
    {
        v = file::read_input( argv[1]);
    }
    else
    {
        std::cerr << "ERROR: Too many arguments!\nUsage: "<< argv[0]<<" [filename]\n";
        return -1;
    }

    v2 = file::read_input( "window_params.txt");
    GLFWwindow* w = draw::glfwInitAndCreateWindow( v2[3], v2[4], "");
    draw::RenderHostData render(v2[1], v2[2]);
    /////////////////////////////////////////////////////////////////////////
    const Parameters p( v);
    p.display( std::cout);
    if( p.k != k)
    {
        std::cerr << "ERROR: k doesn't match: "<<k<<" vs. "<<p.k<<"\n";
        return -1;
    }

    dg::Grid<double > grid( 0, p.lx, 0, p.ly, p.n, p.Nx, p.Ny, p.bc_x, p.bc_y);
    //create RHS 
    dg::ToeflR< dg::DVec > test( grid, p.kappa, p.nu, p.tau, p.eps_pol, p.eps_gamma, p.global); 
    //create initial vector
    dg::Gaussian g( p.posX*grid.lx(), p.posY*grid.ly(), p.sigma, p.sigma, p.n0); //gaussian width is in absolute values
    std::vector<dg::DVec> y0(2, dg::evaluate( g, grid)), y1(y0); // n_e' = gaussian

    dg::blas2::symv( test.gamma(), y0[0], y0[1]); // n_e = \Gamma_i n_i -> n_i = ( 1+alphaDelta) n_e' + 1
    dg::blas2::symv( (dg::DVec)dg::create::v2d( grid), y0[1], y0[1]);

    if( p.global)
    {
        thrust::transform( y0[0].begin(), y0[0].end(), y0[0].begin(), dg::PLUS<double>(+1));
        thrust::transform( y0[1].begin(), y0[1].end(), y0[1].begin(), dg::PLUS<double>(+1));
        test.log( y0, y0); //transform to logarithmic values
    }

    dg::AB< k, std::vector<dg::DVec> > ab( y0);
    //dg::TVB< std::vector<dg::DVec> > ab( y0);

    dg::DVec dvisual( grid.size(), 0.);
    dg::HVec hvisual( grid.size(), 0.), visual(hvisual);
    dg::HMatrix equi = dg::create::backscatter( grid);
    draw::ColorMapRedBlueExt colors( 1.);
    //create timer
    dg::Timer t;
    double time = 0;
    ab.init( test, y0, p.dt);
    ab( test, y0, y1, p.dt);
    y0.swap( y1); 
    const double mass0 = test.mass(), mass_blob0 = mass0 - grid.lx()*grid.ly();
    double E0 = test.energy(), energy0 = E0, E1 = 0, diff = 0;
    std::cout << "Begin computation \n";
    std::cout << std::scientific << std::setprecision( 2);
    unsigned step = 0;
    while ( !glfwWindowShouldClose( w ))
    {
        //transform field to an equidistant grid
        if( p.global)
        {
            test.exp( y1, y1);
            thrust::transform( y1[0].begin(), y1[0].end(), dvisual.begin(), dg::PLUS<double>(-1));
        }
        else
            dvisual = y1[0];

        hvisual = dvisual;
        dg::blas2::gemv( equi, hvisual, visual);
        //compute the color scale
        colors.scale() =  (float)thrust::reduce( visual.begin(), visual.end(), 0., dg::AbsMax<double>() );
        //draw ions
        title << std::setprecision(2) << std::scientific;
        title <<"ne / "<<colors.scale()<<"\t";
        render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);

        //transform phi
        dg::blas2::gemv( test.laplacianM(), test.potential()[0], y1[1]);
        hvisual = y1[1];
        dg::blas2::gemv( equi, hvisual, visual);
        //compute the color scale
        colors.scale() =  (float)thrust::reduce( visual.begin(), visual.end(), 0., dg::AbsMax<double>() );
        //draw phi and swap buffers
        title <<"omega / "<<colors.scale()<<"\t";
        title << std::fixed; 
        title << " &&   time = "<<time;
        render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);
        glfwSetWindowTitle(w,title.str().c_str());
        title.str("");
        glfwPollEvents();
        glfwSwapBuffers( w);

        //step 
#ifdef DG_BENCHMARK
        t.tic();
#endif//DG_BENCHMARK
        for( unsigned i=0; i<p.itstp; i++)
        {
            step++;
            if( p.global)
            {
                std::cout << "(m_tot-m_0)/m_0: "<< (test.mass()-mass0)/mass_blob0<<"\t";
                E0 = E1;
                E1 = test.energy();
                diff = (E1 - E0)/p.dt;
                double diss = test.energy_diffusion( );
                std::cout << "(E_tot-E_0)/E_0: "<< (E1-energy0)/energy0<<"\t";
                std::cout << "Accuracy: "<< 2.*(diff-diss)/(diff+diss)<<"\n";

            }
            try{ ab( test, y0, y1, p.dt);}
            catch( dg::Fail& fail) { 
                std::cerr << "CG failed to converge to "<<fail.epsilon()<<"\n";
                std::cerr << "Does Simulation respect CFL condition?\n";
                glfwSetWindowShouldClose( w, GL_TRUE);
                break;
            }
            y0.swap( y1); //attention on -O3 ?
        }
        time += (double)p.itstp*p.dt;
#ifdef DG_BENCHMARK
        t.toc();
        std::cout << "\n\t Step "<<step;
        std::cout << "\n\t Average time for one step: "<<t.diff()/(double)p.itstp<<"s\n\n";
#endif//DG_BENCHMARK
    }
    glfwTerminate();
    ////////////////////////////////////////////////////////////////////

    return 0;

}
