#include <petsc/private/viewerimpl.h> /*I   "petscsys.h"   I*/
#include <adios.h>
#include <adios_read.h>

#include <petsc/private/vieweradiosimpl.h>

static PetscErrorCode PetscViewerSetFromOptions_ADIOS(PetscViewer v, PetscOptionItems PetscOptionsObject)
{
  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "ADIOS PetscViewer Options");
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileClose_ADIOS(PetscViewer viewer)
{
  PetscViewer_ADIOS *adios = (PetscViewer_ADIOS *)viewer->data;

  PetscFunctionBegin;
  switch (adios->btype) {
  case FILE_MODE_READ:
    PetscCallExternal(adios_read_close, adios->adios_fp);
    break;
  case FILE_MODE_WRITE:
    PetscCallExternal(adios_close, adios->adios_handle);
    break;
  default:
    break;
  }
  PetscCall(PetscFree(adios->filename));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerDestroy_ADIOS(PetscViewer viewer)
{
  PetscViewer_ADIOS *adios = (PetscViewer_ADIOS *)viewer->data;

  PetscFunctionBegin;
  PetscCall(PetscViewerFileClose_ADIOS(viewer));
  PetscCall(PetscFree(adios));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileSetName_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileGetName_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileSetMode_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileSetMode_ADIOS(PetscViewer viewer, PetscFileMode type)
{
  PetscViewer_ADIOS *adios = (PetscViewer_ADIOS *)viewer->data;

  PetscFunctionBegin;
  adios->btype = type;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileSetName_ADIOS(PetscViewer viewer, const char name[])
{
  PetscViewer_ADIOS *adios = (PetscViewer_ADIOS *)viewer->data;

  PetscFunctionBegin;
  if (adios->filename) PetscCall(PetscFree(adios->filename));
  PetscCall(PetscStrallocpy(name, &adios->filename));
  /* Create or open the file collectively */
  switch (adios->btype) {
  case FILE_MODE_READ:
    adios->adios_fp = adios_read_open_file(adios->filename, ADIOS_READ_METHOD_BP, PetscObjectComm((PetscObject)viewer));
    break;
  case FILE_MODE_WRITE:
    adios_open(&adios->adios_handle, "PETSc", adios->filename, "w", PetscObjectComm((PetscObject)viewer));
    break;
  case FILE_MODE_UNDEFINED:
    SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_ORDER, "Must call PetscViewerFileSetMode() before PetscViewerFileSetName()");
  default:
    SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_SUP, "Unsupported file mode %s", PetscFileModes[adios->btype]);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileGetName_ADIOS(PetscViewer viewer, const char **name)
{
  PetscViewer_ADIOS *vadios = (PetscViewer_ADIOS *)viewer->data;

  PetscFunctionBegin;
  *name = vadios->filename;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PETSCVIEWERADIOS - A viewer that writes to an ADIOS file

  Level: beginner

.seealso: `PetscViewerADIOSOpen()`, `PetscViewerStringSPrintf()`, `PetscViewerSocketOpen()`, `PetscViewerDrawOpen()`, `PETSCVIEWERSOCKET`,
          `PetscViewerCreate()`, `PetscViewerASCIIOpen()`, `PetscViewerBinaryOpen()`, `PETSCVIEWERBINARY`, `PETSCVIEWERDRAW`, `PETSCVIEWERSTRING`,
          `PetscViewerMatlabOpen()`, `VecView()`, `DMView()`, `PetscViewerMatlabPutArray()`, `PETSCVIEWERASCII`, `PETSCVIEWERMATLAB`,
          `PetscViewerFileSetName()`, `PetscViewerFileSetMode()`, `PetscViewerFormat`, `PetscViewerType`, `PetscViewerSetType()`
M*/

PETSC_EXTERN PetscErrorCode PetscViewerCreate_ADIOS(PetscViewer v)
{
  PetscViewer_ADIOS *adios;

  PetscFunctionBegin;
  PetscCall(PetscNew(&adios));

  v->data                = (void *)adios;
  v->ops->destroy        = PetscViewerDestroy_ADIOS;
  v->ops->setfromoptions = PetscViewerSetFromOptions_ADIOS;
  v->ops->flush          = NULL;
  adios->btype           = FILE_MODE_UNDEFINED;
  adios->filename        = NULL;
  adios->timestep        = -1;

  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileSetName_C", PetscViewerFileSetName_ADIOS));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileGetName_C", PetscViewerFileGetName_ADIOS));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileSetMode_C", PetscViewerFileSetMode_ADIOS));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerADIOSOpen - Opens a file for ADIOS input/output.

  Collective

  Input Parameters:
+ comm - MPI communicator
. name - name of file
- type - type of file
.vb
    FILE_MODE_WRITE - create new file for binary output
    FILE_MODE_READ - open existing file for binary input
    FILE_MODE_APPEND - open existing file for binary output
.ve

  Output Parameter:
. adiosv - `PetscViewer` for ADIOS input/output to use with the specified file

  Level: beginner

  Note:
  This `PetscViewer` should be destroyed with `PetscViewerDestroy()`.

.seealso: `PetscViewerASCIIOpen()`, `PetscViewerPushFormat()`, `PetscViewerDestroy()`, `PetscViewerHDF5Open()`,
          `VecView()`, `MatView()`, `VecLoad()`, `PetscViewerSetType()`, `PetscViewerFileSetMode()`, `PetscViewerFileSetName()`
          `MatLoad()`, `PetscFileMode`, `PetscViewer`
@*/
PetscErrorCode PetscViewerADIOSOpen(MPI_Comm comm, const char name[], PetscFileMode type, PetscViewer *adiosv)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerCreate(comm, adiosv));
  PetscCall(PetscViewerSetType(*adiosv, PETSCVIEWERADIOS));
  PetscCall(PetscViewerFileSetMode(*adiosv, type));
  PetscCall(PetscViewerFileSetName(*adiosv, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}
