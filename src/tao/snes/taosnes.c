#include <petsc/private/taoimpl.h> /*I "petsctao.h" I*/

typedef struct {
  SNES      snes;
  PetscBool setfromoptionscalled;
} Tao_SNES;

static PetscErrorCode TaoSolve_SNES(Tao tao)
{
  Tao_SNES *taosnes = (Tao_SNES *)tao->data;
  PetscInt  its;

  PetscFunctionBegin;
  /* TODO SNES fails if KSP reaches max_it, while TAO accepts whatever we got */
  PetscCall(SNESSolve(taosnes->snes, NULL, tao->solution));
  /* TODO REASONS */
  tao->reason = TAO_CONVERGED_USER;
  PetscCall(SNESGetIterationNumber(taosnes->snes, &its));
  PetscCall(TaoSetIterationNumber(tao, its));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TaoDestroy_SNES(Tao tao)
{
  Tao_SNES *taosnes = (Tao_SNES *)tao->data;

  PetscFunctionBegin;
  PetscCall(SNESDestroy(&taosnes->snes));
  PetscCall(PetscFree(tao->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TAOSNESObj(SNES snes, Vec X, PetscReal *f, void *ctx)
{
  Tao tao = (Tao)ctx;

  PetscFunctionBegin;
  PetscCall(TaoComputeObjective(tao, X, f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TAOSNESFunc(SNES snes, Vec X, Vec F, void *ctx)
{
  Tao tao = (Tao)ctx;

  PetscFunctionBegin;
  PetscCall(TaoComputeGradient(tao, X, F));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TAOSNESJac(SNES snes, Vec X, Mat A, Mat P, void *ctx)
{
  Tao tao = (Tao)ctx;

  PetscFunctionBegin;
  PetscCall(TaoComputeHessian(tao, X, A, P));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TAOSNESMonitor(SNES snes, PetscInt its, PetscReal fnorm, void *ctx)
{
  Tao       tao = (Tao)ctx;
  PetscReal obj;
  Vec       X;

  PetscFunctionBegin;
  PetscCall(SNESGetSolution(snes, &X));
  PetscCall(TaoComputeObjective(tao, X, &obj));
  PetscCall(TaoSetIterationNumber(tao, its));
  PetscCall(TaoMonitor(tao, its, obj, fnorm, 0.0, 0.0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TaoSetUp_SNES(Tao tao)
{
  Tao_SNES   *taosnes = (Tao_SNES *)tao->data;
  Mat         A, P;
  const char *prefix;

  PetscFunctionBegin;
  PetscCall(TaoGetOptionsPrefix(tao, &prefix));
  PetscCall(SNESSetOptionsPrefix(taosnes->snes, prefix));
  PetscCall(SNESSetSolution(taosnes->snes, tao->solution));
  PetscCall(SNESSetObjective(taosnes->snes, TAOSNESObj, tao));
  PetscCall(SNESSetFunction(taosnes->snes, NULL, TAOSNESFunc, tao));
  PetscCall(SNESMonitorSet(taosnes->snes, TAOSNESMonitor, tao, NULL));
  PetscCall(TaoGetHessian(tao, &A, &P, NULL, NULL));
  if (A) PetscCall(SNESSetJacobian(taosnes->snes, A, P, TAOSNESJac, tao));
  if (taosnes->setfromoptionscalled) PetscCall(SNESSetFromOptions(taosnes->snes));
  taosnes->setfromoptionscalled = PETSC_FALSE;
  PetscCall(SNESSetUp(taosnes->snes));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TaoSetFromOptions_SNES(Tao tao, PetscOptionItems PetscOptionsObject)
{
  Tao_SNES *taosnes = (Tao_SNES *)tao->data;

  PetscFunctionBegin;
  taosnes->setfromoptionscalled = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TaoView_SNES(Tao tao, PetscViewer viewer)
{
  Tao_SNES *taosnes = (Tao_SNES *)tao->data;

  PetscFunctionBegin;
  PetscCall(SNESView(taosnes->snes, viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  TAOSNES - nonlinear solver using SNES

   Level: advanced

.seealso: `TaoCreate()`, `Tao`, `TaoSetType()`, `TaoType`
M*/
PETSC_EXTERN PetscErrorCode TaoCreate_SNES(Tao tao)
{
  Tao_SNES *taosnes;

  PetscFunctionBegin;
  tao->ops->destroy        = TaoDestroy_SNES;
  tao->ops->setup          = TaoSetUp_SNES;
  tao->ops->setfromoptions = TaoSetFromOptions_SNES;
  tao->ops->view           = TaoView_SNES;
  tao->ops->solve          = TaoSolve_SNES;

  PetscCall(TaoParametersInitialize(tao));

  PetscCall(PetscNew(&taosnes));
  tao->data = (void *)taosnes;
  PetscCall(SNESCreate(PetscObjectComm((PetscObject)tao), &taosnes->snes));
  PetscCall(PetscObjectIncrementTabLevel((PetscObject)taosnes->snes, (PetscObject)tao, 1));
  PetscFunctionReturn(PETSC_SUCCESS);
}
