/*
     PetscViewers are objects where other objects can be looked at or stored.
*/
#pragma once

#include <petscsys.h>
#include <petscviewertypes.h>
#include <petscdrawtypes.h>

/* MANSEC = Sys */
/* SUBMANSEC = Viewer */

PETSC_EXTERN PetscClassId PETSC_VIEWER_CLASSID;

/*J
   PetscViewerType - String with the name of a PETSc `PetscViewer` implementation

   Level: beginner

.seealso: [](sec_viewers), `PetscViewerSetType()`, `PetscViewer`, `PetscViewerRegister()`, `PetscViewerCreate()`
J*/
typedef const char *PetscViewerType;
#define PETSCVIEWERSOCKET      "socket"
#define PETSCVIEWERASCII       "ascii"
#define PETSCVIEWERBINARY      "binary"
#define PETSCVIEWERSTRING      "string"
#define PETSCVIEWERDRAW        "draw"
#define PETSCVIEWERVU          "vu"
#define PETSCVIEWERMATHEMATICA "mathematica"
#define PETSCVIEWERHDF5        "hdf5"
#define PETSCVIEWERVTK         "vtk"
#define PETSCVIEWERMATLAB      "matlab"
#define PETSCVIEWERSAWS        "saws"
#define PETSCVIEWERGLVIS       "glvis"
#define PETSCVIEWERADIOS       "adios"
#define PETSCVIEWEREXODUSII    "exodusii"
#define PETSCVIEWERCGNS        "cgns"
#define PETSCVIEWERPYTHON      "python"
#define PETSCVIEWERPYVISTA     "pyvista"

PETSC_EXTERN PetscFunctionList PetscViewerList;
PETSC_EXTERN PetscErrorCode    PetscViewerInitializePackage(void);
PETSC_EXTERN PetscErrorCode    PetscViewerFinalizePackage(void);

PETSC_EXTERN PetscErrorCode PetscViewerRegister(const char[], PetscErrorCode (*)(PetscViewer));

