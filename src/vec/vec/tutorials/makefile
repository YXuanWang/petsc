-include ../../../../petscdir.mk

include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules

ex7: ex7f.o
ex21f90: ex21.o
ex43f: ex43.o

testex31: ex31.PETSc
	-@${MPIEXEC} -n 1 ${MPIEXEC_TAIL} ./ex31 > ex31_1.tmp 2>&1; \
	 if (${DIFF} output/ex31_1.testout ex31_1.tmp > /dev/null 2>&1) then \
           echo "C/C++ example src/vec/vec/tutorials/ex31 run successfully with MATLAB engine"; \
	 else \
           echo "Possible error running C/C++ src/vec/vec/tutorials/ex31 with MATLAB engine"; \
           echo "See https://petsc.org/release/faq/";\
           touch ../../../../check_error;\
           cat ex31_1.tmp;\
         fi

