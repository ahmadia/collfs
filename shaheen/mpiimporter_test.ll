#!/usr/bin/env bash
#
# @ job_name            = kslrun_job
# @ job_type            = bluegene
# @ output              = ./$(job_name)_$(jobid).out
# @ error               = ./$(job_name)_$(jobid).err
# @ environment         = COPY_ALL; 
# @ wall_clock_limit    = 4:00:00,4:00:00
# @ notification        = always
# @ bg_size             = 128 
# @ account_no          = k01

# @ queue

projdir=/project/k01/pyclaw
sandbox=${projdir}/sandbox
builddir=${projdir}/opt/share
srcdir=${builddir}/sources

pythondir=${builddir}/python/2.7.2/bgp
ldpath=${pythondir}/lib
numpy_path=${builddir}/numpy/1.6.2/bgp/lib/python
nose_path=${builddir}/nose/1.1.2/bgp/lib/python
clawpack_path=${builddir}/clawpack/dev/bgp/lib/python
petsc4py_path=${builddir}/petsc4py/1.2/bgp/lib/python
mpi_python_path=${builddir}/mpi4py/1.3/bgp/lib/python

bgp_python_path=${numpy_path}:${nose_path}:${clawpack_path}:${petsc4py_path}:${mpi_python_path}

bgp_python=${pythondir}/bin/python
mpi_python=${builddir}/mpi4py/1.3/bgp/lib/python/mpi4py/bin/python-mpi

testdir=/gpfs/scratch/aron/sandbox/import/collfs

cd $testdir
logdir=${testdir}/test_runs
mkdir -p ${logdir}

for np in 2  
do
    mpirun -env LD_LIBRARY_PATH=${ldpath} -env PYTHONPATH=${bgp_python_path} \
        -mode VN -exp_env HOME -n $np ${bgp_python} -v test_import_numpy.py &> ${logdir}/bgp_python_${np}.txt
done
