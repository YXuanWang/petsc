
static char help[] = "Tests Elemental interface.\n\n"; 

#include <petscmat.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Vec            x,y;
  Mat            B,C,Cexp;
  PetscInt       i,j,m = 5,n,p,nrows,ncols;
  const PetscInt *rows,*cols;
  IS             isrows,iscols;
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscScalar    *v,*vx;
  PetscMPIInt    rank,size;
  PetscReal Cnorm;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  n = m;
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  /*p = n;
   ierr = PetscOptionsGetInt(PETSC_NULL,"-p",&p,PETSC_NULL);CHKERRQ(ierr);*/

  ierr = MatCreate(PETSC_COMM_WORLD,&C);CHKERRQ(ierr);
  ierr = MatSetSizes(C,m,n,PETSC_DECIDE,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
  ierr = MatSetUp(C);CHKERRQ(ierr);

  ierr = PetscOptionsHasName(PETSC_NULL,"-row_oriented",&flg);CHKERRQ(ierr);
  if (flg) {ierr = MatSetOption(C,MAT_ROW_ORIENTED,PETSC_TRUE);CHKERRQ(ierr);}
  ierr = MatGetOwnershipIS(C,&isrows,&iscols);CHKERRQ(ierr);
  if (0) {
    IS tmp;
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Row index set:\n");CHKERRQ(ierr);
    ierr = ISOnComm(isrows,PETSC_COMM_WORLD,PETSC_USE_POINTER,&tmp);CHKERRQ(ierr);
    ierr = ISView(tmp,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = ISDestroy(&tmp);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Column index set:\n");CHKERRQ(ierr);
    ierr = ISOnComm(iscols,PETSC_COMM_WORLD,PETSC_USE_POINTER,&tmp);CHKERRQ(ierr);
    ierr = ISView(tmp,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = ISDestroy(&tmp);CHKERRQ(ierr);
  }
  ierr = ISGetLocalSize(isrows,&nrows);CHKERRQ(ierr);
  ierr = ISGetIndices(isrows,&rows);CHKERRQ(ierr);
  ierr = ISGetLocalSize(iscols,&ncols);CHKERRQ(ierr);
  ierr = ISGetIndices(iscols,&cols);CHKERRQ(ierr);
  ierr = PetscMalloc(nrows*ncols*sizeof *v,&v);CHKERRQ(ierr);
  for (i=0; i<nrows; i++) {
    for (j=0; j<ncols; j++) {
      v[i*ncols+j] = (PetscReal)(100*rows[i]+cols[j]);
    }
  }
  ierr = MatSetValues(C,nrows,rows,ncols,cols,v,INSERT_VALUES);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* Test MatView() */
  ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = MatComputeExplicitOperator(C,&Cexp);CHKERRQ(ierr);
  ierr = MatView(Cexp,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = MatDestroy(&Cexp);CHKERRQ(ierr);

  /* Test MatNorm() */
  ierr = MatNorm(C,NORM_1,&Cnorm);
  printf("The 1-norm of C is %f\n",Cnorm);

  /* Test MatMult() */
  ierr = VecCreate(PETSC_COMM_WORLD,&x);CHKERRQ(ierr);
  ierr = VecSetSizes(x,n,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetFromOptions(x);CHKERRQ(ierr);
  ierr = PetscMalloc(ncols*sizeof *vx,&vx);CHKERRQ(ierr);
  for (j=0; j<ncols; j++) {
    vx[j] = (PetscReal)(.01*cols[j]);
  }
  ierr = VecSetValues(x,ncols,cols,vx,INSERT_VALUES);CHKERRQ(ierr);
  ierr = PetscFree(vx);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrows,&rows);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscols,&cols);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(x);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(x);CHKERRQ(ierr);
  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);  
  ierr = VecCreate(PETSC_COMM_WORLD,&y);CHKERRQ(ierr);
  ierr = VecSetSizes(y,m,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetFromOptions(y);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(y);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(y);CHKERRQ(ierr);
  ierr = MatMult(C,x,y);CHKERRQ(ierr);
  ierr = VecView(y,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);  
  ierr = ISDestroy(&isrows);CHKERRQ(ierr);
  ierr = ISDestroy(&iscols);CHKERRQ(ierr);

  /* Test MatAXPY() and MatAYPX() */
  ierr = MatCreate(PETSC_COMM_WORLD,&B);CHKERRQ(ierr);
  ierr = MatSetSizes(B,m,n,PETSC_DECIDE,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = MatSetFromOptions(B);CHKERRQ(ierr);
  ierr = MatSetUp(B);CHKERRQ(ierr);
  ierr = MatGetOwnershipIS(B,&isrows,&iscols);CHKERRQ(ierr);
  ierr = ISGetLocalSize(isrows,&nrows);CHKERRQ(ierr);
  ierr = ISGetIndices(isrows,&rows);CHKERRQ(ierr);
  ierr = ISGetLocalSize(iscols,&ncols);CHKERRQ(ierr);
  ierr = ISGetIndices(iscols,&cols);CHKERRQ(ierr);
  for (i=0; i<nrows; i++) {
    for (j=0; j<ncols; j++) {
      v[i*ncols+j] = (PetscReal)(1000*rows[i]+cols[j]);
    }
   }
  ierr = MatSetValues(B,nrows,rows,ncols,cols,v,INSERT_VALUES);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrows,&rows);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscols,&cols);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatView(B,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = MatAXPY(B,2.5,C,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatView(B,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = MatAYPX(B,3.75,C,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatView(B,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = ISDestroy(&isrows);CHKERRQ(ierr);
  ierr = ISDestroy(&iscols);CHKERRQ(ierr);
  ierr = MatDestroy(&B);CHKERRQ(ierr);
 
  ierr = PetscFree(v);CHKERRQ(ierr);
  ierr = ISDestroy(&isrows);CHKERRQ(ierr);
  ierr = ISDestroy(&iscols);CHKERRQ(ierr);
  ierr = MatDestroy(&C);CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&y);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return 0;
}

 
