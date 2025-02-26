/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include <petsc/private/dmdaimpl.h> /*I   "petscdmda.h"   I*/

PetscErrorCode DMGlobalToLocalBegin_DA(DM da, Vec g, InsertMode mode, Vec l)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da, DM_CLASSID, 1);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(l, VEC_CLASSID, 4);
  PetscCall(VecScatterBegin(dd->gtol, g, l, mode, SCATTER_FORWARD));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMGlobalToLocalEnd_DA(DM da, Vec g, InsertMode mode, Vec l)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da, DM_CLASSID, 1);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(l, VEC_CLASSID, 4);
  PetscCall(VecScatterEnd(dd->gtol, g, l, mode, SCATTER_FORWARD));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMLocalToGlobalBegin_DA(DM da, Vec l, InsertMode mode, Vec g)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da, DM_CLASSID, 1);
  PetscValidHeaderSpecific(l, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 4);
  if (mode == ADD_VALUES) {
    PetscCall(VecScatterBegin(dd->gtol, l, g, ADD_VALUES, SCATTER_REVERSE));
  } else if (mode == INSERT_VALUES) {
    PetscCheck(dd->bx == DM_BOUNDARY_GHOSTED || dd->bx == DM_BOUNDARY_NONE || dd->s <= 0 || dd->m != 1, PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "Available only for boundary none or with parallelism in x direction");
    PetscCheck(dd->bx == DM_BOUNDARY_GHOSTED || dd->by == DM_BOUNDARY_NONE || dd->s <= 0 || dd->n != 1, PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "Available only for boundary none or with parallelism in y direction");
    PetscCheck(dd->bx == DM_BOUNDARY_GHOSTED || dd->bz == DM_BOUNDARY_NONE || dd->s <= 0 || dd->p != 1, PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "Available only for boundary none or with parallelism in z direction");
    PetscCall(VecScatterBegin(dd->gtol, l, g, INSERT_VALUES, SCATTER_REVERSE_LOCAL));
  } else SETERRQ(PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "Not yet implemented");
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMLocalToGlobalEnd_DA(DM da, Vec l, InsertMode mode, Vec g)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da, DM_CLASSID, 1);
  PetscValidHeaderSpecific(l, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 4);
  if (mode == ADD_VALUES) {
    PetscCall(VecScatterEnd(dd->gtol, l, g, ADD_VALUES, SCATTER_REVERSE));
  } else if (mode == INSERT_VALUES) {
    PetscCall(VecScatterEnd(dd->gtol, l, g, INSERT_VALUES, SCATTER_REVERSE_LOCAL));
  } else SETERRQ(PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "Not yet implemented");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  DMDAGlobalToNatural_Create - Create the global to natural scatter object

  Collective

  Input Parameter:
. da - the distributed array context

  Level: developer

  Note:
  This is an internal routine called by `DMDAGlobalToNatural()` to
  create the scatter context.

.seealso: [](sec_struct), `DM`, `DMDA`, `DMDAGlobalToNaturalBegin()`, `DMDAGlobalToNaturalEnd()`, `DMLocalToGlobalBegin()`, `DMDACreate2d()`,
          `DMGlobalToLocalBegin()`, `DMGlobalToLocalEnd()`, `DMDACreateNaturalVector()`
