ifeq ($(strip $(NERSC_HOST)),perlmutter)

#DEFAULT
CC=CC #C++ compiler
MPICC=CC #mpic++  #mpi compiler
CFLAGS=-Wall -std=c++14 -mavx -mfma #flags for CC
NVCC=nvcc #CUDA compiler
NVCCARCH=-arch sm_61 -Xcudafe "--diag_suppress=code_is_unreachable --diag_suppress=initialization_not_reachable" #nvcc gpu compute capability
NVCCFLAGS= -std=c++14 -Xcompiler "-Wall -mavx -mfma" --extended-lambda  #--target-accel=nvidia80 #-allow-unsupported-compiler #flags for NVCC
OPT=-O2 # optimization flags for host code (it is O2 and not O3 because g++-7 up to g++-8.0 have a bug with fma in -O3, fixed in g++-8.1)
OMPFLAG=-fopenmp #openmp flag for CC and MPICC

#external libraries
INCLUDE = -I$(HOME)/include# cusp, thrust and the draw libraries
# the libhdf5-dev package installs *_serial.so libs in order to distinguish from *_openmpi.so
LIBS= #-L$(HOME)/github/netcdf-c/install/lib -L$(HOME)/github/hdf5/install/lib  #-lnetcdf -lhdf5_serial -lhdf5_serial_hl # serial netcdf library for file output
LAPACKLIB=-llapacke
JSONLIB= -L/global/homes/r/rgerrum/github/vcpkg/packages/jsoncpp_x64-linux/lib -ljsoncpp

#-L$(HOME)/github/jsoncpp/src/lib_json -ljsoncpp # json library for input parameters
GLFLAGS =$$(pkg-config --static --libs glfw3) -lGL #glfw3 installation


INCLUDE +=  -I$(HOME)/include/json #-I$(HOME)/include/netcdf -I$(HOME)/include/hdf5/inc








#TEST FOR PERLMUTTER
#CC=g++
#CFLAGS=
#MPICC= mpic++
#MPICFLAGS=
#OPT=
#OMPFLAG= -fopenmp
#NVCC=nvcc
#NVCCFLAGS= -allow-unsupported-compiler -std=c++14 -Xcompiler "-Wall -mavx -mfma" --extended-lambda
#NVCCARCH=
#LIBS=
#JSONLIB=
#LAPACKLIB=
#GLFLAGS=

#INCLUDE += -I/global/u1/r/rgerrum/github/netcdf-c/install/include -I/../hdf5/install/include
#INCLUDE += -I$(HOME)/include
#INCLUDE += -I$(HOME)/include/netcdf/inc -I$(HOME)/include/hdf5/inc -I$(HOME)/include/json
#  -I../../inc -I../../../../include -I/global/homes/r/rgerrum/include/netcdf  -I/global/homes/r/rgerrum/include/netcdf -I../

#M100 make file
##CC=xlc++ #C++ compiler
##MPICC=mpixlC  #mpi compiler
##CFLAGS=-Wall -std=c++1y -DWITHOUT_VCL -mcpu=power9 -qstrict# -mavx -mfma #flags for CC
##OMPFLAG=-qsmp=omp

#CFLAGS=-Wall -std=c++14 -DWITHOUT_VCL -mcpu=power9 # -mavx -mfma #flags for CC
#OPT=-O3 # optimization flags for host code
#NVCC=nvcc #CUDA compiler
#NVCCARCH=-arch sm_70 -Xcudafe "--diag_suppress=code_is_unreachable --#diag_suppress=initialization_not_reachable" #nvcc gpu compute capability
#NVCCFLAGS= -std=c++14 -Xcompiler "-mcpu=power9 -Wall" --extended-lambda# -mavx -mfma" #flags for NVCC

#INCLUDE += -I$(NETCDF_INC) -I$(HDF5_INC) -I$(JSONCPP_INC)
#INCLUDE += -L$(BOOST_INC) -I$(LAPACK_INC)
#JSONLIB=-L$(JSONCPP_LIB) -ljsoncpp # json library for input parameters
#LIBS    =-L$(HDF5_LIB) -lhdf5 -lhdf5_hl
#LIBS    +=-L$(NETCDF_LIB) -lnetcdf -lcurl
#LAPACKLIB = -L$(LAPACK_LIB) -llapacke





##INCLUDE += --enable-orterun-prefix-by-default  --prefix=/global/common/software/m3169/perlmutter/openmpi/5.0.0-ofi-cuda-22.7_11.7/gnu --with-cuda=/opt/nvidia/hpc_sdk/Linux_x86_64/22.7/cuda/11.7  --with-cuda-libdir=/usr/lib64 --with-ucx=no  --with-verbs=no  --enable-mpi-java --with-ofi  --enable-mpi1-compatibility --with-pmix=internal  --disable-sphinx

#CRAIG should always compile with CC in perlmutter.
# USE ALWAYS SRUN for running
#   INCLUDE += # -I/global/homes/r/rgerrum/github/jsoncpp/include  #-I/global/common/software/m3169/perlmutter/openmpi/5.0.0-ofi-cuda-22.7_11.7/gnu/include  -I/opt/cray/pe/netcdf/4.9.0.3/gnu/9.1/include -I/opt/cray/pe/hdf5/1.12.2.3/gnu/9.1/include -I/global/homes/r/rgerrum/github/jsoncpp/include 
#INCLUDE +=# -L$(BOOST_INC) -I$(LAPACK_INC)
#   JSONLIB= #-L/global/homes/r/rgerrum/github/vcpkg/installed/x64-linux/lib -ljsoncpp # json library for input parameters
#LIBS    =-L/opt/cray/pe/hdf5/1.12.2.3/gnu/9.1/lib -lhdf5 -lhdf5_hl
#LIBS    +=-L/opt/cray/pe/netcdf/4.9.0.3/gnu/9.1/lib -lnetcdf #-lcurl
#LIBS += -L/global/common/software/m3169/perlmutter/openmpi/5.0.0-ofi-cuda-22.7_11.7/gnu/lib -lmpi

endif

