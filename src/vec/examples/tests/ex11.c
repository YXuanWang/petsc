#ifndef lint
static char vcid[] = "$Id: ex11.c,v 1.32 1996/07/08 22:16:40 bsmith Exp bsmith $";
#endif

static char help[] = "Scatters from a parallel vector to a sequential vector.\n\n";

#include "petsc.h"
#include "is.h"
#include "vec.h"
#include "sys.h"
#include <math.h>

int main(int argc,char **argv)
{
  int           ierr,size,rank,i,N;
  Scalar        mone = -1.0, value;
  Vec           x,y;
  IS            is1,is2;
  VecScatter    ctx = 0;

  PetscInitialize(&argc,&argv,(char*)0,help); 
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  /* create two vectors */
  ierr = VecCreateMPI(MPI_COMM_WORLD,rank+1,PETSC_DECIDE,&x); CHKERRA(ierr);
  ierr = VecGetSize(x,&N); CHKERRA(ierr);
  ierr = VecCreateSeq(MPI_COMM_SELF,N-rank,&y); CHKERRA(ierr);

  /* create two index sets */
  ierr = ISCreateStride(MPI_COMM_SELF,N-rank,rank,1,&is1);CHKERRA(ierr);
  ierr = ISCreateStride(MPI_COMM_SELF,N-rank,0,1,&is2); CHKERRA(ierr);

  /* fill parallel vector: note this is not efficient way*/
  for ( i=0; i<N; i++ ) {
    value = (Scalar) i;
    ierr = VecSetValues(x,1,&i,&value,INSERT_VALUES); CHKERRA(ierr);
  }
  ierr = VecAssemblyBegin(x); CHKERRA(ierr);
  ierr = VecAssemblyEnd(x); CHKERRA(ierr);
  ierr = VecSet(&mone,y); CHKERRA(ierr);

  ierr = VecView(x,VIEWER_STDOUT_WORLD); CHKERRA(ierr);

  ierr = VecScatterCreate(x,is1,y,is2,&ctx); CHKERRA(ierr);
  ierr = VecScatterBegin(x,y,INSERT_VALUES,SCATTER_ALL,ctx); CHKERRA(ierr);
  ierr = VecScatterEnd(x,y,INSERT_VALUES,SCATTER_ALL,ctx); CHKERRA(ierr);
  ierr = VecScatterDestroy(ctx); CHKERRA(ierr);

  if (!rank) 
    {printf("----\n"); ierr = VecView(y,VIEWER_STDOUT_SELF); CHKERRA(ierr);}

  ierr = ISDestroy(is1); CHKERRA(ierr);
  ierr = ISDestroy(is2); CHKERRA(ierr);
  ierr = VecDestroy(x); CHKERRA(ierr);
  ierr = VecDestroy(y); CHKERRA(ierr);

  PetscFinalize(); 
  return 0;
}
 
