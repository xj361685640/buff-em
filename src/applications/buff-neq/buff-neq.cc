/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of BUFF-EM.
 *
 * BUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * BUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * buff-neq    -- a standalone code within the buff-em suite
 *             -- for implementing the fluctuating-surface-current
 *             -- approach to nonequilibrium phenomena (more
 *             -- specifically, for computing heat radiation,
 *             -- heat transfer, and nonequilibrium casimir forces)
 *
 * homer reid  -- 5/2012
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "buff-neq.h"
#include "SVTensor.h"
#include <libhrutil.h>

/***************************************************************/
/***************************************************************/
/***************************************************************/
#define MAXFREQ  10    // max number of frequencies
#define MAXCACHE 10    // max number of cache files for preload

#define MAXTEMPS 10    // max number of objects for which temperatures may be set


#define II cdouble(0.0,1.0)

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[])
{
  InstallHRSignalHandler();

  /***************************************************************/
  /* process options *********************************************/
  /***************************************************************/
  char *GeoFile=0;
  char *TransFile=0;

  /*--------------------------------------------------------------*/
  bool PAbs=false;
  bool PRad=false;
  bool XForce=false;
  bool YForce=false;
  bool ZForce=false;
  bool XTorque=false;
  bool YTorque=false;
  bool ZTorque=false;

  /*--------------------------------------------------------------*/
  cdouble OmegaVals[MAXFREQ];           int nOmegaVals;
  char *OmegaFile=0;
  double OmegaMin=0.01;                 int nOmegaMin;
  double OmegaMax=10.0;                 int nOmegaMax;
  char *OmegaQuadrature="adaptive";
  double AbsTol=0.0;
  double RelTol=1.0e-2;
  int Intervals=25;

  /*--------------------------------------------------------------*/
  #define MAXOBJS    10
  char *TempArgs[2*MAXOBJS];            int nTempArgs;
  char *TempFileArgs[2*MAXOBJS];        int nTempFileArgs;

  /*--------------------------------------------------------------*/
  bool DoOPFT      = false;
  bool DoEMTPFT    = false;
  bool DoMomentPFT = false;
  int DSIPoints    = 0;
  char *DSIMesh    = 0;
  double DSIRadius = 10.0;
  char *DSIOmegaFile=0;
  int DSIPoints2   = 0;

  /*--------------------------------------------------------------*/
  char *FileBase=0;
  bool UseExistingData=false;

  /* name               type    #args  max_instances  storage           count         description*/
  OptStruct OSArray[]=
   { 
     {"Geometry",       PA_STRING,  1, 1,       (void *)&GeoFile,    0,             "geometry file"},
/**/     
     {"TransFile",      PA_STRING,  1, 1,       (void *)&TransFile,  0,             "list of geometrical transformation"},
/**/     
     {"Temperature",     PA_STRING,  2, MAXOBJS, (void *)TempArgs,     &nTempArgs,     ""},
     {"TemperatureFile", PA_STRING,  2, MAXOBJS, (void *)TempFileArgs, &nTempFileArgs, ""},
/**/     
     {"Power",          PA_BOOL,    0, 1,       (void *)&PAbs,       0,             "compute power transfer"},
     {"PAbs",           PA_BOOL,    0, 1,       (void *)&PAbs,       0,             "(synonym for --power)"},
     {"PRad",           PA_BOOL,    0, 1,       (void *)&PRad,       0,             "compute power transfer"},
     {"XForce",         PA_BOOL,    0, 1,       (void *)&XForce,     0,             "compute X-force"},
     {"YForce",         PA_BOOL,    0, 1,       (void *)&YForce,     0,             "compute Y-force"},
     {"ZForce",         PA_BOOL,    0, 1,       (void *)&ZForce,     0,             "compute Z-force"},
     {"XTorque",        PA_BOOL,    0, 1,       (void *)&XTorque,    0,             "compute X-torque"},
     {"YTorque",        PA_BOOL,    0, 1,       (void *)&YTorque,    0,             "compute Y-torque"},
     {"ZTorque",        PA_BOOL,    0, 1,       (void *)&ZTorque,    0,             "compute Z-torque"},
/**/     
     {"Omega",          PA_CDOUBLE, 1, MAXFREQ, (void *)OmegaVals,   &nOmegaVals,   "(angular) frequency"},
     {"OmegaFile",      PA_STRING,  1, 1,       (void *)&OmegaFile,   0,            "list of (angular) frequencies"},
     {"OmegaMin",       PA_DOUBLE,  1, 1,       (void *)&OmegaMin,   &nOmegaMin,    "lower integration limit"},
     {"OmegaMax",       PA_DOUBLE,  1, 1,       (void *)&OmegaMax,   &nOmegaMax,    "upper integration limit"},
     {"OmegaQuadrature",PA_STRING,  1, 1,       (void *)&OmegaQuadrature, 0,        "adaptive | trapsimp"},
     {"AbsTol",         PA_DOUBLE,  1, 1,       (void *)&AbsTol,      0,            "absolute integration tolerance"},
     {"RelTol",         PA_DOUBLE,  1, 1,       (void *)&RelTol,      0,            "relative integration tolerance"},
     {"Intervals",      PA_INT,     1, 1,       (void *)&Intervals,  0,             "number of intervals for frequency quadrature"},
/**/     
     {"OPFT",           PA_BOOL,    0, 1,       (void *)&DoOPFT,      0,            "do overlap PFT computation"},
     {"EMTPFT",         PA_BOOL,    0, 1,       (void *)&DoEMTPFT,    0,            "do J dot E PFT computation"},
     {"MomentPFT",      PA_BOOL,    0, 1,       (void *)&DoMomentPFT, 0,            "do J dot E PFT computation"},
     {"DSIPoints",      PA_INT,     1, 1,       (void *)&DSIPoints,   0,            "number of quadrature points for DSIPFT"},
     {"DSIMesh",        PA_STRING,  1, 1,       (void *)&DSIMesh,     0,            "bounding surface .msh file for DSIPFT"},
     {"DSIRadius",      PA_DOUBLE,  1, 1,       (void *)&DSIRadius,   0,            "bounding-sphere radius for DSIPFT"},
     {"DSIPoints2",     PA_INT,     1, 1,       (void *)&DSIPoints2,  0,            "number of quadrature points for DSIPFT 2"},
     {"DSIOmegaFile",   PA_STRING,  1, 1,       (void *)&DSIOmegaFile,0,            "list of frequencies at which to do DSI calculation"},
/**/     
     {"FileBase",       PA_STRING,  1, 1,       (void *)&FileBase,   0,             "base filename for output files"},
/**/
     {"UseExistingData", PA_BOOL,   0, 1,       (void *)&UseExistingData, 0,        "read existing data from .flux files"},
/**/
     {0,0,0,0,0,0,0}
   };
  ProcessOptions(argc, argv, OSArray);

  if (GeoFile==0)
   OSUsage(argv[0], OSArray, "--geometry option is mandatory");

  if (FileBase)
   SetLogFileName("%s.log",FileBase);
  InitializeLog(argv[0]);

  /*******************************************************************/
  /* process frequency-related options to construct a list of        */
  /* frequencies at which to run simulations                         */
  /*******************************************************************/
  HVector *OmegaPoints=0, *OmegaPoints0;
  int NumFreqs=0;
  if (OmegaFile) // first process --OmegaFile option if present
   { 
     OmegaPoints=new HVector(OmegaFile,LHM_TEXT);
     if (OmegaPoints->ErrMsg)
      ErrExit(OmegaPoints->ErrMsg);
     NumFreqs=OmegaPoints->N;
     Log("Read %i frequencies from file %s.",NumFreqs,OmegaFile);
   };

  // now add any individually specified --Omega options
  if (nOmegaVals>0)
   { 
     NumFreqs += nOmegaVals;
     OmegaPoints0=OmegaPoints;
     OmegaPoints=new HVector(NumFreqs, LHM_COMPLEX);
     int nFreq=0;
     if (OmegaPoints0)
      { for(nFreq=0; nFreq<OmegaPoints0->N; nFreq++)
         OmegaPoints->SetEntry(nFreq, OmegaPoints0->GetEntry(nFreq));
        delete OmegaPoints0;
      };
     for(int nOV=0; nOV<nOmegaVals; nOV++)
      OmegaPoints->SetEntry(nFreq+nOV, OmegaVals[nOV]);
     Log("Read %i frequencies from command line.",nOmegaVals);
   };

  /*******************************************************************/
  /* determine which output quantities were requested. ***************/
  /*******************************************************************/
  int QuantityFlags=0;
  if (PAbs)    QuantityFlags|=QFLAG_PABS;
  if (PRad)    QuantityFlags|=QFLAG_PRAD;
  if (XForce)  QuantityFlags|=QFLAG_XFORCE;
  if (YForce)  QuantityFlags|=QFLAG_YFORCE;
  if (ZForce)  QuantityFlags|=QFLAG_ZFORCE;
  if (XTorque) QuantityFlags|=QFLAG_XTORQUE;
  if (YTorque) QuantityFlags|=QFLAG_YTORQUE;
  if (ZTorque) QuantityFlags|=QFLAG_ZTORQUE;
  if (NumFreqs==0 && QuantityFlags==0)
   ErrExit("you must specify at least one quantity to compute");

  /*******************************************************************/
  /* check that the user didn't simultaneously ask for a discret set */
  /* of frequencies and a frequency range over which to integrate;   */
  /* if a range was specified check that it makes sense              */
  /*******************************************************************/
  if ( OmegaPoints ) 
   { if ( nOmegaMin>0 || nOmegaMax>0 )
      ErrExit("--OmegaMin/--OmegaMax options may not be used with --Omega/--OmegaFile");
     Log("Computing spectral density at %i frequencies.",NumFreqs);
   }
  else
   { 
     if ( nOmegaMin==1 && OmegaMin<0.0 )
      ErrExit("invalid value specified for --OmegaMin");
     if ( nOmegaMax==1 && OmegaMax<OmegaMin )
      ErrExit("invalid value specified for --OmegaMax");

     if ( OmegaMax==-1.0 )
      Log("Integrating over range Omega=(%g,infinity).",OmegaMin);
     else
      Log("Integrating over range Omega=(%g,%g).",OmegaMin,OmegaMax);
   };

  /*******************************************************************/
  /* create the BNEQData structure that contains all the info needed */
  /* to evaluate the neq transfer at a single frequency              */
  /*******************************************************************/
  BNEQData *BNEQD=CreateBNEQData(GeoFile, TransFile, QuantityFlags, FileBase,
                                 DoOPFT, DoEMTPFT, DoMomentPFT,
                                 DSIPoints, DSIRadius, DSIMesh,
                                 DSIPoints2);

  SWGGeometry *G=BNEQD->G;
  BNEQD->UseExistingData = UseExistingData;

  if (DSIOmegaFile)
   BNEQD->DSIOmegaPoints = new HVector(DSIOmegaFile);

  /*******************************************************************/
  /*******************************************************************/
  /*******************************************************************/
  for(int n=0; n<nTempArgs; n++)
   { int no;
     G->GetObjectByLabel(TempArgs[2*n],&no);
     if (no==-1) 
      ErrExit("unknown object %s in --Temperature specification",TempArgs[2*n]);
     double T;
     if (1!=sscanf(TempArgs[2*n+1],"%le",&T))
      ErrExit("invalid temperature %s in --Temperature specification",TempArgs[2*n+1]);
     char Name[100];
     snprintf(Name,100,"CONST_EPS_%e",T);
     BNEQD->TemperatureSVTs[no]=new SVTensor(Name,true);
     Log("Setting temperature of object %s to T=%e Kelvin.",TempArgs[2*n],T);
   };

  for(int n=0; n<nTempFileArgs; n++)
   { int no;
     G->GetObjectByLabel(TempFileArgs[2*n],&no);
     if (no==-1) 
      ErrExit("unknown object %s in --TempFile specification",TempFileArgs[2*n]);
     BNEQD->TemperatureSVTs[no]=new SVTensor(TempFileArgs[2*n+1],false);
     Log("Read temperature SVT for object %s from %s.",TempFileArgs[2*n],TempFileArgs[2*n+1]);
   };

  if (nTempArgs>0 || nTempFileArgs>0)
   for(int no=0; no<G->NumObjects; no++)
    if (BNEQD->TemperatureSVTs[no]==0)
     BNEQD->TemperatureSVTs[no]=new SVTensor("CONST_EPS_0.0",true);

  /*******************************************************************/
  /* now switch off based on the requested frequency behavior to     */
  /* perform the actual calculations                                 */
  /*******************************************************************/
  if (NumFreqs>0)
   { 
     int FluxVectorLength = BNEQD->NumTransformations * G->NumObjects* G->NumObjects*NUMPFT;
     double *Flux=new double[FluxVectorLength];
     for (int nFreq=0; nFreq<NumFreqs; nFreq++)
      if (nTempArgs>0 || nTempFileArgs>0)
       GetNEQIntegrand(BNEQD, OmegaPoints->GetEntry(nFreq), Flux);
      else
       GetFlux(BNEQD, OmegaPoints->GetEntry(nFreq), Flux);
   }
  else
   { if ( !strcasecmp(OmegaQuadrature,"adaptive") )
      EvaluateFrequencyIntegral_Adaptive(BNEQD, OmegaMin, OmegaMax, AbsTol, RelTol);
     else
      EvaluateFrequencyIntegral_TrapSimp(BNEQD, OmegaMin, OmegaMax, Intervals);
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  printf("Thank you for your support.\n");

}


