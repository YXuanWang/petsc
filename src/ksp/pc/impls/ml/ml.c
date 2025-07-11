/*
   Provides an interface to the ML smoothed Aggregation
   Note: Something non-obvious breaks -pc_mg_type ADDITIVE for parallel runs
                                    Jed Brown, see [PETSC #18321, #18449].
*/
#include <petsc/private/pcimpl.h>   /*I "petscpc.h" I*/
#include <petsc/private/pcmgimpl.h> /*I "petscksp.h" I*/
#include <../src/mat/impls/aij/seq/aij.h>
#include <../src/mat/impls/aij/mpi/mpiaij.h>
#include <petscdm.h> /* for DMDestroy(&pc->mg) hack */

EXTERN_C_BEGIN
/* HAVE_CONFIG_H flag is required by ML include files */
#ifndef HAVE_CONFIG_H
  #define HAVE_CONFIG_H
#endif
#include <ml_include.h>
#include <ml_viz_stats.h>
EXTERN_C_END

typedef enum {
  PCML_NULLSPACE_AUTO,
  PCML_NULLSPACE_USER,
  PCML_NULLSPACE_BLOCK,
  PCML_NULLSPACE_SCALAR
} PCMLNullSpaceType;
static const char *const PCMLNullSpaceTypes[] = {"AUTO", "USER", "BLOCK", "SCALAR", "PCMLNullSpaceType", "PCML_NULLSPACE_", 0};

/* The context (data structure) at each grid level */
typedef struct {
  Vec x, b, r; /* global vectors */
  Mat A, P, R;
  KSP ksp;
  Vec coords; /* projected by ML, if PCSetCoordinates is called; values packed by node */
} GridCtx;

/* The context used to input PETSc matrix into ML at fine grid */
typedef struct {
  Mat          A;    /* PETSc matrix in aij format */
  Mat          Aloc; /* local portion of A to be used by ML */
  Vec          x, y;
  ML_Operator *mlmat;
  PetscScalar *pwork; /* tmp array used by PetscML_comm() */
} FineGridCtx;

/* The context associates a ML matrix with a PETSc shell matrix */
typedef struct {
  Mat          A;     /* PETSc shell matrix associated with mlmat */
  ML_Operator *mlmat; /* ML matrix assorciated with A */
} Mat_MLShell;

/* Private context for the ML preconditioner */
typedef struct {
  ML               *ml_object;
  ML_Aggregate     *agg_object;
  GridCtx          *gridctx;
  FineGridCtx      *PetscMLdata;
  PetscInt          Nlevels, MaxNlevels, MaxCoarseSize, CoarsenScheme, EnergyMinimization, MinPerProc, PutOnSingleProc, RepartitionType, ZoltanScheme;
  PetscReal         Threshold, DampingFactor, EnergyMinimizationDropTol, MaxMinRatio, AuxThreshold;
  PetscBool         SpectralNormScheme_Anorm, BlockScaling, EnergyMinimizationCheap, Symmetrize, OldHierarchy, KeepAggInfo, Reusable, Repartition, Aux;
  PetscBool         reuse_interpolation;
  PCMLNullSpaceType nulltype;
  PetscMPIInt       size; /* size of communicator for pc->pmat */
  PetscInt          dim;  /* data from PCSetCoordinates(_ML) */
  PetscInt          nloc;
  PetscReal        *coords; /* ML has a grid object for each level: the finest grid will point into coords */
} PC_ML;

static int PetscML_getrow(ML_Operator *ML_data, int N_requested_rows, int requested_rows[], int allocated_space, int columns[], double values[], int row_lengths[])
{
  PetscInt     m, i, j, k = 0, row, *aj;
  PetscScalar *aa;
  FineGridCtx *ml = (FineGridCtx *)ML_Get_MyGetrowData(ML_data);
  Mat_SeqAIJ  *a  = (Mat_SeqAIJ *)ml->Aloc->data;

  if (MatGetSize(ml->Aloc, &m, NULL)) return 0;
  for (i = 0; i < N_requested_rows; i++) {
    row            = requested_rows[i];
    row_lengths[i] = a->ilen[row];
    if (allocated_space < k + row_lengths[i]) return 0;
    if ((row >= 0) || (row <= (m - 1))) {
      aj = a->j + a->i[row];
      aa = a->a + a->i[row];
      for (j = 0; j < row_lengths[i]; j++) {
        columns[k]  = aj[j];
        values[k++] = aa[j];
      }
    }
  }
  return 1;
}

