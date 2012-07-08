#!/usr/bin/env bash
#
# @ job_name            = kslrun_job
# @ job_type            = bluegene
# @ output              = ./$(job_name)_$(jobid).out
# @ error               = ./$(job_name)_$(jobid).err
# @ environment         = COPY_ALL; 
# @ wall_clock_limit    = 4:00:00,4:00:00
# @ notification        = always
# @ bg_size             = 2048
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

testdir=/gpfs/scratch/aron/sandbox/import/collfs/tests

cd $testdir
logdir=${testdir}/runs
mkdir -p ${logdir}

for np in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096
do
    mpirun -env LD_LIBRARY_PATH=${ldpath} -env PYTHONPATH=${bgp_python_path} \
        -mode VN -exp_env HOME -n $np ${bgp_python} test_python_importer.py &> ${logdir}/python_${np}.txt
    mpirun -env LD_LIBRARY_PATH=${ldpath} -env PYTHONPATH=${bgp_python_path} \
        -mode VN -exp_env HOME -n $np ${mpi_python} test_collfs_importer.py &> ${logdir}/collfs_${np}.txt
    mpirun -env LD_LIBRARY_PATH=${ldpath} -env PYTHONPATH=${bgp_python_path} \
        -mode VN -exp_env HOME -n $np ${bgp_python} test_mpi4py_cached_importer.py &> ${logdir}/asher_${np}.txt
    mpirun -env LD_LIBRARY_PATH=${ldpath} -env PYTHONPATH=${bgp_python_path} \
        -mode VN -exp_env HOME -n $np ${mpi_python} test_mpi4py_cached_importer.py &> ${logdir}/asher_collfs_${np}.txt
done