*/
static PetscErrorCode DMDAGlobalToNatural_Create(DM da)
{
  PetscInt m, start, Nlocal;
  IS       from, to;
  Vec      global;
  DM_DA   *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(da, DM_CLASSID, 1);
  PetscCheck(dd->natural, PetscObjectComm((PetscObject)da), PETSC_ERR_ORDER, "Natural layout vector not yet created; cannot scatter into it");

  /* create the scatter context */
  PetscCall(VecGetLocalSize(dd->natural, &m));
  PetscCall(VecGetOwnershipRange(dd->natural, &start, NULL));

  PetscCall(DMDAGetNatural_Private(da, &Nlocal, &to));
  PetscCheck(Nlocal == m, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Internal error: Nlocal %" PetscInt_FMT " local vector size %" PetscInt_FMT, Nlocal, m);
  PetscCall(ISCreateStride(PetscObjectComm((PetscObject)da), m, start, 1, &from));
  PetscCall(VecCreateMPIWithArray(PetscObjectComm((PetscObject)da), dd->w, dd->Nlocal, PETSC_DETERMINE, NULL, &global));
  PetscCall(VecScatterCreate(global, from, dd->natural, to, &dd->gton));
  PetscCall(VecDestroy(&global));
  PetscCall(ISDestroy(&from));
  PetscCall(ISDestroy(&to));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMDAGlobalToNaturalBegin - Maps values from the global vector to a global vector
  in the "natural" grid ordering. Must be followed by
  `DMDAGlobalToNaturalEnd()` to complete the exchange.

  Neighbor-wise Collective

  Input Parameters:
+ da   - the distributed array context
. g    - the global vector, see `DMCreateGlobalVector()`
- mode - one of `INSERT_VALUES` or `ADD_VALUES`

  Output Parameter:
. n - the natural ordering values, see `DMDACreateNaturalVector()`

  Level: advanced

  Notes:
  The global and natural vectors used here need not be the same as those
  obtained from `DMCreateGlobalVector()` and `DMDACreateNaturalVector()`, BUT they
  must have the same parallel data layout; they could, for example, be
  obtained with `VecDuplicate()` from the `DMDA` originating vectors.

  You must call `DMDACreateNaturalVector()` before using this routine

.seealso: [](sec_struct), `DM`, `DMDA`, `DMDAGlobalToNaturalEnd()`, `DMLocalToGlobalBegin()`, `DMDACreate2d()`,
          `DMGlobalToLocalBegin()`, `DMGlobalToLocalEnd()`, `DMDACreateNaturalVector()`
@*/
PetscErrorCode DMDAGlobalToNaturalBegin(DM da, Vec g, InsertMode mode, Vec n)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(n, VEC_CLASSID, 4);
  if (!dd->gton) {
    /* create the scatter context */
    PetscCall(DMDAGlobalToNatural_Create(da));
  }
  PetscCall(VecScatterBegin(dd->gton, g, n, mode, SCATTER_FORWARD));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMDAGlobalToNaturalEnd - Maps values from the global vector to a global vector
  in the natural ordering. Must be preceded by `DMDAGlobalToNaturalBegin()`.

  Neighbor-wise Collective

  Input Parameters:
+ da   - the distributed array context
. g    - the global vector, see `DMCreateGlobalVector()`
- mode - one of `INSERT_VALUES` or `ADD_VALUES`

  Output Parameter:
. n - the global values in the natural ordering, see `DMDACreateNaturalVector()`

  Level: advanced

  Notes:
  The global and local vectors used here need not be the same as those
  obtained from `DMCreateGlobalVector()` and `DMDACreateNaturalVector()`, BUT they
  must have the same parallel data layout; they could, for example, be
  obtained with VecDuplicate() from the `DMDA` originating vectors.

.seealso: [](sec_struct), `DM`, `DMDA`, `DMDAGlobalToNaturalBegin()`, `DMLocalToGlobalBegin()`, `DMDACreate2d()`,
          `DMGlobalToLocalBegin()`, `DMGlobalToLocalEnd()`, `DMDACreateNaturalVector()`
@*/
PetscErrorCode DMDAGlobalToNaturalEnd(DM da, Vec g, InsertMode mode, Vec n)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(n, VEC_CLASSID, 4);
  PetscCall(VecScatterEnd(dd->gton, g, n, mode, SCATTER_FORWARD));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMDANaturalToGlobalBegin - Maps values from a global vector in the "natural" ordering
  to a global vector in the PETSc `DMDA` grid ordering. Must be followed by
  `DMDANaturalToGlobalEnd()` to complete the exchange.

  Neighbor-wise Collective

  Input Parameters:
+ da   - the distributed array context
. g    - the global vector in a natural ordering, see `DMDACreateNaturalVector()`
- mode - one of `INSERT_VALUES` or `ADD_VALUES`

  Output Parameter:
. n - the values in the `DMDA` ordering

  Level: advanced

  Notes:
  The global and natural vectors used here need not be the same as those
  obtained from `DMCreateGlobalVector()` and `DMDACreateNaturalVector()`, BUT they
  must have the same parallel data layout; they could, for example, be
  obtained with `VecDuplicate()` from the `DMDA` originating vectors.

.seealso: [](sec_struct), `DM`, `DMDA`, `DMDAGlobalToNaturalEnd()`, `DMDAGlobalToNaturalBegin()`, `DMLocalToGlobalBegin()`, `DMDACreate2d()`,
          `DMGlobalToLocalBegin()`, `DMGlobalToLocalEnd()`, `DMDACreateNaturalVector()`
@*/
PetscErrorCode DMDANaturalToGlobalBegin(DM da, Vec n, InsertMode mode, Vec g)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidHeaderSpecific(n, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 4);
  if (!dd->gton) {
    /* create the scatter context */
    PetscCall(DMDAGlobalToNatural_Create(da));
  }
  PetscCall(VecScatterBegin(dd->gton, n, g, mode, SCATTER_REVERSE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMDANaturalToGlobalEnd - Maps values from the natural ordering global vector
  to a global vector in the PETSc `DMDA` ordering. Must be preceded by `DMDANaturalToGlobalBegin()`.

  Neighbor-wise Collective

  Input Parameters:
+ da   - the distributed array context
. g    - the global vector in a natural ordering
- mode - one of `INSERT_VALUES` or `ADD_VALUES`

  Output Parameter:
. n - the global values in the PETSc `DMDA` ordering

  Level: advanced

  Notes:
  The global and local vectors used here need not be the same as those
  obtained from `DMCreateGlobalVector()` and `DMDACreateNaturalVector()`, BUT they
  must have the same parallel data layout; they could, for example, be
  obtained with `VecDuplicate()` from the `DMDA` originating vectors.

.seealso: [](sec_struct), `DM`, `DMDA`, `DMDAGlobalToNaturalBegin()`, `DMDAGlobalToNaturalEnd()`, `DMLocalToGlobalBegin()`, `DMDACreate2d()`,
          `DMGlobalToLocalBegin()`, `DMGlobalToLocalEnd()`, `DMDACreateNaturalVector()`
@*/
PetscErrorCode DMDANaturalToGlobalEnd(DM da, Vec n, InsertMode mode, Vec g)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidHeaderSpecific(n, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(g, VEC_CLASSID, 4);
  PetscCall(VecScatterEnd(dd->gton, n, g, mode, SCATTER_REVERSE));
  PetscFunctionReturn(PETSC_SUCCESS);
}