PETSC_EXTERN PetscErrorCode PetscViewerCreate(MPI_Comm, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerSetFromOptions(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIOpenWithFILE(MPI_Comm, FILE *, PetscViewer *);

PETSC_EXTERN PetscErrorCode PetscViewerASCIIOpen(MPI_Comm, const char[], PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerASCIISetFILE(PetscViewer, FILE *);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryOpen(MPI_Comm, const char[], PetscFileMode, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerADIOSOpen(MPI_Comm, const char[], PetscFileMode, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetFlowControl(PetscViewer, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscViewerBinarySetFlowControl(PetscViewer, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerBinarySetUseMPIIO(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetUseMPIIO(PetscViewer, PetscBool *);
#if defined(PETSC_HAVE_MPIIO)
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetMPIIODescriptor(PetscViewer, MPI_File *);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetMPIIOOffset(PetscViewer, MPI_Offset *);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryAddMPIIOOffset(PetscViewer, MPI_Offset);
#endif

PETSC_EXTERN PetscErrorCode PetscViewerSocketOpen(MPI_Comm, const char[], int, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerStringOpen(MPI_Comm, char[], size_t, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerDrawOpen(MPI_Comm, const char[], const char[], int, int, int, int, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerDrawSetDrawType(PetscViewer, PetscDrawType);

PETSC_EXTERN PetscErrorCode PetscViewerMathematicaOpen(MPI_Comm, int, const char[], const char[], PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerMatlabOpen(MPI_Comm, const char[], PetscFileMode, PetscViewer *);

/*E
    PetscViewerGLVisType - indicates what type of `PETSCVIEWERGLVIS` viewer to use

    Values:
+   `PETSC_VIEWER_GLVIS_DUMP`   - save the data to a file
-   `PETSC_VIEWER_GLVIS_SOCKET` - communicate the data to another program via a socket

    Level: beginner

.seealso: [](sec_viewers), `PETSCVIEWERGLVIS`, `PetscViewerGLVisOpen()`
E*/
typedef enum {
  PETSC_VIEWER_GLVIS_DUMP,
  PETSC_VIEWER_GLVIS_SOCKET
} PetscViewerGLVisType;
PETSC_EXTERN PetscErrorCode PetscViewerGLVisOpen(MPI_Comm, PetscViewerGLVisType, const char *, PetscInt, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerGLVisSetPrecision(PetscViewer, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerGLVisSetSnapId(PetscViewer, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerGLVisSetFields(PetscViewer, PetscInt, const char *[], PetscInt[], PetscErrorCode (*)(PetscObject, PetscInt, PetscObject[], void *), PetscObject[], void *, PetscErrorCode (*)(void *));

PETSC_EXTERN PetscErrorCode PetscViewerGetType(PetscViewer, PetscViewerType *);
PETSC_EXTERN PetscErrorCode PetscViewerSetType(PetscViewer, PetscViewerType);
PETSC_EXTERN PetscErrorCode PetscViewerDestroy(PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerGetSubViewer(PetscViewer, MPI_Comm, PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerRestoreSubViewer(PetscViewer, MPI_Comm, PetscViewer *);

PETSC_EXTERN PetscErrorCode PetscViewerSetUp(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerView(PetscViewer, PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerViewFromOptions(PetscViewer, PetscObject, const char[]);

PETSC_EXTERN PetscErrorCode PetscViewerSetOptionsPrefix(PetscViewer, const char[]);
PETSC_EXTERN PetscErrorCode PetscViewerAppendOptionsPrefix(PetscViewer, const char[]);
PETSC_EXTERN PetscErrorCode PetscViewerGetOptionsPrefix(PetscViewer, const char *[]);

PETSC_EXTERN PetscErrorCode PetscViewerReadable(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerWritable(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerCheckReadable(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerCheckWritable(PetscViewer);

/*E
   PetscViewerFormat - Way a viewer presents the object

   Values:
+    `PETSC_VIEWER_DEFAULT`           - default format for the specific object being viewed
.    `PETSC_VIEWER_ASCII_MATLAB`      - MATLAB format
.    `PETSC_VIEWER_ASCII_DENSE`       - print matrix as a dense two dimensiona array
.    `PETSC_VIEWER_ASCII_IMPL`        - implementation-specific format (which is in many cases the same as the default)
.    `PETSC_VIEWER_ASCII_INFO`        - basic information about object
.    `PETSC_VIEWER_ASCII_INFO_DETAIL` - more detailed info about object (but still not vector or matrix entries)
.    `PETSC_VIEWER_ASCII_COMMON`      - identical output format for all objects of a particular type
.    `PETSC_VIEWER_ASCII_INDEX`       - (for vectors) prints the vector  element number next to each vector entry
.    `PETSC_VIEWER_ASCII_SYMMODU`     - print parallel vectors without indicating the MPI process ranges that own the entries
.    `PETSC_VIEWER_ASCII_VTK`         - outputs the object to a VTK file (deprecated since v3.14)
.    `PETSC_VIEWER_NATIVE`            - store the object to the binary file in its native format (for example, dense
                                        matrices are stored as dense), `DMDA` vectors are dumped directly to the
                                        file instead of being first put in the natural ordering
.    `PETSC_VIEWER_ASCII_LATEX`       - output the data in LaTeX
.    `PETSC_VIEWER_BINARY_MATLAB`     - output additional information that can be used to read the data into MATLAB
.    `PETSC_VIEWER_DRAW_BASIC`        - views the vector with a simple 1d plot
.    `PETSC_VIEWER_DRAW_LG`           - views the vector with a line graph
-    `PETSC_VIEWER_DRAW_CONTOUR`      - views the vector with a contour plot

   Level: beginner

   Note:
   A variety of specialized formats also exist

.seealso: [](sec_viewers), `PetscViewer`, `PetscViewerType`, `PetscViewerPushFormat()`, `PetscViewerPopFormat()`
E*/
typedef enum {
  PETSC_VIEWER_DEFAULT,
  PETSC_VIEWER_ASCII_MATLAB,
  PETSC_VIEWER_ASCII_MATHEMATICA,
  PETSC_VIEWER_ASCII_IMPL,
  PETSC_VIEWER_ASCII_INFO,
  PETSC_VIEWER_ASCII_INFO_DETAIL,
  PETSC_VIEWER_ASCII_COMMON,
  PETSC_VIEWER_ASCII_SYMMODU,
  PETSC_VIEWER_ASCII_INDEX,
  PETSC_VIEWER_ASCII_DENSE,
  PETSC_VIEWER_ASCII_MATRIXMARKET,
  PETSC_VIEWER_ASCII_PCICE,
  PETSC_VIEWER_ASCII_PYTHON,
  PETSC_VIEWER_ASCII_FACTOR_INFO,
  PETSC_VIEWER_ASCII_LATEX,
  PETSC_VIEWER_ASCII_XML,
  PETSC_VIEWER_ASCII_FLAMEGRAPH,
  PETSC_VIEWER_ASCII_GLVIS,
  PETSC_VIEWER_ASCII_CSV,
  PETSC_VIEWER_DRAW_BASIC,
  PETSC_VIEWER_DRAW_LG,
  PETSC_VIEWER_DRAW_LG_XRANGE,
  PETSC_VIEWER_DRAW_CONTOUR,
  PETSC_VIEWER_DRAW_PORTS,
  PETSC_VIEWER_VTK_VTS,
  PETSC_VIEWER_VTK_VTR,
  PETSC_VIEWER_VTK_VTU,
  PETSC_VIEWER_BINARY_MATLAB,
  PETSC_VIEWER_NATIVE,
  PETSC_VIEWER_HDF5_PETSC,
  PETSC_VIEWER_HDF5_VIZ,
  PETSC_VIEWER_HDF5_XDMF,
  PETSC_VIEWER_HDF5_MAT,
  PETSC_VIEWER_NOFORMAT,
  PETSC_VIEWER_LOAD_BALANCE,
  PETSC_VIEWER_FAILED,
  PETSC_VIEWER_ALL
} PetscViewerFormat;
PETSC_EXTERN const char *const PetscViewerFormats[];

PETSC_EXTERN PETSC_DEPRECATED_FUNCTION(3, 7, 0, "PetscViewerPushFormat() / PetscViewerPopFormat()", ) PetscErrorCode PetscViewerSetFormat(PetscViewer, PetscViewerFormat);
PETSC_EXTERN PetscErrorCode PetscViewerPushFormat(PetscViewer, PetscViewerFormat);
PETSC_EXTERN PetscErrorCode PetscViewerPopFormat(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerGetFormat(PetscViewer, PetscViewerFormat *);
PETSC_EXTERN PetscErrorCode PetscViewerFlush(PetscViewer);

PETSC_EXTERN PetscErrorCode PetscOptionsPushCreateViewerOff(PetscBool);
PETSC_EXTERN PetscErrorCode PetscOptionsPopCreateViewerOff(void);
PETSC_EXTERN PetscErrorCode PetscOptionsGetCreateViewerOff(PetscBool *);
PETSC_EXTERN PetscErrorCode PetscOptionsCreateViewer(MPI_Comm, PetscOptions, const char[], const char[], PetscViewer *, PetscViewerFormat *, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscOptionsCreateViewers(MPI_Comm, PetscOptions, const char[], const char[], PetscInt *, PetscViewer *, PetscViewerFormat *, PetscBool *);
#define PetscOptionsViewer(a, b, c, d, e, f) PetscOptionsViewer_Private(PetscOptionsObject, a, b, c, d, e, f)
PETSC_EXTERN PetscErrorCode PetscOptionsViewer_Private(PetscOptionItems, const char[], const char[], const char[], PetscViewer *, PetscViewerFormat *, PetscBool *);

PETSC_DEPRECATED_FUNCTION(3, 22, 0, "PetscViewerDestroy()", ) static inline PetscErrorCode PetscOptionsRestoreViewer(PetscViewer *viewer)
{
  return PetscViewerDestroy(viewer);
}
PETSC_DEPRECATED_FUNCTION(3, 22, 0, "PetscOptionsCreateViewer()", ) static inline PetscErrorCode PetscOptionsGetViewer(MPI_Comm comm, PetscOptions op, const char a[], const char b[], PetscViewer *v, PetscViewerFormat *f, PetscBool *fg)
{
  return PetscOptionsCreateViewer(comm, op, a, b, v, f, fg);
}
PETSC_DEPRECATED_FUNCTION(3, 22, 0, "PetscOptionsCreateViewers()", ) static inline PetscErrorCode PetscOptionsGetViewers(MPI_Comm comm, PetscOptions op, const char a[], const char b[], PetscInt *n, PetscViewer *v, PetscViewerFormat *f, PetscBool *fg)
{
  return PetscOptionsCreateViewers(comm, op, a, b, n, v, f, fg);
}
PETSC_DEPRECATED_FUNCTION(3, 22, 0, "PetscOptionsGetCreateViewerOff()", ) static inline PetscErrorCode PetscOptionsGetViewerOff(PetscBool *fg)
{
  return PetscOptionsGetCreateViewerOff(fg);
}
PETSC_DEPRECATED_FUNCTION(3, 22, 0, "PetscOptionsPushCreateViewerOff()", ) static inline PetscErrorCode PetscOptionsPushGetViewerOff(PetscBool fg)
{
  return PetscOptionsPushCreateViewerOff(fg);
}
PETSC_DEPRECATED_FUNCTION(3, 22, 0, "PetscOptionsPushCreateViewerOff()", ) static inline PetscErrorCode PetscOptionsPopGetViewerOff(void)
{
  return PetscOptionsPopCreateViewerOff();
}

typedef struct {
  PetscViewer        viewer;
  PetscViewerFormat  format;
  PetscInt           view_interval;
  void              *data;
  PetscCtxDestroyFn *data_destroy;
} PetscViewerAndFormat;
PETSC_EXTERN PetscErrorCode PetscViewerAndFormatCreate(PetscViewer, PetscViewerFormat, PetscViewerAndFormat **);
PETSC_EXTERN PetscErrorCode PetscViewerAndFormatDestroy(PetscViewerAndFormat **);

/*
   Operations explicit to a particular class of viewers
*/

PETSC_EXTERN PetscErrorCode PetscViewerASCIIGetPointer(PetscViewer, FILE **);
PETSC_EXTERN PetscErrorCode PetscViewerFileGetMode(PetscViewer, PetscFileMode *);
PETSC_EXTERN PetscErrorCode PetscViewerFileSetMode(PetscViewer, PetscFileMode);
PETSC_EXTERN PetscErrorCode PetscViewerRead(PetscViewer, void *, PetscInt, PetscInt *, PetscDataType);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIPrintf(PetscViewer, const char[], ...) PETSC_ATTRIBUTE_FORMAT(2, 3);
PETSC_EXTERN PetscErrorCode PetscViewerASCIISynchronizedPrintf(PetscViewer, const char[], ...) PETSC_ATTRIBUTE_FORMAT(2, 3);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIPushSynchronized(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIPopSynchronized(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIPushTab(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIPopTab(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIUseTabs(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerASCIISetTab(PetscViewer, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIGetTab(PetscViewer, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIAddTab(PetscViewer, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerASCIISubtractTab(PetscViewer, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIRead(PetscViewer, void *, PetscInt, PetscInt *, PetscDataType);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetDescriptor(PetscViewer, int *);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetInfoPointer(PetscViewer, FILE **);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryRead(PetscViewer, void *, PetscInt, PetscInt *, PetscDataType);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryWrite(PetscViewer, const void *, PetscInt, PetscDataType);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryReadAll(PetscViewer, void *, PetscCount, PetscCount, PetscCount, PetscDataType);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryWriteAll(PetscViewer, const void *, PetscCount, PetscCount, PetscCount, PetscDataType);
PETSC_EXTERN PetscErrorCode PetscViewerStringSPrintf(PetscViewer, const char[], ...) PETSC_ATTRIBUTE_FORMAT(2, 3);
PETSC_EXTERN PetscErrorCode PetscViewerStringSetString(PetscViewer, char[], size_t);
PETSC_EXTERN PetscErrorCode PetscViewerStringGetStringRead(PetscViewer, const char *[], size_t *);
PETSC_EXTERN PetscErrorCode PetscViewerStringSetOwnString(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerDrawClear(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerDrawSetHold(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerDrawGetHold(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerDrawSetPause(PetscViewer, PetscReal);
PETSC_EXTERN PetscErrorCode PetscViewerDrawGetPause(PetscViewer, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscViewerDrawSetInfo(PetscViewer, const char[], const char[], int, int, int, int);
PETSC_EXTERN PetscErrorCode PetscViewerDrawResize(PetscViewer, int, int);
PETSC_EXTERN PetscErrorCode PetscViewerDrawSetBounds(PetscViewer, PetscInt, const PetscReal *);
PETSC_EXTERN PetscErrorCode PetscViewerDrawGetBounds(PetscViewer, PetscInt *, const PetscReal **);
PETSC_EXTERN PetscErrorCode PetscViewerSocketSetConnection(PetscViewer, const char[], int);
PETSC_EXTERN PetscErrorCode PetscViewerBinarySkipInfo(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerBinarySetSkipInfo(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetSkipInfo(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerBinarySetSkipOptions(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetSkipOptions(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerBinarySetSkipHeader(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryGetSkipHeader(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryReadStringArray(PetscViewer, char ***);
PETSC_EXTERN PetscErrorCode PetscViewerBinaryWriteStringArray(PetscViewer, const char *const *);

PETSC_EXTERN PetscErrorCode PetscViewerFileSetName(PetscViewer, const char[]);
PETSC_EXTERN PetscErrorCode PetscViewerFileGetName(PetscViewer, const char *[]);

PETSC_EXTERN PetscErrorCode PetscViewerVUGetPointer(PetscViewer, FILE **);
PETSC_EXTERN PetscErrorCode PetscViewerVUSetVecSeen(PetscViewer, PetscBool);
PETSC_EXTERN PetscErrorCode PetscViewerVUGetVecSeen(PetscViewer, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscViewerVUPrintDeferred(PetscViewer, const char[], ...) PETSC_ATTRIBUTE_FORMAT(2, 3);
PETSC_EXTERN PetscErrorCode PetscViewerVUFlushDeferred(PetscViewer);

/*@C
  PetscViewerVUSetMode - Sets the mode in which to open the file.

  Not Collective

  Input Parameters:
+ viewer - The `PetscViewer`
- mode   - The file mode

  Level: deprecated

  Note:
  Use `PetscViewerFileSetMode()` instead.

.seealso: [](sec_viewers), `PetscViewer`, `PetscViewerFileSetMode()`
@*/
PETSC_DEPRECATED_FUNCTION(3, 15, 0, "PetscViewerFileSetMode()", ) static inline PetscErrorCode PetscViewerVUSetMode(PetscViewer viewer, PetscFileMode mode)
{
  return PetscViewerFileSetMode(viewer, mode);
}

PETSC_EXTERN PetscErrorCode PetscViewerMathematicaInitializePackage(void);
PETSC_EXTERN PetscErrorCode PetscViewerMathematicaFinalizePackage(void);
PETSC_EXTERN PetscErrorCode PetscViewerMathematicaGetName(PetscViewer, const char **);
PETSC_EXTERN PetscErrorCode PetscViewerMathematicaSetName(PetscViewer, const char[]);
PETSC_EXTERN PetscErrorCode PetscViewerMathematicaClearName(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerMathematicaSkipPackets(PetscViewer, int);

PETSC_EXTERN PetscErrorCode PetscViewerSiloClearName(PetscViewer);
PETSC_EXTERN PetscErrorCode PetscViewerSiloClearMeshName(PetscViewer);

typedef enum {
  PETSC_VTK_INVALID,
  PETSC_VTK_POINT_FIELD,
  PETSC_VTK_POINT_VECTOR_FIELD,
  PETSC_VTK_CELL_FIELD,
  PETSC_VTK_CELL_VECTOR_FIELD
} PetscViewerVTKFieldType;
PETSC_EXTERN PetscErrorCode PetscViewerVTKAddField(PetscViewer, PetscObject, PetscErrorCode (*PetscViewerVTKWriteFunction)(PetscObject, PetscViewer), PetscInt, PetscViewerVTKFieldType, PetscBool, PetscObject);
PETSC_EXTERN PetscErrorCode PetscViewerVTKGetDM(PetscViewer, PetscObject *);
PETSC_EXTERN PetscErrorCode PetscViewerVTKOpen(MPI_Comm, const char[], PetscFileMode, PetscViewer *);

/*
     These are all the default viewers that do not have to be explicitly opened
*/
PETSC_EXTERN PetscViewer    PETSC_VIEWER_STDOUT_(MPI_Comm);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIGetStdout(MPI_Comm, PetscViewer *);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_STDERR_(MPI_Comm);
PETSC_EXTERN PetscErrorCode PetscViewerASCIIGetStderr(MPI_Comm, PetscViewer *);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_DRAW_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_SOCKET_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_BINARY_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_MATLAB_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_HDF5_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_GLVIS_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_EXODUSII_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_PYTHON_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_PYVISTA_(MPI_Comm);
PETSC_EXTERN PetscViewer    PETSC_VIEWER_MATHEMATICA_WORLD_PRIVATE;

/*MC
  PETSC_VIEWER_STDERR_SELF  - same as `PETSC_VIEWER_STDERR_`(PETSC_COMM_SELF)

  Level: beginner
M*/
#define PETSC_VIEWER_STDERR_SELF PETSC_VIEWER_STDERR_(PETSC_COMM_SELF)

/*MC
  PETSC_VIEWER_STDERR_WORLD  - same as `PETSC_VIEWER_STDERR_`(PETSC_COMM_WORLD)

  Level: beginner
M*/
#define PETSC_VIEWER_STDERR_WORLD PETSC_VIEWER_STDERR_(PETSC_COMM_WORLD)

/*MC
  PETSC_VIEWER_STDOUT_WORLD  - same as `PETSC_VIEWER_STDOUT_`(PETSC_COMM_WORLD)

  Level: beginner
M*/
#define PETSC_VIEWER_STDOUT_WORLD PETSC_VIEWER_STDOUT_(PETSC_COMM_WORLD)

/*MC
  PETSC_VIEWER_STDOUT_SELF  - same as `PETSC_VIEWER_STDOUT_`(PETSC_COMM_SELF)

  Level: beginner
M*/
#define PETSC_VIEWER_STDOUT_SELF PETSC_VIEWER_STDOUT_(PETSC_COMM_SELF)

/*MC
  PETSC_VIEWER_DRAW_WORLD  - same as `PETSC_VIEWER_DRAW_`(PETSC_COMM_WORLD)

  Level: intermediate
M*/
#define PETSC_VIEWER_DRAW_WORLD PETSC_VIEWER_DRAW_(PETSC_COMM_WORLD)

/*MC
  PETSC_VIEWER_DRAW_SELF  - same as `PETSC_VIEWER_DRAW_`(PETSC_COMM_SELF)

  Level: intermediate
M*/
#define PETSC_VIEWER_DRAW_SELF PETSC_VIEWER_DRAW_(PETSC_COMM_SELF)

/*MC
  PETSC_VIEWER_SOCKET_WORLD  - same as `PETSC_VIEWER_SOCKET_`(PETSC_COMM_WORLD)

  Level: intermediate
M*/
#define PETSC_VIEWER_SOCKET_WORLD PETSC_VIEWER_SOCKET_(PETSC_COMM_WORLD)

/*MC
  PETSC_VIEWER_SOCKET_SELF  - same as `PETSC_VIEWER_SOCKET_`(PETSC_COMM_SELF)

  Level: intermediate
M*/
#define PETSC_VIEWER_SOCKET_SELF PETSC_VIEWER_SOCKET_(PETSC_COMM_SELF)

/*MC
  PETSC_VIEWER_BINARY_WORLD  - same as `PETSC_VIEWER_BINARY_`(PETSC_COMM_WORLD)

  Level: intermediate
M*/
#define PETSC_VIEWER_BINARY_WORLD PETSC_VIEWER_BINARY_(PETSC_COMM_WORLD)

/*MC
  PETSC_VIEWER_BINARY_SELF  - same as `PETSC_VIEWER_BINARY_`(PETSC_COMM_SELF)

  Level: intermediate
M*/
#define PETSC_VIEWER_BINARY_SELF PETSC_VIEWER_BINARY_(PETSC_COMM_SELF)

/*MC
  PETSC_VIEWER_MATLAB_WORLD  - same as `PETSC_VIEWER_MATLAB_`(PETSC_COMM_WORLD)

  Level: intermediate
M*/
#define PETSC_VIEWER_MATLAB_WORLD PETSC_VIEWER_MATLAB_(PETSC_COMM_WORLD)

/*MC
  PETSC_VIEWER_MATLAB_SELF  - same as `PETSC_VIEWER_MATLAB_`(PETSC_COMM_SELF)

  Level: intermediate
M*/
#define PETSC_VIEWER_MATLAB_SELF PETSC_VIEWER_MATLAB_(PETSC_COMM_SELF)

#define PETSC_VIEWER_MATHEMATICA_WORLD (PetscViewerInitializeMathematicaWorld_Private(), PETSC_VIEWER_MATHEMATICA_WORLD_PRIVATE)

PETSC_EXTERN PetscErrorCode PetscViewerFlowControlStart(PetscViewer, PetscInt *, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscViewerFlowControlStepMain(PetscViewer, PetscInt, PetscInt *, PetscInt);
PETSC_EXTERN PetscErrorCode PetscViewerFlowControlEndMain(PetscViewer, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscViewerFlowControlStepWorker(PetscViewer, PetscMPIInt, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscViewerFlowControlEndWorker(PetscViewer, PetscInt *);
PETSC_DEPRECATED_FUNCTION(3, 15, 0, "PetscViewerFlowControlStepMain()", ) static inline PetscErrorCode PetscViewerFlowControlStepMaster(PetscViewer viewer, PetscInt i, PetscInt *mcnt, PetscInt cnt)
{
  return PetscViewerFlowControlStepMain(viewer, i, mcnt, cnt);
}
PETSC_DEPRECATED_FUNCTION(3, 15, 0, "PetscViewerFlowControlEndMain()", ) static inline PetscErrorCode PetscViewerFlowControlEndMaster(PetscViewer viewer, PetscInt *mcnt)
{
  return PetscViewerFlowControlEndMain(viewer, mcnt);
}

/*
   PetscViewer writes to MATLAB .mat file
*/
PETSC_EXTERN PetscErrorCode PetscViewerMatlabPutArray(PetscViewer, int, int, const PetscScalar *, const char *);
PETSC_EXTERN PetscErrorCode PetscViewerMatlabGetArray(PetscViewer, int, int, PetscScalar *, const char *);
PETSC_EXTERN PetscErrorCode PetscViewerMatlabPutVariable(PetscViewer, const char *, void *);

#if defined(PETSC_HAVE_SAWS)
PETSC_EXTERN PetscErrorCode PetscObjectViewSAWs(PetscObject, PetscViewer);
#endif

PETSC_EXTERN PetscErrorCode PetscViewerPythonSetType(PetscViewer, const char[]);
PETSC_EXTERN PetscErrorCode PetscViewerPythonGetType(PetscViewer, const char *[]);
PETSC_EXTERN PetscErrorCode PetscViewerPythonCreate(MPI_Comm, const char[], PetscViewer *);
PETSC_EXTERN PetscErrorCode PetscViewerPythonViewObject(PetscViewer, PetscObject);

PETSC_EXTERN PetscErrorCode PetscViewerMonitorLGSetUp(PetscViewer, const char[], const char[], const char[], PetscInt, const char *[], int, int, int, int);

/*S
   PetscViewers - Abstract collection of `PetscViewer`s. It is stored as an expandable array of viewers.

   Level: intermediate

.seealso: [](sec_viewers), `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerType`, `PetscViewer`, `PetscViewersCreate()`,
          `PetscViewersGetViewer()`
S*/
typedef struct _n_PetscViewers *PetscViewers;
PETSC_EXTERN PetscErrorCode     PetscViewersCreate(MPI_Comm, PetscViewers *);
PETSC_EXTERN PetscErrorCode     PetscViewersDestroy(PetscViewers *);
PETSC_EXTERN PetscErrorCode     PetscViewersGetViewer(PetscViewers, PetscInt, PetscViewer *);
