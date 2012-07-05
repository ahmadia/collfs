projdir=/project/k01/pyclaw
sandbox=${projdir}/sandbox
builddir=${projdir}/opt/share
logdir=${builddir}/logs
srcdir=${builddir}/sources

pythondir=${builddir}/python/2.7.2/bgp
ldpath=${pythondir}/lib
numpy_path=${builddir}/numpy/1.6.2/bgp/lib/python
nose_path=${builddir}/nose/1.1.2/bgp/lib/python
clawpack_path=${builddir}/clawpack/dev/bgp/lib/python
petsc4py_path=${builddir}/petsc4py/1.2/bgp/lib/python
mpi_python_path=${builddir}/mpi4py/1.3/bgp/lib/python

bgp_python_path=${numpy_path}:${nose_path}:${clawpack_path}:${petsc4py_path}:${mpi_python_path}

mpi_python=${builddir}/mpi4py/1.3/bgp/lib/python/mpi4py/bin/python-mpi

mpirun -env LD_LIBRARY_PATH=${ldpath} -env PYTHONPATH=${bgp_python_path} \
    -mode VN -exp_env HOME -n 2 ${mpi_python} -v test.py