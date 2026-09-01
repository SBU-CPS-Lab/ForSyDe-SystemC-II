/* ------------------------------------------------------------------------- 
 * fmi.h
 * Struct with the corresponding function pointers for FMI 2.0.
 * Copyright QTronic GmbH. All rights reserved.
 * -------------------------------------------------------------------------*/

#ifndef FMI_H
#define FMI_H

#ifdef _MSC_VER
#include <windows.h>
#define WINDOWS 1
#else /* _MSC_VER */
#include <errno.h>
#define WINDOWS 0
#define TRUE 1
#define FALSE 0
#define min(a,b) (a>b ? b : a)
#define HMODULE void *
/* See http://www.yolinux.com/TUTORIALS/LibraryArchives-StaticAndDynamic.html */
#include <dlfcn.h>
#endif /* _MSC_VER */

#include "fmi2Functions.h"

#include "XmlParserCApi.h"

// The FMU-description XML parser (D15) ships as three .cpp files rather
// than headers, matching its upstream layout, and this library is
// header-only, so the implementations are included directly here in
// their dependency order rather than built as a separate static library
// for one optional backend.
//
// KNOWN LIMITATION -- FORSYDE_WITH_FMI is single-translation-unit only.
// These are real .cpp files with ordinary external linkage throughout
// (out-of-line member definitions, plus the XmlParser::elmNames and
// XmlParser::attNames static data members), so including forsyde.hpp
// with FORSYDE_WITH_FMI from two translation units and linking them
// fails with a multiple-definition error for each. Verified directly.
//
// That is the same defect class as D1, but it is NOT fixed by D1's fix
// and it is not confined to these three files: the vendored QTronic FMU
// SDK header sim_support.h, which ct_wrappers.hpp pulls in, does the
// same thing for unzip(), loadFMU(), error() and a dozen more, and it
// has no declarations-only counterpart to include instead. A guard that
// suppressed only the block below would therefore still not produce a
// linkable two-TU FMI model, so there deliberately isn't one -- the fix
// is to compile the FMU SDK and this parser once as a real library
// rather than #including .cpp files at all, which belongs with the
// Phase-1b rework that is already going to rebuild this backend.
//
// The core library is multi-TU clean and stays that way via
// tests/multi_tu; the GDB backend was verified the same way by hand, and
// parallel_sim_helpers.hpp (MPI) defines nothing that is neither a
// template nor inline.
#include "XmlElement.cpp"
#include "XmlParser.cpp"
#include "XmlParserCApi.cpp"

typedef struct {
    ModelDescription* modelDescription;

    HMODULE dllHandle; // fmu.dll handle
    /***************************************************
    Common Functions
    ****************************************************/
    fmi2GetTypesPlatformTYPE         *getTypesPlatform;
    fmi2GetVersionTYPE               *getVersion;
    fmi2SetDebugLoggingTYPE          *setDebugLogging;
    fmi2InstantiateTYPE              *instantiate;
    fmi2FreeInstanceTYPE             *freeInstance;
    fmi2SetupExperimentTYPE          *setupExperiment;
    fmi2EnterInitializationModeTYPE  *enterInitializationMode;
    fmi2ExitInitializationModeTYPE   *exitInitializationMode;
    fmi2TerminateTYPE                *terminate;
    fmi2ResetTYPE                    *reset;
    fmi2GetRealTYPE                  *getReal;
    fmi2GetIntegerTYPE               *getInteger;
    fmi2GetBooleanTYPE               *getBoolean;
    fmi2GetStringTYPE                *getString;
    fmi2SetRealTYPE                  *setReal;
    fmi2SetIntegerTYPE               *setInteger;
    fmi2SetBooleanTYPE               *setBoolean;
    fmi2SetStringTYPE                *setString;
    fmi2GetFMUstateTYPE              *getFMUstate;
    fmi2SetFMUstateTYPE              *setFMUstate;
    fmi2FreeFMUstateTYPE             *freeFMUstate;
    fmi2SerializedFMUstateSizeTYPE   *serializedFMUstateSize;
    fmi2SerializeFMUstateTYPE        *serializeFMUstate;
    fmi2DeSerializeFMUstateTYPE      *deSerializeFMUstate;
    fmi2GetDirectionalDerivativeTYPE *getDirectionalDerivative;
    /***************************************************
    Functions for FMI2 for Co-Simulation
    ****************************************************/
    fmi2SetRealInputDerivativesTYPE  *setRealInputDerivatives;
    fmi2GetRealOutputDerivativesTYPE *getRealOutputDerivatives;
    fmi2DoStepTYPE                   *doStep;
    fmi2CancelStepTYPE               *cancelStep;
    fmi2GetStatusTYPE                *getStatus;
    fmi2GetRealStatusTYPE            *getRealStatus;
    fmi2GetIntegerStatusTYPE         *getIntegerStatus;
    fmi2GetBooleanStatusTYPE         *getBooleanStatus;
    fmi2GetStringStatusTYPE          *getStringStatus;
    /***************************************************
    Functions for FMI2 for Model Exchange
    ****************************************************/
    fmi2EnterEventModeTYPE                *enterEventMode;
    fmi2NewDiscreteStatesTYPE             *newDiscreteStates;
    fmi2EnterContinuousTimeModeTYPE       *enterContinuousTimeMode;
    fmi2CompletedIntegratorStepTYPE       *completedIntegratorStep;
    fmi2SetTimeTYPE                       *setTime;
    fmi2SetContinuousStatesTYPE           *setContinuousStates;
    fmi2GetDerivativesTYPE                *getDerivatives;
    fmi2GetEventIndicatorsTYPE            *getEventIndicators;
    fmi2GetContinuousStatesTYPE           *getContinuousStates;
    fmi2GetNominalsOfContinuousStatesTYPE *getNominalsOfContinuousStates;
} FMU;

#endif // FMI_H