static PetscErrorCode PetscML_comm(double p[], void *ML_data)
{
  FineGridCtx       *ml = (FineGridCtx *)ML_data;
  Mat                A  = ml->A;
  Mat_MPIAIJ        *a  = (Mat_MPIAIJ *)A->data;
  PetscMPIInt        size;
  PetscInt           i, in_length = A->rmap->n, out_length = ml->Aloc->cmap->n;
  const PetscScalar *array;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  if (size == 1) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(VecPlaceArray(ml->y, p));
  PetscCall(VecScatterBegin(a->Mvctx, ml->y, a->lvec, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(a->Mvctx, ml->y, a->lvec, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecResetArray(ml->y));
  PetscCall(VecGetArrayRead(a->lvec, &array));
  for (i = in_length; i < out_length; i++) p[i] = array[i - in_length];
  PetscCall(VecRestoreArrayRead(a->lvec, &array));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Needed since ML expects an int (*)(double *, void *) but PetscErrorCode may be an
  enum. Instead of modifying PetscML_comm() it is easier to just wrap it
*/
static int ML_PetscML_comm(double p[], void *ML_data)
{
  return (int)PetscML_comm(p, ML_data);
}

static int PetscML_matvec(ML_Operator *ML_data, int in_length, double p[], int out_length, double ap[])
{
  FineGridCtx *ml = (FineGridCtx *)ML_Get_MyMatvecData(ML_data);
  Mat          A = ml->A, Aloc = ml->Aloc;
  PetscMPIInt  size;
  PetscScalar *pwork = ml->pwork;
  PetscInt     i;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  if (size == 1) {
    PetscCall(VecPlaceArray(ml->x, p));
  } else {
    for (i = 0; i < in_length; i++) pwork[i] = p[i];
    PetscCall(PetscML_comm(pwork, ml));
    PetscCall(VecPlaceArray(ml->x, pwork));
  }
  PetscCall(VecPlaceArray(ml->y, ap));
  PetscCall(MatMult(Aloc, ml->x, ml->y));
  PetscCall(VecResetArray(ml->x));
  PetscCall(VecResetArray(ml->y));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMult_ML(Mat A, Vec x, Vec y)
{
  Mat_MLShell       *shell;
  PetscScalar       *yarray;
  const PetscScalar *xarray;
  PetscInt           x_length, y_length;

  PetscFunctionBegin;
  PetscCall(MatShellGetContext(A, &shell));
  PetscCall(VecGetArrayRead(x, &xarray));
  PetscCall(VecGetArray(y, &yarray));
  x_length = shell->mlmat->invec_leng;
  y_length = shell->mlmat->outvec_leng;
  PetscStackCallExternalVoid("ML_Operator_Apply", ML_Operator_Apply(shell->mlmat, x_length, (PetscScalar *)xarray, y_length, yarray));
  PetscCall(VecRestoreArrayRead(x, &xarray));
  PetscCall(VecRestoreArray(y, &yarray));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* newtype is ignored since only handles one case */
static PetscErrorCode MatConvert_MPIAIJ_ML(Mat A, MatType newtype, MatReuse scall, Mat *Aloc)
{
  Mat_MPIAIJ  *mpimat = (Mat_MPIAIJ *)A->data;
  Mat_SeqAIJ  *mat, *a = (Mat_SeqAIJ *)mpimat->A->data, *b = (Mat_SeqAIJ *)mpimat->B->data;
  PetscInt    *ai = a->i, *aj = a->j, *bi = b->i, *bj = b->j;
  PetscScalar *aa, *ba, *ca;
  PetscInt     am = A->rmap->n, an = A->cmap->n, i, j, k;
  PetscInt    *ci, *cj, ncols;

  PetscFunctionBegin;
  PetscCheck(am == an, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "A must have a square diagonal portion, am: %d != an: %d", am, an);
  PetscCall(MatSeqAIJGetArrayRead(mpimat->A, (const PetscScalar **)&aa));
  PetscCall(MatSeqAIJGetArrayRead(mpimat->B, (const PetscScalar **)&ba));
  if (scall == MAT_INITIAL_MATRIX) {
    PetscCall(PetscMalloc1(1 + am, &ci));
    ci[0] = 0;
    for (i = 0; i < am; i++) ci[i + 1] = ci[i] + (ai[i + 1] - ai[i]) + (bi[i + 1] - bi[i]);
    PetscCall(PetscMalloc1(1 + ci[am], &cj));
    PetscCall(PetscMalloc1(1 + ci[am], &ca));

    k = 0;
    for (i = 0; i < am; i++) {
      /* diagonal portion of A */
      ncols = ai[i + 1] - ai[i];
      for (j = 0; j < ncols; j++) {
        cj[k]   = *aj++;
        ca[k++] = *aa++;
      }
      /* off-diagonal portion of A */
      ncols = bi[i + 1] - bi[i];
      for (j = 0; j < ncols; j++) {
        cj[k] = an + (*bj);
        bj++;
        ca[k++] = *ba++;
      }
    }
    PetscCheck(k == ci[am], PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "k: %d != ci[am]: %d", k, ci[am]);

    /* put together the new matrix */
    an = mpimat->A->cmap->n + mpimat->B->cmap->n;
    PetscCall(MatCreateSeqAIJWithArrays(PETSC_COMM_SELF, am, an, ci, cj, ca, Aloc));

    /* MatCreateSeqAIJWithArrays flags matrix so PETSc doesn't free the user's arrays. */
    /* Since these are PETSc arrays, change flags to free them as necessary. */
    mat          = (Mat_SeqAIJ *)(*Aloc)->data;
    mat->free_a  = PETSC_TRUE;
    mat->free_ij = PETSC_TRUE;

    mat->nonew = 0;
  } else if (scall == MAT_REUSE_MATRIX) {
    mat = (Mat_SeqAIJ *)(*Aloc)->data;
    ci  = mat->i;
    cj  = mat->j;
    ca  = mat->a;
    for (i = 0; i < am; i++) {
      /* diagonal portion of A */
      ncols = ai[i + 1] - ai[i];
      for (j = 0; j < ncols; j++) *ca++ = *aa++;
      /* off-diagonal portion of A */
      ncols = bi[i + 1] - bi[i];
      for (j = 0; j < ncols; j++) *ca++ = *ba++;
    }
  } else SETERRQ(PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONG, "Invalid MatReuse %d", (int)scall);
  PetscCall(MatSeqAIJRestoreArrayRead(mpimat->A, (const PetscScalar **)&aa));
  PetscCall(MatSeqAIJRestoreArrayRead(mpimat->B, (const PetscScalar **)&ba));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDestroy_ML(Mat A)
{
  Mat_MLShell *shell;

  PetscFunctionBegin;
  PetscCall(MatShellGetContext(A, &shell));
  PetscCall(PetscFree(shell));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatWrapML_SeqAIJ(ML_Operator *mlmat, MatReuse reuse, Mat *newmat)
{
  struct ML_CSR_MSRdata *matdata = (struct ML_CSR_MSRdata *)mlmat->data;
  PetscInt               m = mlmat->outvec_leng, n = mlmat->invec_leng, *nnz = NULL, nz_max;
  PetscInt              *ml_cols = matdata->columns, *ml_rowptr = matdata->rowptr, *aj, i;
  PetscScalar           *ml_vals = matdata->values, *aa;

  PetscFunctionBegin;
  PetscCheck(mlmat->getrow, PETSC_COMM_SELF, PETSC_ERR_ARG_NULL, "mlmat->getrow = NULL");
  if (m != n) { /* ML Pmat and Rmat are in CSR format. Pass array pointers into SeqAIJ matrix */
    if (reuse) {
      Mat_SeqAIJ *aij = (Mat_SeqAIJ *)(*newmat)->data;
      aij->i          = ml_rowptr;
      aij->j          = ml_cols;
      aij->a          = ml_vals;
    } else {
      /* sort ml_cols and ml_vals */
      PetscCall(PetscMalloc1(m + 1, &nnz));
      for (i = 0; i < m; i++) nnz[i] = ml_rowptr[i + 1] - ml_rowptr[i];
      aj = ml_cols;
      aa = ml_vals;
      for (i = 0; i < m; i++) {
        PetscCall(PetscSortIntWithScalarArray(nnz[i], aj, aa));
        aj += nnz[i];
        aa += nnz[i];
      }
      PetscCall(MatCreateSeqAIJWithArrays(PETSC_COMM_SELF, m, n, ml_rowptr, ml_cols, ml_vals, newmat));
      PetscCall(PetscFree(nnz));
    }
    PetscCall(MatAssemblyBegin(*newmat, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(*newmat, MAT_FINAL_ASSEMBLY));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  nz_max = PetscMax(1, mlmat->max_nz_per_row);
  PetscCall(PetscMalloc2(nz_max, &aa, nz_max, &aj));
  if (!reuse) {
    PetscCall(MatCreate(PETSC_COMM_SELF, newmat));
    PetscCall(MatSetSizes(*newmat, m, n, PETSC_DECIDE, PETSC_DECIDE));
    PetscCall(MatSetType(*newmat, MATSEQAIJ));
    /* keep track of block size for A matrices */
    PetscCall(MatSetBlockSize(*newmat, mlmat->num_PDEs));

    PetscCall(PetscMalloc1(m, &nnz));
    for (i = 0; i < m; i++) PetscStackCallExternalVoid("ML_Operator_Getrow", ML_Operator_Getrow(mlmat, 1, &i, nz_max, aj, aa, &nnz[i]));
    PetscCall(MatSeqAIJSetPreallocation(*newmat, 0, nnz));
  }
  for (i = 0; i < m; i++) {
    PetscInt ncols;

    PetscStackCallExternalVoid("ML_Operator_Getrow", ML_Operator_Getrow(mlmat, 1, &i, nz_max, aj, aa, &ncols));
    PetscCall(MatSetValues(*newmat, 1, &i, ncols, aj, aa, INSERT_VALUES));
  }
  PetscCall(MatAssemblyBegin(*newmat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(*newmat, MAT_FINAL_ASSEMBLY));

  PetscCall(PetscFree2(aa, aj));
  PetscCall(PetscFree(nnz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatWrapML_SHELL(ML_Operator *mlmat, MatReuse reuse, Mat *newmat)
{
  PetscInt     m, n;
  ML_Comm     *MLcomm;
  Mat_MLShell *shellctx;

  PetscFunctionBegin;
  m = mlmat->outvec_leng;
  n = mlmat->invec_leng;

  if (reuse) {
    PetscCall(MatShellGetContext(*newmat, &shellctx));
    shellctx->mlmat = mlmat;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  MLcomm = mlmat->comm;

  PetscCall(PetscNew(&shellctx));
  PetscCall(MatCreateShell(MLcomm->USR_comm, m, n, PETSC_DETERMINE, PETSC_DETERMINE, shellctx, newmat));
  PetscCall(MatShellSetOperation(*newmat, MATOP_MULT, (void (*)(void))MatMult_ML));
  PetscCall(MatShellSetOperation(*newmat, MATOP_DESTROY, (void (*)(void))MatDestroy_ML));

  shellctx->A     = *newmat;
  shellctx->mlmat = mlmat;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatWrapML_MPIAIJ(ML_Operator *mlmat, MatReuse reuse, Mat *newmat)
{
  PetscInt    *aj;
  PetscScalar *aa;
  PetscInt     i, j, *gordering;
  PetscInt     m = mlmat->outvec_leng, n, nz_max, row;
  Mat          A;

  PetscFunctionBegin;
  PetscCheck(mlmat->getrow, PETSC_COMM_SELF, PETSC_ERR_ARG_NULL, "mlmat->getrow = NULL");
  n = mlmat->invec_leng;
  PetscCheck(m == n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "m %d must equal to n %d", m, n);

  /* create global row numbering for a ML_Operator */
  PetscStackCallExternalVoid("ML_build_global_numbering", ML_build_global_numbering(mlmat, &gordering, "rows"));

  nz_max = PetscMax(1, mlmat->max_nz_per_row) + 1;
  PetscCall(PetscMalloc2(nz_max, &aa, nz_max, &aj));
  if (reuse) {
    A = *newmat;
  } else {
    PetscInt *nnzA, *nnzB, *nnz;
    PetscInt  rstart;
    PetscCall(MatCreate(mlmat->comm->USR_comm, &A));
    PetscCall(MatSetSizes(A, m, n, PETSC_DECIDE, PETSC_DECIDE));
    PetscCall(MatSetType(A, MATMPIAIJ));
    /* keep track of block size for A matrices */
    PetscCall(MatSetBlockSize(A, mlmat->num_PDEs));
    PetscCall(PetscMalloc3(m, &nnzA, m, &nnzB, m, &nnz));
    PetscCallMPI(MPI_Scan(&m, &rstart, 1, MPIU_INT, MPI_SUM, mlmat->comm->USR_comm));
    rstart -= m;

    for (i = 0; i < m; i++) {
      row = gordering[i] - rstart;
      PetscStackCallExternalVoid("ML_Operator_Getrow", ML_Operator_Getrow(mlmat, 1, &i, nz_max, aj, aa, &nnz[i]));
      nnzA[row] = 0;
      for (j = 0; j < nnz[i]; j++) {
        if (aj[j] < m) nnzA[row]++;
      }
      nnzB[row] = nnz[i] - nnzA[row];
    }
    PetscCall(MatMPIAIJSetPreallocation(A, 0, nnzA, 0, nnzB));
    PetscCall(PetscFree3(nnzA, nnzB, nnz));
  }
  for (i = 0; i < m; i++) {
    PetscInt ncols;
    row = gordering[i];

    PetscStackCallExternalVoid(",ML_Operator_Getrow", ML_Operator_Getrow(mlmat, 1, &i, nz_max, aj, aa, &ncols));
    for (j = 0; j < ncols; j++) aj[j] = gordering[aj[j]];
    PetscCall(MatSetValues(A, 1, &row, ncols, aj, aa, INSERT_VALUES));
  }
  PetscStackCallExternalVoid("ML_free", ML_free(gordering));
  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  *newmat = A;

  PetscCall(PetscFree2(aa, aj));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   PCSetCoordinates_ML

   Input Parameter:
   .  pc - the preconditioner context
*/
static PetscErrorCode PCSetCoordinates_ML(PC pc, PetscInt ndm, PetscInt a_nloc, PetscReal *coords)
{
  PC_MG   *mg    = (PC_MG *)pc->data;
  PC_ML   *pc_ml = (PC_ML *)mg->innerctx;
  PetscInt arrsz, oldarrsz, bs, my0, kk, ii, nloc, Iend, aloc;
  Mat      Amat = pc->pmat;

  /* this function copied and modified from PCSetCoordinates_GEO -TGI */
  PetscFunctionBegin;
  PetscValidHeaderSpecific(Amat, MAT_CLASSID, 1);
  PetscCall(MatGetBlockSize(Amat, &bs));

  PetscCall(MatGetOwnershipRange(Amat, &my0, &Iend));
  aloc = (Iend - my0);
  nloc = (Iend - my0) / bs;

  PetscCheck((nloc == a_nloc) || (aloc == a_nloc), PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Number of local blocks %" PetscInt_FMT " must be %" PetscInt_FMT " or %" PetscInt_FMT ".", a_nloc, nloc, aloc);

  oldarrsz    = pc_ml->dim * pc_ml->nloc;
  pc_ml->dim  = ndm;
  pc_ml->nloc = nloc;
  arrsz       = ndm * nloc;

  /* create data - syntactic sugar that should be refactored at some point */
  if (pc_ml->coords == 0 || (oldarrsz != arrsz)) {
    PetscCall(PetscFree(pc_ml->coords));
    PetscCall(PetscMalloc1(arrsz, &pc_ml->coords));
  }
  for (kk = 0; kk < arrsz; kk++) pc_ml->coords[kk] = -999.;
  /* copy data in - column-oriented */
  if (nloc == a_nloc) {
    for (kk = 0; kk < nloc; kk++) {
      for (ii = 0; ii < ndm; ii++) pc_ml->coords[ii * nloc + kk] = coords[kk * ndm + ii];
    }
  } else { /* assumes the coordinates are blocked */
    for (kk = 0; kk < nloc; kk++) {
      for (ii = 0; ii < ndm; ii++) pc_ml->coords[ii * nloc + kk] = coords[bs * kk * ndm + ii];
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCReset_ML(PC pc)
{
  PC_MG   *mg    = (PC_MG *)pc->data;
  PC_ML   *pc_ml = (PC_ML *)mg->innerctx;
  PetscInt level, fine_level = pc_ml->Nlevels - 1, dim = pc_ml->dim;

  PetscFunctionBegin;
  if (dim) {
    for (level = 0; level <= fine_level; level++) PetscCall(VecDestroy(&pc_ml->gridctx[level].coords));
    if (pc_ml->ml_object && pc_ml->ml_object->Grid) {
      ML_Aggregate_Viz_Stats *grid_info = (ML_Aggregate_Viz_Stats *)pc_ml->ml_object->Grid[0].Grid;
      grid_info->x                      = 0; /* do this so ML doesn't try to free coordinates */
      grid_info->y                      = 0;
      grid_info->z                      = 0;
      PetscStackCallExternalVoid("ML_Operator_Getrow", ML_Aggregate_VizAndStats_Clean(pc_ml->ml_object));
    }
  }
  PetscStackCallExternalVoid("ML_Aggregate_Destroy", ML_Aggregate_Destroy(&pc_ml->agg_object));
  PetscStackCallExternalVoid("ML_Aggregate_Destroy", ML_Destroy(&pc_ml->ml_object));

  if (pc_ml->PetscMLdata) {
    PetscCall(PetscFree(pc_ml->PetscMLdata->pwork));
    PetscCall(MatDestroy(&pc_ml->PetscMLdata->Aloc));
    PetscCall(VecDestroy(&pc_ml->PetscMLdata->x));
    PetscCall(VecDestroy(&pc_ml->PetscMLdata->y));
  }
  PetscCall(PetscFree(pc_ml->PetscMLdata));

  if (pc_ml->gridctx) {
    for (level = 0; level < fine_level; level++) {
      if (pc_ml->gridctx[level].A) PetscCall(MatDestroy(&pc_ml->gridctx[level].A));
      if (pc_ml->gridctx[level].P) PetscCall(MatDestroy(&pc_ml->gridctx[level].P));
      if (pc_ml->gridctx[level].R) PetscCall(MatDestroy(&pc_ml->gridctx[level].R));
      if (pc_ml->gridctx[level].x) PetscCall(VecDestroy(&pc_ml->gridctx[level].x));
      if (pc_ml->gridctx[level].b) PetscCall(VecDestroy(&pc_ml->gridctx[level].b));
      if (pc_ml->gridctx[level + 1].r) PetscCall(VecDestroy(&pc_ml->gridctx[level + 1].r));
    }
  }
  PetscCall(PetscFree(pc_ml->gridctx));
  PetscCall(PetscFree(pc_ml->coords));

  pc_ml->dim  = 0;
  pc_ml->nloc = 0;
  PetscCall(PCReset_MG(pc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   PCSetUp_ML - Prepares for the use of the ML preconditioner
                    by setting data structures and options.

   Input Parameter:
.  pc - the preconditioner context

   Application Interface Routine: PCSetUp()

   Note:
   The interface routine PCSetUp() is not usually called directly by
   the user, but instead is called by PCApply() if necessary.
*/
static PetscErrorCode PCSetUp_ML(PC pc)
{
  PetscMPIInt      size;
  FineGridCtx     *PetscMLdata;
  ML              *ml_object;
  ML_Aggregate    *agg_object;
  ML_Operator     *mlmat;
  PetscInt         nlocal_allcols, Nlevels, mllevel, level, level1, m, fine_level, bs;
  Mat              A, Aloc;
  GridCtx         *gridctx;
  PC_MG           *mg    = (PC_MG *)pc->data;
  PC_ML           *pc_ml = (PC_ML *)mg->innerctx;
  PetscBool        isSeq, isMPI;
  KSP              smoother;
  PC               subpc;
  PetscInt         mesh_level, old_mesh_level;
  MatInfo          info;
  static PetscBool cite = PETSC_FALSE;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister("@TechReport{ml_users_guide,\n  author = {M. Sala and J.J. Hu and R.S. Tuminaro},\n  title = {{ML}3.1 {S}moothed {A}ggregation {U}ser's {G}uide},\n  institution =  {Sandia National Laboratories},\n  number = "
                                   "{SAND2004-4821},\n  year = 2004\n}\n",
                                   &cite));
  A = pc->pmat;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));

  if (pc->setupcalled) {
    if (pc->flag == SAME_NONZERO_PATTERN && pc_ml->reuse_interpolation) {
      /*
       Reuse interpolaton instead of recomputing aggregates and updating the whole hierarchy. This is less expensive for
       multiple solves in which the matrix is not changing too quickly.
       */
      ml_object             = pc_ml->ml_object;
      gridctx               = pc_ml->gridctx;
      Nlevels               = pc_ml->Nlevels;
      fine_level            = Nlevels - 1;
      gridctx[fine_level].A = A;

      PetscCall(PetscObjectBaseTypeCompare((PetscObject)A, MATSEQAIJ, &isSeq));
      PetscCall(PetscObjectBaseTypeCompare((PetscObject)A, MATMPIAIJ, &isMPI));
      PetscCheck(isMPI || isSeq, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_WRONG, "Matrix type '%s' cannot be used with ML. ML can only handle AIJ matrices.", ((PetscObject)A)->type_name);
      if (isMPI) {
        PetscCall(MatConvert_MPIAIJ_ML(A, NULL, MAT_INITIAL_MATRIX, &Aloc));
      } else {
        Aloc = A;
        PetscCall(PetscObjectReference((PetscObject)Aloc));
      }

      PetscCall(MatGetSize(Aloc, &m, &nlocal_allcols));
      PetscMLdata = pc_ml->PetscMLdata;
      PetscCall(MatDestroy(&PetscMLdata->Aloc));
      PetscMLdata->A    = A;
      PetscMLdata->Aloc = Aloc;
      PetscStackCallExternalVoid("ML_Aggregate_Destroy", ML_Init_Amatrix(ml_object, 0, m, m, PetscMLdata));
      PetscStackCallExternalVoid("ML_Set_Amatrix_Matvec", ML_Set_Amatrix_Matvec(ml_object, 0, PetscML_matvec));

      mesh_level = ml_object->ML_finest_level;
      while (ml_object->SingleLevel[mesh_level].Rmat->to) {
        old_mesh_level = mesh_level;
        mesh_level     = ml_object->SingleLevel[mesh_level].Rmat->to->levelnum;

        /* clean and regenerate A */
        mlmat = &ml_object->Amat[mesh_level];
        PetscStackCallExternalVoid("ML_Operator_Clean", ML_Operator_Clean(mlmat));
        PetscStackCallExternalVoid("ML_Operator_Init", ML_Operator_Init(mlmat, ml_object->comm));
        PetscStackCallExternalVoid("ML_Gen_AmatrixRAP", ML_Gen_AmatrixRAP(ml_object, old_mesh_level, mesh_level));
      }

      level = fine_level - 1;
      if (size == 1) { /* convert ML P, R and A into seqaij format */
        for (mllevel = 1; mllevel < Nlevels; mllevel++) {
          mlmat = &ml_object->Amat[mllevel];
          PetscCall(MatWrapML_SeqAIJ(mlmat, MAT_REUSE_MATRIX, &gridctx[level].A));
          level--;
        }
      } else { /* convert ML P and R into shell format, ML A into mpiaij format */
        for (mllevel = 1; mllevel < Nlevels; mllevel++) {
          mlmat = &ml_object->Amat[mllevel];
          PetscCall(MatWrapML_MPIAIJ(mlmat, MAT_REUSE_MATRIX, &gridctx[level].A));
          level--;
        }
      }

      for (level = 0; level < fine_level; level++) {
        if (level > 0) PetscCall(PCMGSetResidual(pc, level, PCMGResidualDefault, gridctx[level].A));
        PetscCall(KSPSetOperators(gridctx[level].ksp, gridctx[level].A, gridctx[level].A));
      }
      PetscCall(PCMGSetResidual(pc, fine_level, PCMGResidualDefault, gridctx[fine_level].A));
      PetscCall(KSPSetOperators(gridctx[fine_level].ksp, gridctx[level].A, gridctx[fine_level].A));

      PetscCall(PCSetUp_MG(pc));
      PetscFunctionReturn(PETSC_SUCCESS);
    } else {
      /* since ML can change the size of vectors/matrices at any level we must destroy everything */
      PetscCall(PCReset_ML(pc));
    }
  }

  /* setup special features of PCML */
  /* convert A to Aloc to be used by ML at fine grid */
  pc_ml->size = size;
  PetscCall(PetscObjectBaseTypeCompare((PetscObject)A, MATSEQAIJ, &isSeq));
  PetscCall(PetscObjectBaseTypeCompare((PetscObject)A, MATMPIAIJ, &isMPI));
  PetscCheck(isMPI || isSeq, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_WRONG, "Matrix type '%s' cannot be used with ML. ML can only handle AIJ matrices.", ((PetscObject)A)->type_name);
  if (isMPI) {
    PetscCall(MatConvert_MPIAIJ_ML(A, NULL, MAT_INITIAL_MATRIX, &Aloc));
  } else {
    Aloc = A;
    PetscCall(PetscObjectReference((PetscObject)Aloc));
  }

  /* create and initialize struct 'PetscMLdata' */
  PetscCall(PetscNew(&PetscMLdata));
  pc_ml->PetscMLdata = PetscMLdata;
  PetscCall(PetscMalloc1(Aloc->cmap->n + 1, &PetscMLdata->pwork));

  PetscCall(MatCreateVecs(Aloc, &PetscMLdata->x, &PetscMLdata->y));

  PetscMLdata->A    = A;
  PetscMLdata->Aloc = Aloc;
  if (pc_ml->dim) { /* create vecs around the coordinate data given */
    PetscInt   i, j, dim = pc_ml->dim;
    PetscInt   nloc = pc_ml->nloc, nlocghost;
    PetscReal *ghostedcoords;

    PetscCall(MatGetBlockSize(A, &bs));
    nlocghost = Aloc->cmap->n / bs;
    PetscCall(PetscMalloc1(dim * nlocghost, &ghostedcoords));
    for (i = 0; i < dim; i++) {
      /* copy coordinate values into first component of pwork */
      for (j = 0; j < nloc; j++) PetscMLdata->pwork[bs * j] = pc_ml->coords[nloc * i + j];
      /* get the ghost values */
      PetscCall(PetscML_comm(PetscMLdata->pwork, PetscMLdata));
      /* write into the vector */
      for (j = 0; j < nlocghost; j++) ghostedcoords[i * nlocghost + j] = PetscMLdata->pwork[bs * j];
    }
    /* replace the original coords with the ghosted coords, because these are
     * what ML needs */
    PetscCall(PetscFree(pc_ml->coords));
    pc_ml->coords = ghostedcoords;
  }

  /* create ML discretization matrix at fine grid */
  /* ML requires input of fine-grid matrix. It determines nlevels. */
  PetscCall(MatGetSize(Aloc, &m, &nlocal_allcols));
  PetscCall(MatGetBlockSize(A, &bs));
  PetscStackCallExternalVoid("ML_Create", ML_Create(&ml_object, pc_ml->MaxNlevels));
  PetscStackCallExternalVoid("ML_Comm_Set_UsrComm", ML_Comm_Set_UsrComm(ml_object->comm, PetscObjectComm((PetscObject)A)));
  pc_ml->ml_object = ml_object;
  PetscStackCallExternalVoid("ML_Init_Amatrix", ML_Init_Amatrix(ml_object, 0, m, m, PetscMLdata));
  PetscStackCallExternalVoid("ML_Set_Amatrix_Getrow", ML_Set_Amatrix_Getrow(ml_object, 0, PetscML_getrow, ML_PetscML_comm, nlocal_allcols));
  PetscStackCallExternalVoid("ML_Set_Amatrix_Matvec", ML_Set_Amatrix_Matvec(ml_object, 0, PetscML_matvec));

  PetscStackCallExternalVoid("ML_Set_Symmetrize", ML_Set_Symmetrize(ml_object, pc_ml->Symmetrize ? ML_YES : ML_NO));

  /* aggregation */
  PetscStackCallExternalVoid("ML_Aggregate_Create", ML_Aggregate_Create(&agg_object));
  pc_ml->agg_object = agg_object;

  {
    MatNullSpace mnull;
    PetscCall(MatGetNearNullSpace(A, &mnull));
    if (pc_ml->nulltype == PCML_NULLSPACE_AUTO) {
      if (mnull) pc_ml->nulltype = PCML_NULLSPACE_USER;
      else if (bs > 1) pc_ml->nulltype = PCML_NULLSPACE_BLOCK;
      else pc_ml->nulltype = PCML_NULLSPACE_SCALAR;
    }
    switch (pc_ml->nulltype) {
    case PCML_NULLSPACE_USER: {
      PetscScalar       *nullvec;
      const PetscScalar *v;
      PetscBool          has_const;
      PetscInt           i, j, mlocal, nvec, M;
      const Vec         *vecs;

      PetscCheck(mnull, PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "Must provide explicit null space using MatSetNearNullSpace() to use user-specified null space");
      PetscCall(MatGetSize(A, &M, NULL));
      PetscCall(MatGetLocalSize(Aloc, &mlocal, NULL));
      PetscCall(MatNullSpaceGetVecs(mnull, &has_const, &nvec, &vecs));
      PetscCall(PetscMalloc1((nvec + !!has_const) * mlocal, &nullvec));
      if (has_const)
        for (i = 0; i < mlocal; i++) nullvec[i] = 1.0 / M;
      for (i = 0; i < nvec; i++) {
        PetscCall(VecGetArrayRead(vecs[i], &v));
        for (j = 0; j < mlocal; j++) nullvec[(i + !!has_const) * mlocal + j] = v[j];
        PetscCall(VecRestoreArrayRead(vecs[i], &v));
      }
      PetscStackCallExternalVoid("ML_Aggregate_Set_NullSpace", ML_Aggregate_Set_NullSpace(agg_object, bs, nvec + !!has_const, nullvec, mlocal));
      PetscCall(PetscFree(nullvec));
    } break;
    case PCML_NULLSPACE_BLOCK:
      PetscStackCallExternalVoid("ML_Aggregate_Set_NullSpace", ML_Aggregate_Set_NullSpace(agg_object, bs, bs, 0, 0));
      break;
    case PCML_NULLSPACE_SCALAR:
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_SUP, "Unknown null space type");
    }
  }
  PetscStackCallExternalVoid("ML_Aggregate_Set_MaxCoarseSize", ML_Aggregate_Set_MaxCoarseSize(agg_object, pc_ml->MaxCoarseSize));
  /* set options */
  switch (pc_ml->CoarsenScheme) {
  case 1:
    PetscStackCallExternalVoid("ML_Aggregate_Set_CoarsenScheme_Coupled", ML_Aggregate_Set_CoarsenScheme_Coupled(agg_object));
    break;
  case 2:
    PetscStackCallExternalVoid("ML_Aggregate_Set_CoarsenScheme_MIS", ML_Aggregate_Set_CoarsenScheme_MIS(agg_object));
    break;
  case 3:
    PetscStackCallExternalVoid("ML_Aggregate_Set_CoarsenScheme_METIS", ML_Aggregate_Set_CoarsenScheme_METIS(agg_object));
    break;
  }
  PetscStackCallExternalVoid("ML_Aggregate_Set_Threshold", ML_Aggregate_Set_Threshold(agg_object, pc_ml->Threshold));
  PetscStackCallExternalVoid("ML_Aggregate_Set_DampingFactor", ML_Aggregate_Set_DampingFactor(agg_object, pc_ml->DampingFactor));
  if (pc_ml->SpectralNormScheme_Anorm) PetscStackCallExternalVoid("ML_Set_SpectralNormScheme_Anorm", ML_Set_SpectralNormScheme_Anorm(ml_object));
  agg_object->keep_agg_information      = (int)pc_ml->KeepAggInfo;
  agg_object->keep_P_tentative          = (int)pc_ml->Reusable;
  agg_object->block_scaled_SA           = (int)pc_ml->BlockScaling;
  agg_object->minimizing_energy         = (int)pc_ml->EnergyMinimization;
  agg_object->minimizing_energy_droptol = (double)pc_ml->EnergyMinimizationDropTol;
  agg_object->cheap_minimizing_energy   = (int)pc_ml->EnergyMinimizationCheap;

  if (pc_ml->Aux) {
    PetscCheck(pc_ml->dim, PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "Auxiliary matrix requires coordinates");
    ml_object->Amat[0].aux_data->threshold = pc_ml->AuxThreshold;
    ml_object->Amat[0].aux_data->enable    = 1;
    ml_object->Amat[0].aux_data->max_level = 10;
    ml_object->Amat[0].num_PDEs            = bs;
  }

  PetscCall(MatGetInfo(A, MAT_LOCAL, &info));
  ml_object->Amat[0].N_nonzeros = (int)info.nz_used;

  if (pc_ml->dim) {
    PetscInt                i, dim = pc_ml->dim;
    ML_Aggregate_Viz_Stats *grid_info;
    PetscInt                nlocghost;

    PetscCall(MatGetBlockSize(A, &bs));
    nlocghost = Aloc->cmap->n / bs;

    PetscStackCallExternalVoid("ML_Aggregate_VizAndStats_Setup(", ML_Aggregate_VizAndStats_Setup(ml_object)); /* create ml info for coords */
    grid_info = (ML_Aggregate_Viz_Stats *)ml_object->Grid[0].Grid;
    for (i = 0; i < dim; i++) {
      /* set the finest level coordinates to point to the column-order array
       * in pc_ml */
      /* NOTE: must point away before VizAndStats_Clean so ML doesn't free */
      switch (i) {
      case 0:
        grid_info->x = pc_ml->coords + nlocghost * i;
        break;
      case 1:
        grid_info->y = pc_ml->coords + nlocghost * i;
        break;
      case 2:
        grid_info->z = pc_ml->coords + nlocghost * i;
        break;
      default:
        SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_SIZ, "PCML coordinate dimension must be <= 3");
      }
    }
    grid_info->Ndim = dim;
  }

  /* repartitioning */
  if (pc_ml->Repartition) {
    PetscStackCallExternalVoid("ML_Repartition_Activate", ML_Repartition_Activate(ml_object));
    PetscStackCallExternalVoid("ML_Repartition_Set_LargestMinMaxRatio", ML_Repartition_Set_LargestMinMaxRatio(ml_object, pc_ml->MaxMinRatio));
    PetscStackCallExternalVoid("ML_Repartition_Set_MinPerProc", ML_Repartition_Set_MinPerProc(ml_object, pc_ml->MinPerProc));
    PetscStackCallExternalVoid("ML_Repartition_Set_PutOnSingleProc", ML_Repartition_Set_PutOnSingleProc(ml_object, pc_ml->PutOnSingleProc));
#if 0 /* Function not yet defined in ml-6.2 */
    /* I'm not sure what compatibility issues might crop up if we partitioned
     * on the finest level, so to be safe repartition starts on the next
     * finest level (reflection default behavior in
     * ml_MultiLevelPreconditioner) */
    PetscStackCallExternalVoid("ML_Repartition_Set_StartLevel",ML_Repartition_Set_StartLevel(ml_object,1));
#endif

    if (!pc_ml->RepartitionType) {
      PetscInt i;

      PetscCheck(pc_ml->dim, PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "ML Zoltan repartitioning requires coordinates");
      PetscStackCallExternalVoid("ML_Repartition_Set_Partitioner", ML_Repartition_Set_Partitioner(ml_object, ML_USEZOLTAN));
      PetscStackCallExternalVoid("ML_Aggregate_Set_Dimensions", ML_Aggregate_Set_Dimensions(agg_object, pc_ml->dim));

      for (i = 0; i < ml_object->ML_num_levels; i++) {
        ML_Aggregate_Viz_Stats *grid_info = (ML_Aggregate_Viz_Stats *)ml_object->Grid[i].Grid;
        grid_info->zoltan_type            = pc_ml->ZoltanScheme + 1; /* ml numbers options 1, 2, 3 */
        /* defaults from ml_agg_info.c */
        grid_info->zoltan_estimated_its = 40; /* only relevant to hypergraph / fast hypergraph */
        grid_info->zoltan_timers        = 0;
        grid_info->smoothing_steps      = 4; /* only relevant to hypergraph / fast hypergraph */
      }
    } else {
      PetscStackCallExternalVoid("ML_Repartition_Set_Partitioner", ML_Repartition_Set_Partitioner(ml_object, ML_USEPARMETIS));
    }
  }

  if (pc_ml->OldHierarchy) {
    PetscStackCallExternalVoid("ML_Gen_MGHierarchy_UsingAggregation", Nlevels = ML_Gen_MGHierarchy_UsingAggregation(ml_object, 0, ML_INCREASING, agg_object));
  } else {
    PetscStackCallExternalVoid("ML_Gen_MultiLevelHierarchy_UsingAggregation", Nlevels = ML_Gen_MultiLevelHierarchy_UsingAggregation(ml_object, 0, ML_INCREASING, agg_object));
  }
  PetscCheck(Nlevels > 0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Nlevels %d must > 0", Nlevels);
  pc_ml->Nlevels = Nlevels;
  fine_level     = Nlevels - 1;

  PetscCall(PCMGSetLevels(pc, Nlevels, NULL));
  /* set default smoothers */
  for (level = 1; level <= fine_level; level++) {
    PetscCall(PCMGGetSmoother(pc, level, &smoother));
    PetscCall(KSPSetType(smoother, KSPRICHARDSON));
    PetscCall(KSPGetPC(smoother, &subpc));
    PetscCall(PCSetType(subpc, PCSOR));
  }
  PetscObjectOptionsBegin((PetscObject)pc);
  PetscCall(PCSetFromOptions_MG(pc, PetscOptionsObject)); /* should be called in PCSetFromOptions_ML(), but cannot be called prior to PCMGSetLevels() */
  PetscOptionsEnd();

  PetscCall(PetscMalloc1(Nlevels, &gridctx));

  pc_ml->gridctx = gridctx;

  /* wrap ML matrices by PETSc shell matrices at coarsened grids.
     Level 0 is the finest grid for ML, but coarsest for PETSc! */
  gridctx[fine_level].A = A;

  level = fine_level - 1;
  /* TODO: support for GPUs */
  if (size == 1) { /* convert ML P, R and A into seqaij format */
    for (mllevel = 1; mllevel < Nlevels; mllevel++) {
      mlmat = &ml_object->Pmat[mllevel];
      PetscCall(MatWrapML_SeqAIJ(mlmat, MAT_INITIAL_MATRIX, &gridctx[level].P));
      mlmat = &ml_object->Rmat[mllevel - 1];
      PetscCall(MatWrapML_SeqAIJ(mlmat, MAT_INITIAL_MATRIX, &gridctx[level].R));

      mlmat = &ml_object->Amat[mllevel];
      PetscCall(MatWrapML_SeqAIJ(mlmat, MAT_INITIAL_MATRIX, &gridctx[level].A));
      level--;
    }
  } else { /* convert ML P and R into shell format, ML A into mpiaij format */
    for (mllevel = 1; mllevel < Nlevels; mllevel++) {
      mlmat = &ml_object->Pmat[mllevel];
      PetscCall(MatWrapML_SHELL(mlmat, MAT_INITIAL_MATRIX, &gridctx[level].P));
      mlmat = &ml_object->Rmat[mllevel - 1];
      PetscCall(MatWrapML_SHELL(mlmat, MAT_INITIAL_MATRIX, &gridctx[level].R));

      mlmat = &ml_object->Amat[mllevel];
      PetscCall(MatWrapML_MPIAIJ(mlmat, MAT_INITIAL_MATRIX, &gridctx[level].A));
      level--;
    }
  }

  /* create vectors and ksp at all levels */
  for (level = 0; level < fine_level; level++) {
    level1 = level + 1;

    PetscCall(MatCreateVecs(gridctx[level].A, &gridctx[level].x, &gridctx[level].b));
    PetscCall(MatCreateVecs(gridctx[level1].A, NULL, &gridctx[level1].r));
    PetscCall(PCMGSetX(pc, level, gridctx[level].x));
    PetscCall(PCMGSetRhs(pc, level, gridctx[level].b));
    PetscCall(PCMGSetR(pc, level1, gridctx[level1].r));

    if (level == 0) {
      PetscCall(PCMGGetCoarseSolve(pc, &gridctx[level].ksp));
    } else {
      PetscCall(PCMGGetSmoother(pc, level, &gridctx[level].ksp));
    }
  }
  PetscCall(PCMGGetSmoother(pc, fine_level, &gridctx[fine_level].ksp));

  /* create coarse level and the interpolation between the levels */
  for (level = 0; level < fine_level; level++) {
    level1 = level + 1;

    PetscCall(PCMGSetInterpolation(pc, level1, gridctx[level].P));
    PetscCall(PCMGSetRestriction(pc, level1, gridctx[level].R));
    if (level > 0) PetscCall(PCMGSetResidual(pc, level, PCMGResidualDefault, gridctx[level].A));
    PetscCall(KSPSetOperators(gridctx[level].ksp, gridctx[level].A, gridctx[level].A));
  }
  PetscCall(PCMGSetResidual(pc, fine_level, PCMGResidualDefault, gridctx[fine_level].A));
  PetscCall(KSPSetOperators(gridctx[fine_level].ksp, gridctx[level].A, gridctx[fine_level].A));

  /* put coordinate info in levels */
  if (pc_ml->dim) {
    PetscInt   i, j, dim = pc_ml->dim;
    PetscInt   bs, nloc;
    PC         subpc;
    PetscReal *array;

    level = fine_level;
    for (mllevel = 0; mllevel < Nlevels; mllevel++) {
      ML_Aggregate_Viz_Stats *grid_info = (ML_Aggregate_Viz_Stats *)ml_object->Amat[mllevel].to->Grid->Grid;
      MPI_Comm                comm      = ((PetscObject)gridctx[level].A)->comm;

      PetscCall(MatGetBlockSize(gridctx[level].A, &bs));
      PetscCall(MatGetLocalSize(gridctx[level].A, NULL, &nloc));
      nloc /= bs; /* number of local nodes */

      PetscCall(VecCreate(comm, &gridctx[level].coords));
      PetscCall(VecSetSizes(gridctx[level].coords, dim * nloc, PETSC_DECIDE));
      PetscCall(VecSetType(gridctx[level].coords, VECMPI));
      PetscCall(VecGetArray(gridctx[level].coords, &array));
      for (j = 0; j < nloc; j++) {
        for (i = 0; i < dim; i++) {
          switch (i) {
          case 0:
            array[dim * j + i] = grid_info->x[j];
            break;
          case 1:
            array[dim * j + i] = grid_info->y[j];
            break;
          case 2:
            array[dim * j + i] = grid_info->z[j];
            break;
          default:
            SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_SIZ, "PCML coordinate dimension must be <= 3");
          }
        }
      }

      /* passing coordinates to smoothers/coarse solver, should they need them */
      PetscCall(KSPGetPC(gridctx[level].ksp, &subpc));
      PetscCall(PCSetCoordinates(subpc, dim, nloc, array));
      PetscCall(VecRestoreArray(gridctx[level].coords, &array));
      level--;
    }
  }

  /* setupcalled is set to 0 so that MG is setup from scratch */
  pc->setupcalled = PETSC_FALSE;
  PetscCall(PCSetUp_MG(pc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   PCDestroy_ML - Destroys the private context for the ML preconditioner
   that was created with PCCreate_ML().

   Input Parameter:
.  pc - the preconditioner context

   Application Interface Routine: PCDestroy()
*/
static PetscErrorCode PCDestroy_ML(PC pc)
{
  PC_MG *mg    = (PC_MG *)pc->data;
  PC_ML *pc_ml = (PC_ML *)mg->innerctx;

  PetscFunctionBegin;
  PetscCall(PCReset_ML(pc));
  PetscCall(PetscFree(pc_ml));
  PetscCall(PCDestroy_MG(pc));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCSetCoordinates_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetFromOptions_ML(PC pc, PetscOptionItems PetscOptionsObject)
{
  PetscInt    indx, PrintLevel, partindx;
  const char *scheme[] = {"Uncoupled", "Coupled", "MIS", "METIS"};
  const char *part[]   = {"Zoltan", "ParMETIS"};
#if defined(HAVE_ML_ZOLTAN)
  const char *zscheme[] = {"RCB", "hypergraph", "fast_hypergraph"};
#endif
  PC_MG      *mg    = (PC_MG *)pc->data;
  PC_ML      *pc_ml = (PC_ML *)mg->innerctx;
  PetscMPIInt size;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)pc, &comm));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscOptionsHeadBegin(PetscOptionsObject, "ML options");

  PrintLevel = 0;
  indx       = 0;
  partindx   = 0;

  PetscCall(PetscOptionsInt("-pc_ml_PrintLevel", "Print level", "ML_Set_PrintLevel", PrintLevel, &PrintLevel, NULL));
  PetscStackCallExternalVoid("ML_Set_PrintLevel", ML_Set_PrintLevel(PrintLevel));
  PetscCall(PetscOptionsInt("-pc_ml_maxNlevels", "Maximum number of levels", "None", pc_ml->MaxNlevels, &pc_ml->MaxNlevels, NULL));
  PetscCall(PetscOptionsInt("-pc_ml_maxCoarseSize", "Maximum coarsest mesh size", "ML_Aggregate_Set_MaxCoarseSize", pc_ml->MaxCoarseSize, &pc_ml->MaxCoarseSize, NULL));
  PetscCall(PetscOptionsEList("-pc_ml_CoarsenScheme", "Aggregate Coarsen Scheme", "ML_Aggregate_Set_CoarsenScheme_*", scheme, 4, scheme[0], &indx, NULL));

  pc_ml->CoarsenScheme = indx;

  PetscCall(PetscOptionsReal("-pc_ml_DampingFactor", "P damping factor", "ML_Aggregate_Set_DampingFactor", pc_ml->DampingFactor, &pc_ml->DampingFactor, NULL));
  PetscCall(PetscOptionsReal("-pc_ml_Threshold", "Smoother drop tol", "ML_Aggregate_Set_Threshold", pc_ml->Threshold, &pc_ml->Threshold, NULL));
  PetscCall(PetscOptionsBool("-pc_ml_SpectralNormScheme_Anorm", "Method used for estimating spectral radius", "ML_Set_SpectralNormScheme_Anorm", pc_ml->SpectralNormScheme_Anorm, &pc_ml->SpectralNormScheme_Anorm, NULL));
  PetscCall(PetscOptionsBool("-pc_ml_Symmetrize", "Symmetrize aggregation", "ML_Set_Symmetrize", pc_ml->Symmetrize, &pc_ml->Symmetrize, NULL));
  PetscCall(PetscOptionsBool("-pc_ml_BlockScaling", "Scale all dofs at each node together", "None", pc_ml->BlockScaling, &pc_ml->BlockScaling, NULL));
  PetscCall(PetscOptionsEnum("-pc_ml_nullspace", "Which type of null space information to use", "None", PCMLNullSpaceTypes, (PetscEnum)pc_ml->nulltype, (PetscEnum *)&pc_ml->nulltype, NULL));
  PetscCall(PetscOptionsInt("-pc_ml_EnergyMinimization", "Energy minimization norm type (0=no minimization; see ML manual for 1,2,3; -1 and 4 undocumented)", "None", pc_ml->EnergyMinimization, &pc_ml->EnergyMinimization, NULL));
  PetscCall(PetscOptionsBool("-pc_ml_reuse_interpolation", "Reuse the interpolation operators when possible (cheaper, weaker when matrix entries change a lot)", "None", pc_ml->reuse_interpolation, &pc_ml->reuse_interpolation, NULL));
  /*
    The following checks a number of conditions.  If we let this stuff slip by, then ML's error handling will take over.
    This is suboptimal because it amounts to calling exit(1) so we check for the most common conditions.

    We also try to set some sane defaults when energy minimization is activated, otherwise it's hard to find a working
    combination of options and ML's exit(1) explanations don't help matters.
  */
  PetscCheck(pc_ml->EnergyMinimization >= -1 && pc_ml->EnergyMinimization <= 4, comm, PETSC_ERR_ARG_OUTOFRANGE, "EnergyMinimization must be in range -1..4");
  PetscCheck(pc_ml->EnergyMinimization != 4 || size == 1, comm, PETSC_ERR_SUP, "Energy minimization type 4 does not work in parallel");
  if (pc_ml->EnergyMinimization == 4) PetscCall(PetscInfo(pc, "Mandel's energy minimization scheme is experimental and broken in ML-6.2\n"));
  if (pc_ml->EnergyMinimization) PetscCall(PetscOptionsReal("-pc_ml_EnergyMinimizationDropTol", "Energy minimization drop tolerance", "None", pc_ml->EnergyMinimizationDropTol, &pc_ml->EnergyMinimizationDropTol, NULL));
  if (pc_ml->EnergyMinimization == 2) {
    /* According to ml_MultiLevelPreconditioner.cpp, this option is only meaningful for norm type (2) */
    PetscCall(PetscOptionsBool("-pc_ml_EnergyMinimizationCheap", "Use cheaper variant of norm type 2", "None", pc_ml->EnergyMinimizationCheap, &pc_ml->EnergyMinimizationCheap, NULL));
  }
  /* energy minimization sometimes breaks if this is turned off, the more classical stuff should be okay without it */
  if (pc_ml->EnergyMinimization) pc_ml->KeepAggInfo = PETSC_TRUE;
  PetscCall(PetscOptionsBool("-pc_ml_KeepAggInfo", "Allows the preconditioner to be reused, or auxiliary matrices to be generated", "None", pc_ml->KeepAggInfo, &pc_ml->KeepAggInfo, NULL));
  /* Option (-1) doesn't work at all (calls exit(1)) if the tentative restriction operator isn't stored. */
  if (pc_ml->EnergyMinimization == -1) pc_ml->Reusable = PETSC_TRUE;
  PetscCall(PetscOptionsBool("-pc_ml_Reusable", "Store intermedaiate data structures so that the multilevel hierarchy is reusable", "None", pc_ml->Reusable, &pc_ml->Reusable, NULL));
  /*
    ML's C API is severely underdocumented and lacks significant functionality.  The C++ API calls
    ML_Gen_MultiLevelHierarchy_UsingAggregation() which is a modified copy (!?) of the documented function
    ML_Gen_MGHierarchy_UsingAggregation().  This modification, however, does not provide a strict superset of the
    functionality in the old function, so some users may still want to use it.  Note that many options are ignored in
    this context, but ML doesn't provide a way to find out which ones.
   */
  PetscCall(PetscOptionsBool("-pc_ml_OldHierarchy", "Use old routine to generate hierarchy", "None", pc_ml->OldHierarchy, &pc_ml->OldHierarchy, NULL));
  PetscCall(PetscOptionsBool("-pc_ml_repartition", "Allow ML to repartition levels of the hierarchy", "ML_Repartition_Activate", pc_ml->Repartition, &pc_ml->Repartition, NULL));
  if (pc_ml->Repartition) {
    PetscCall(PetscOptionsReal("-pc_ml_repartitionMaxMinRatio", "Acceptable ratio of repartitioned sizes", "ML_Repartition_Set_LargestMinMaxRatio", pc_ml->MaxMinRatio, &pc_ml->MaxMinRatio, NULL));
    PetscCall(PetscOptionsInt("-pc_ml_repartitionMinPerProc", "Smallest repartitioned size", "ML_Repartition_Set_MinPerProc", pc_ml->MinPerProc, &pc_ml->MinPerProc, NULL));
    PetscCall(PetscOptionsInt("-pc_ml_repartitionPutOnSingleProc", "Problem size automatically repartitioned to one processor", "ML_Repartition_Set_PutOnSingleProc", pc_ml->PutOnSingleProc, &pc_ml->PutOnSingleProc, NULL));
#if defined(HAVE_ML_ZOLTAN)
    partindx = 0;
    PetscCall(PetscOptionsEList("-pc_ml_repartitionType", "Repartitioning library to use", "ML_Repartition_Set_Partitioner", part, 2, part[0], &partindx, NULL));

    pc_ml->RepartitionType = partindx;
    if (!partindx) {
      PetscInt zindx = 0;

      PetscCall(PetscOptionsEList("-pc_ml_repartitionZoltanScheme", "Repartitioning scheme to use", "None", zscheme, 3, zscheme[0], &zindx, NULL));

      pc_ml->ZoltanScheme = zindx;
    }
#else
    partindx = 1;
    PetscCall(PetscOptionsEList("-pc_ml_repartitionType", "Repartitioning library to use", "ML_Repartition_Set_Partitioner", part, 2, part[1], &partindx, NULL));
    pc_ml->RepartitionType = partindx;
    PetscCheck(partindx, PetscObjectComm((PetscObject)pc), PETSC_ERR_SUP_SYS, "ML not compiled with Zoltan");
#endif
    PetscCall(PetscOptionsBool("-pc_ml_Aux", "Aggregate using auxiliary coordinate-based laplacian", "None", pc_ml->Aux, &pc_ml->Aux, NULL));
    PetscCall(PetscOptionsReal("-pc_ml_AuxThreshold", "Auxiliary smoother drop tol", "None", pc_ml->AuxThreshold, &pc_ml->AuxThreshold, NULL));
  }
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   PCCreate_ML - Creates a ML preconditioner context, PC_ML,
   and sets this as the private data within the generic preconditioning
   context, PC, that was created within PCCreate().

   Input Parameter:
.  pc - the preconditioner context

   Application Interface Routine: PCCreate()
*/

/*MC
     PCML - Use the SNL ML algebraic multigrid preconditioner.

   Options Database Keys:
   Multigrid options(inherited):
+  -pc_mg_cycle_type <v> - v for V cycle, w for W-cycle (`PCMGSetCycleType()`)
.  -pc_mg_distinct_smoothup - Should one configure the up and down smoothers separately (`PCMGSetDistinctSmoothUp()`)
-  -pc_mg_type <multiplicative> - (one of) additive multiplicative full kascade

   ML Options Database Key:
+  -pc_ml_PrintLevel <0>                    - Print level (`ML_Set_PrintLevel()`)
.  -pc_ml_maxNlevels <10>                   - Maximum number of levels (None)
.  -pc_ml_maxCoarseSize <1>                 - Maximum coarsest mesh size (`ML_Aggregate_Set_MaxCoarseSize()`)
.  -pc_ml_CoarsenScheme <Uncoupled>         - (one of) Uncoupled Coupled MIS METIS
.  -pc_ml_DampingFactor <1.33333>           - P damping factor (`ML_Aggregate_Set_DampingFactor()`)
.  -pc_ml_Threshold <0>                     - Smoother drop tol (`ML_Aggregate_Set_Threshold()`)
.  -pc_ml_SpectralNormScheme_Anorm <false>  - Method used for estimating spectral radius (`ML_Set_SpectralNormScheme_Anorm()`)
.  -pc_ml_repartition <false>               - Allow ML to repartition levels of the hierarchy (`ML_Repartition_Activate()`)
.  -pc_ml_repartitionMaxMinRatio <1.3>      - Acceptable ratio of repartitioned sizes (`ML_Repartition_Set_LargestMinMaxRatio()`)
.  -pc_ml_repartitionMinPerProc <512>       - Smallest repartitioned size (`ML_Repartition_Set_MinPerProc()`)
.  -pc_ml_repartitionPutOnSingleProc <5000> - Problem size automatically repartitioned to one processor (`ML_Repartition_Set_PutOnSingleProc()`)
.  -pc_ml_repartitionType <Zoltan>          - Repartitioning library to use (`ML_Repartition_Set_Partitioner()`)
.  -pc_ml_repartitionZoltanScheme <RCB>     - Repartitioning scheme to use (None)
.  -pc_ml_Aux <false>                       - Aggregate using auxiliary coordinate-based Laplacian (None)
-  -pc_ml_AuxThreshold <0.0>                - Auxiliary smoother drop tol (None)

   Level: intermediate

   Developer Note:
   The coarser grid matrices and restriction/interpolation
   operators are computed by ML, with the matrices converted to PETSc matrices in `MATAIJ` format
   and the restriction/interpolation operators wrapped as PETSc shell matrices.

.seealso: [](ch_ksp), `PCCreate()`, `PCSetType()`, `PCType`, `PC`, `PCMGType`, `PCMG`, `PCHYPRE`, `PCGAMG`,
          `PCMGSetLevels()`, `PCMGGetLevels()`, `PCMGSetType()`, `MPSetCycles()`, `PCMGSetDistinctSmoothUp()`,
          `PCMGGetCoarseSolve()`, `PCMGSetResidual()`, `PCMGSetInterpolation()`,
          `PCMGSetRestriction()`, `PCMGGetSmoother()`, `PCMGGetSmootherUp()`, `PCMGGetSmootherDown()`,
          `PCMGSetCycleTypeOnLevel()`, `PCMGSetRhs()`, `PCMGSetX()`, `PCMGSetR()`
M*/

PETSC_EXTERN PetscErrorCode PCCreate_ML(PC pc)
{
  PC_ML *pc_ml;
  PC_MG *mg;

  PetscFunctionBegin;
  /* PCML is an inherited class of PCMG. Initialize pc as PCMG */
  PetscCall(PCSetType(pc, PCMG)); /* calls PCCreate_MG() and MGCreate_Private() */
  PetscCall(PetscObjectChangeTypeName((PetscObject)pc, PCML));
  /* Since PCMG tries to use DM associated with PC must delete it */
  PetscCall(DMDestroy(&pc->dm));
  PetscCall(PCMGSetGalerkin(pc, PC_MG_GALERKIN_EXTERNAL));
  mg = (PC_MG *)pc->data;

  /* create a supporting struct and attach it to pc */
  PetscCall(PetscNew(&pc_ml));
  mg->innerctx = pc_ml;

  pc_ml->ml_object                = 0;
  pc_ml->agg_object               = 0;
  pc_ml->gridctx                  = 0;
  pc_ml->PetscMLdata              = 0;
  pc_ml->Nlevels                  = -1;
  pc_ml->MaxNlevels               = 10;
  pc_ml->MaxCoarseSize            = 1;
  pc_ml->CoarsenScheme            = 1;
  pc_ml->Threshold                = 0.0;
  pc_ml->DampingFactor            = 4.0 / 3.0;
  pc_ml->SpectralNormScheme_Anorm = PETSC_FALSE;
  pc_ml->size                     = 0;
  pc_ml->dim                      = 0;
  pc_ml->nloc                     = 0;
  pc_ml->coords                   = 0;
  pc_ml->Repartition              = PETSC_FALSE;
  pc_ml->MaxMinRatio              = 1.3;
  pc_ml->MinPerProc               = 512;
  pc_ml->PutOnSingleProc          = 5000;
  pc_ml->RepartitionType          = 0;
  pc_ml->ZoltanScheme             = 0;
  pc_ml->Aux                      = PETSC_FALSE;
  pc_ml->AuxThreshold             = 0.0;

  /* allow for coordinates to be passed */
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCSetCoordinates_C", PCSetCoordinates_ML));

  /* overwrite the pointers of PCMG by the functions of PCML */
  pc->ops->setfromoptions = PCSetFromOptions_ML;
  pc->ops->setup          = PCSetUp_ML;
  pc->ops->reset          = PCReset_ML;
  pc->ops->destroy        = PCDestroy_ML;
  PetscFunctionReturn(PETSC_SUCCESS);
}
