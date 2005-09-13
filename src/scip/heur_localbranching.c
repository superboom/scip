/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2005 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2005 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: heur_localbranching.c,v 1.2 2005/09/13 18:07:58 bzfpfend Exp $"

/**@file   heur_localbranching.c
 * @brief  localbranching primal heuristic
 * @author Timo Berthold
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include "scip/scip.h"
#include "scip/cons_linear.h"
#include "scip/scipdefplugins.h"
#include "scip/heur_localbranching.h"

#define HEUR_NAME             "localbranching"
#define HEUR_DESC             "local branching heuristic by Fischetti and Lodi"
#define HEUR_DISPCHAR         'L'
#define HEUR_PRIORITY         -1010000
#define HEUR_FREQ             -1
#define HEUR_FREQOFS          9
#define HEUR_MAXDEPTH         -1
#define HEUR_PSEUDONODES      TRUE      /* call heuristic at nodes where only a pseudo solution exist?              */
#define HEUR_DURINGPLUNGING   TRUE      /* call heuristic during plunging?                                          */
#define HEUR_DURINGLPLOOP     FALSE     /* call heuristic during the LP price-and-cut loop? */
#define HEUR_AFTERNODE        TRUE      /* call heuristic after or before the current node was solved?              */

#define DEFAULT_NEIGHBOURHOODSIZE  18   /* radius of the incumbents neighbourhood to be searched                    */
#define DEFAULT_NODESOFS      5000      /* number of nodes added to the contingent of the total nodes               */
#define DEFAULT_MAXNODES      10000     /* maximum number of nodes to regard in the subproblem                      */
#define DEFAULT_MINNODES      1000      /* minimum number of nodes required to start the subproblem                 */
#define DEFAULT_NODESQUOT     0.05      /* contingent of sub problem nodes in relation to original nodes            */

#define NEWSOLFOUND           0
#define SOLVEDNOTIMPROVED     1
#define NODELIMITREACHED      2
#define WAITFORNEWSOL         3

/*
 * Data structures
 */

/** primal heuristic data */
struct SCIP_HeurData
{
   int                    nodesofs;           /**< number of nodes added to the contingent of the total nodes       */
   int                    minnodes;           /**< minimum number of nodes required to start the subproblem         */
   int                    maxnodes;           /**< maximum number of nodes to regard in the subproblem              */
   SCIP_Longint           usednodes;          /**< amount of nodes local branching used during all calls            */
   SCIP_Real              nodesquot;          /**< contingent of sub problem nodes in relation to original nodes    */
   int                    neighbourhoodsize;  /**< radius of the incumbent's neighbourhood to be searched           */
   int                    statlastcall;       /**< stores at which status localbranching did stop at the last call  */
   SCIP_SOL*              lastsol;            /**< the last incumbent localbranching used as reference point        */
};


/*
 * Local methods
 */

/** copies the problem of scip to the problem of subscip */
static
SCIP_RETCODE createSubproblem(
   SCIP*                 scip,               /**< SCIP data structure of the original problem                      */
   SCIP*                 subscip,            /**< SCIP data structure of the subproblem                            */
   SCIP_VAR**            subvars             /**< variables of the subproblem                                      */
   )
{
   SCIP_ROW** rows;
   SCIP_VAR** vars;
   int nrows;
   int nvars;
   int i;
   SCIP_SOL* bestsol;
   char name[SCIP_MAXSTRLEN];
    
   /* get the data of the variables and the best solution */
   SCIP_CALL( SCIPgetVarsData(scip, &vars, &nvars, NULL, NULL, NULL, NULL) );
   bestsol = SCIPgetBestSol(scip);
   assert(bestsol != NULL);

   /* get name of the original problem and add the string "_localbranchsub" */
   sprintf(name, "%s_localbranchsub", SCIPgetProbName(scip));

   /* create the subproblem */
   SCIP_CALL( SCIPcreateProb(subscip, name, NULL, NULL, NULL, NULL, NULL, NULL) );

   /* create the variables of the subproblem */
   for ( i = 0; i < nvars; i++ )
   {
      SCIP_CALL( SCIPcreateVar(subscip, &subvars[i], SCIPvarGetName(vars[i]), SCIPvarGetLbGlobal(vars[i]),
               SCIPvarGetUbGlobal(vars[i]), SCIPvarGetObj(vars[i]), SCIPvarGetType(vars[i]),
               SCIPvarIsInitial(vars[i]), SCIPvarIsRemoveable(vars[i]), NULL, NULL, NULL, NULL) );
      SCIP_CALL( SCIPaddVar(subscip, subvars[i]) );
   }

   /* get the rows and their number */
   SCIP_CALL( SCIPgetLPRowsData(scip, &rows, &nrows) ); 
   
   for( i = 0; i < nrows; i++ )
   {
      SCIP_CONS* cons;
      SCIP_VAR** consvars;
      SCIP_COL** cols;
      SCIP_Real constant;
      SCIP_Real lhs;
      SCIP_Real rhs;
      SCIP_Real* vals;
      int nnonz;
      int j;
          
      /* ignore rows that are only locally valid */
      if( SCIProwIsLocal(rows[i]) )
         continue;
      
      /* get the row's data */
      constant = SCIProwGetConstant(rows[i]);
      lhs = SCIProwGetLhs(rows[i]) - constant;
      rhs = SCIProwGetRhs(rows[i]) - constant;
      vals = SCIProwGetVals(rows[i]);
      nnonz = SCIProwGetNNonz(rows[i]);
      cols = SCIProwGetCols(rows[i]);
      
      assert(lhs <= rhs);
      
      /* allocate memory array to be filled with the corresponding subproblem variables */
      SCIP_CALL( SCIPallocBufferArray(scip, &consvars, nnonz) );
      for( j = 0; j < nnonz; j++ ) 
         consvars[j] = subvars[SCIPvarGetProbindex(SCIPcolGetVar(cols[j]))];

      /* create new constraint and add it to subscip */
      SCIP_CALL( SCIPcreateConsLinear(subscip, &cons, SCIProwGetName(rows[i]), nnonz, consvars, vals, lhs, rhs,
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE) );
      SCIP_CALL( SCIPaddCons(subscip, cons) );
      SCIP_CALL( SCIPreleaseCons(subscip, &cons) );
      
      /* free memory */
      SCIPfreeBufferArray(scip, &consvars);
   }

   return SCIP_OKAY;
}

/** create the extra constraint of local branching and add it to subscip */
static
SCIP_RETCODE addLocalBranchingConstraint(
   SCIP*                 scip,               /**< SCIP data structure of the original problem   */
   SCIP*                 subscip,            /**< SCIP data structure of the subproblem         */
   SCIP_VAR**            subvars,            /**< variables of the subproblem                   */
   SCIP_HEURDATA*        heurdata            /**< heuristic's data structure                    */ 
   )
{
   SCIP_CONS* cons;                        /* local branching constraint to create */  
   SCIP_VAR** consvars;
   SCIP_VAR** vars;
   SCIP_SOL* bestsol;

   int nbinvars;
   int i;   
   SCIP_Real rhs;
   SCIP_Real lhs;
   SCIP_Real* consvals;
   char consname[SCIP_MAXSTRLEN];

   sprintf(consname, "%s_localbranchcons", SCIPgetProbName(scip));

   /* get the data of the variables and the best solution */
   SCIP_CALL( SCIPgetVarsData(scip, &vars, NULL, &nbinvars, NULL, NULL, NULL) );   
   bestsol = SCIPgetBestSol(scip);
   assert( bestsol != NULL );

   /* memory allociation */
   SCIP_CALL( SCIPallocBufferArray(scip,&consvars,nbinvars) );  
   SCIP_CALL( SCIPallocBufferArray(scip,&consvals,nbinvars) );
   
   /* determine the right side of the local branching constraint */
   if( heurdata->lastsol != bestsol )
     rhs = heurdata->neighbourhoodsize;
   else if( heurdata->statlastcall == SOLVEDNOTIMPROVED )
      rhs = SCIPfeasCeil(scip,heurdata->neighbourhoodsize * 1.5 );
   else
   { 
      assert( heurdata->statlastcall == NODELIMITREACHED );
      rhs = SCIPfeasFloor( scip,heurdata->neighbourhoodsize * 0.5 );
   }

   lhs = 1.0;

   /* create the distance (to incumbent) function of the binary variables */
   for( i = 0; i < nbinvars; i++ )
   {
      SCIP_Real solval;
      solval = SCIPgetSolVal(scip, bestsol, vars[i]);
      assert( SCIPisFeasIntegral(scip,solval) );

      /* is variable i  part of the binary support of bestsol? */
      if( SCIPisFeasEQ(scip,solval,1.0) )
      {
         consvals[i] = -1.0;
         rhs -= 1.0;
	 lhs -= 1.0;
      }
      else
         consvals[i] = 1.0;
      consvars[i] = subvars[i];  
      assert( SCIPvarGetType(consvars[i]) == SCIP_VARTYPE_BINARY );
   }
      
   /* creates localbranching constraint and adds it to subscip */
   SCIP_CALL( SCIPcreateConsLinear(subscip, &cons, consname, nbinvars, consvars, consvals,
         lhs, rhs, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE) );
   SCIP_CALL( SCIPaddCons(subscip, cons) );
   SCIP_CALL( SCIPreleaseCons(subscip, &cons) );
      
   /* free local memory */
   SCIPfreeBufferArray(scip,&consvals);
   SCIPfreeBufferArray(scip,&consvars);

   return SCIP_OKAY;
}


/** creates a new solution for the original problem by copying the solution of the subproblem */
static
SCIP_RETCODE createNewSol(
   SCIP*                 scip,               /**< SCIP data structure  of the original problem      */
   SCIP*                 subscip,            /**< SCIP data structure  of the subproblem            */
   SCIP_HEUR*            heur,               /**< the Localbranching heuristic                      */
   SCIP_Bool*            success             /**< pointer to store, whether new solution was found  */
)
{
   SCIP_VAR** subvars;
   SCIP_VAR** vars;
   int nvars;
   SCIP_SOL* newsol;
   SCIP_SOL* subsol;
   SCIP_Real* subsolvals;
        
   assert( scip != NULL );
   assert( subscip != NULL );

   subsol = SCIPgetBestSol(subscip);
   assert( subsol != NULL );

   /* copy the solution */
   SCIP_CALL( SCIPgetVarsData(scip, &vars, &nvars, NULL, NULL, NULL, NULL) );
   subvars = SCIPgetOrigVars(subscip);
   assert(nvars == SCIPgetNOrigVars(subscip));
   SCIP_CALL( SCIPallocBufferArray(scip, &subsolvals, nvars) );
   SCIP_CALL( SCIPgetSolVals(subscip, subsol, nvars, subvars, subsolvals) );
       
   /* create new solution for the original problem */
   SCIP_CALL( SCIPcreateSol(scip, &newsol, heur) );
   SCIP_CALL( SCIPsetSolVals(scip, newsol, nvars, vars, subsolvals) );

   SCIP_CALL( SCIPtrySolFree(scip, &newsol, TRUE, TRUE, TRUE, success) );

   SCIPfreeBufferArray(scip, &subsolvals);

   return SCIP_OKAY;
}


/*
 * Callback methods of primal heuristic
 */

/** destructor of primal heuristic to free user data (called when SCIP is exiting) */
static
SCIP_DECL_HEURFREE(heurFreeLocalbranching)
{    
   SCIP_HEURDATA* heurdata;

   assert( heur != NULL );
   assert( scip != NULL );

   /* get heuristic data */
   heurdata = SCIPheurGetData(heur);
   assert( heurdata != NULL );

   /* free heuristic data */
   SCIPfreeMemory(scip, &heurdata);
   SCIPheurSetData(heur, NULL);
   
   return SCIP_OKAY;
}


/** initialization method of primal heuristic (called after problem was transformed) */
static
SCIP_DECL_HEURINIT(heurInitLocalbranching)
{  
   SCIP_HEURDATA* heurdata;

   assert( heur != NULL );
   assert( scip != NULL );

   /* get heuristic's data */
   heurdata = SCIPheurGetData(heur);
   assert( heurdata != NULL );

   /* with a little abuse we initialize the heurdata as if localbranching would have finished its last step regularly */
   heurdata->statlastcall = WAITFORNEWSOL;
   heurdata->lastsol = NULL;

   return SCIP_OKAY;
}


/** deinitialization method of primal heuristic (called before transformed problem is freed) */
#define heurExitLocalbranching NULL

/** solving process initialization method of primal heuristic (called when branch and bound process is about to begin) */
#define heurInitsolLocalbranching NULL

/** solving process deinitialization method of primal heuristic (called before branch and bound process data is freed) */
#define heurExitsolLocalbranching NULL



/** execution method of primal heuristic */
static
SCIP_DECL_HEUREXEC(heurExecLocalbranching)
{ 
   SCIP_HEURDATA* heurdata;
   SCIP* subscip;
   SCIP_VAR** subvars;
   SCIP_SOL* bestsol;                        /* best solution so far */
   SCIP_Real timelimit;                      /* timelimit for subscip (equals remaining time of scip) */
   SCIP_Longint maxnnodes;                   /* maximum number of subnodes */
   SCIP_Longint nsubnodes;                   /* nodelimit for subscip */
   int nvars;
   int i;
   
   assert(heur != NULL);
   assert(scip != NULL);
   assert(result != NULL);

   *result = SCIP_DIDNOTRUN;

   /* get heuristic's data */
   heurdata = SCIPheurGetData(heur);
   assert( heurdata != NULL );

   /* there should be enough binary variables that a local branching constraint makes sense */
   if( SCIPgetNBinVars(scip) < 2*heurdata->neighbourhoodsize )
      return SCIP_OKAY;

   /* only call heuristic, if an optimal LP solution is at hand */
   if( SCIPgetLPSolstat(scip) != SCIP_LPSOLSTAT_OPTIMAL || SCIPgetNSols(scip) <= 0  )
   {
      *result = SCIP_DELAYED;
      return SCIP_OKAY;
   }

   /* if no new solution was found and local branching also seems to fail, just keep on waiting */
   bestsol = SCIPgetBestSol(scip);
   if( heurdata->lastsol == bestsol && heurdata->statlastcall == WAITFORNEWSOL )
   {
      *result = SCIP_DELAYED;
      return SCIP_OKAY;
   }

   /* calculate the maximal number of branching nodes until heuristic is aborted */
   maxnnodes = heurdata->nodesquot * SCIPgetNNodes(scip);

   /* reward local branching if it succeeded often */
   maxnnodes *= 1.0 + 2.0 * ( SCIPheurGetNSolsFound(heur) + 1.0 )  / ( SCIPheurGetNCalls(heur) + 1.0 )  ;
   maxnnodes += heurdata->nodesofs;

   /* determine the node limit for the current process */
   nsubnodes = maxnnodes - heurdata->usednodes;
   nsubnodes = MIN(nsubnodes, heurdata->maxnodes);

   /* check whether we have enough nodes left to call sub problem solving */
   if( nsubnodes < heurdata->minnodes )
      return SCIP_OKAY;
   
   *result = SCIP_DIDNOTFIND;

   nvars = SCIPgetNVars(scip);
   
   /* initializing the subproblem */  
   SCIP_CALL( SCIPallocBufferArray(scip, &subvars, nvars) ); 
   SCIP_CALL( SCIPcreate(&subscip) );
   SCIP_CALL( SCIPincludeDefaultPlugins(subscip) );
 
   SCIP_CALL( SCIPsetIntParam(subscip, "display/verblevel", 0) );

   /* set limits for the subproblem */
   SCIP_CALL( SCIPsetLongintParam(subscip, "limits/nodes", nsubnodes) ); 
   SCIP_CALL( SCIPsetIntParam(subscip, "limits/bestsol", 1) );
   SCIP_CALL( SCIPgetRealParam(scip, "limits/time", &timelimit) );
   SCIP_CALL( SCIPsetRealParam(subscip, "limits/time", timelimit - SCIPgetTotalTime(scip) + 10 ) );

   /* forbid recursive call of local branching as well as usage of rins */
   SCIP_CALL( SCIPsetIntParam(subscip, "heuristics/localbranching/freq", -1) );
   SCIP_CALL( SCIPsetIntParam(subscip, "heuristics/rins/freq", -1) );

   /* disable heuristics which aim to feasibility instead of optimality */
   SCIP_CALL( SCIPsetIntParam(subscip, "heuristics/feaspump/freq", -1) );
   SCIP_CALL( SCIPsetIntParam(subscip, "heuristics/octane/freq", -1) );
   SCIP_CALL( SCIPsetIntParam(subscip, "heuristics/objpscostdiving/freq", -1) );
   SCIP_CALL( SCIPsetIntParam(subscip, "heuristics/rootsoldiving/freq", -1) );

   /* disable cut separation in sub problem */
   SCIP_CALL( SCIPsetIntParam(subscip, "separating/maxrounds", 0) );
   SCIP_CALL( SCIPsetIntParam(subscip, "separating/maxroundsroot", 0) );
   SCIP_CALL( SCIPsetIntParam(subscip, "separating/maxcuts", 0) ); 
   SCIP_CALL( SCIPsetIntParam(subscip, "separating/maxcutsroot", 0) );
   
   /* use pseudo cost branching without strong branching */
   SCIP_CALL( SCIPsetIntParam(subscip, "branching/pscost/priority", INT_MAX) );

   /* disable expensive presolving */
   SCIP_CALL( SCIPsetIntParam(subscip, "presolving/probing/maxrounds", 0) );
   SCIP_CALL( SCIPsetIntParam(subscip, "constraints/linear/maxpresolpairrounds", 0) );
   SCIP_CALL( SCIPsetRealParam(subscip, "constraints/linear/maxaggrnormscale", 0.0) );

   /* disable conflict analysis */
   SCIP_CALL( SCIPsetBoolParam(subscip, "conflict/useprop", FALSE) ); 
   SCIP_CALL( SCIPsetBoolParam(subscip, "conflict/uselp", FALSE) ); 
   SCIP_CALL( SCIPsetBoolParam(subscip, "conflict/usesb", FALSE) ); 
   SCIP_CALL( SCIPsetBoolParam(subscip, "conflict/usepseudo", FALSE) );
 
   /* copy the original problem and add the local branching constraint */
   createSubproblem(scip, subscip, subvars);
   addLocalBranchingConstraint(scip,subscip,subvars,heurdata);

   /* add an objective cutoff */
   SCIP_CALL( SCIPsetObjlimit(subscip, SCIPgetSolTransObj(scip, bestsol) - SCIPepsilon(scip)) );

   /* solve the subproblem */  
   SCIP_CALL( SCIPsolve(subscip) ); 
   heurdata->usednodes += SCIPgetNNodes(subscip);
   
   /* check, whether a solution was found */
   if( SCIPgetNSols(subscip) > 0 )
   {
      SCIP_Bool success;
      success = FALSE;
      SCIP_CALL( createNewSol(scip, subscip, heur, &success) );
      if( success )
         *result = SCIP_FOUNDSOL;  
   }

   heurdata->lastsol = bestsol;

   /* store the status at which localbranching stopped */
   if( bestsol != SCIPgetBestSol(scip) )
     heurdata->statlastcall = NEWSOLFOUND; 
   else if( SCIPgetStatus(subscip) == SCIP_STATUS_NODELIMIT  
       && ( heurdata->statlastcall == NEWSOLFOUND || heurdata->statlastcall == WAITFORNEWSOL ) )
     heurdata->statlastcall = NODELIMITREACHED;
   else if( SCIPgetStatus(subscip) == SCIP_STATUS_OPTIMAL 
       && ( heurdata->statlastcall == NEWSOLFOUND || heurdata->statlastcall == WAITFORNEWSOL ) )
     heurdata->statlastcall = SOLVEDNOTIMPROVED;
   else
     heurdata->statlastcall = WAITFORNEWSOL;

   /* free subproblem */
   SCIP_CALL( SCIPfreeTransform(subscip) );
   for( i = 0; i < nvars; i++ )
   {
      SCIP_CALL( SCIPreleaseVar(subscip, &subvars[i]) );
   }
   SCIPfreeBufferArray(scip, &subvars);
   SCIP_CALL( SCIPfree(&subscip) );

   return SCIP_OKAY;
}


/*
 * primal heuristic specific interface methods
 */

/** creates the localbranching primal heuristic and includes it in SCIP */
SCIP_RETCODE SCIPincludeHeurLocalbranching(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_HEURDATA* heurdata;

   /* create localbranching primal heuristic data */
   SCIP_CALL( SCIPallocMemory(scip, &heurdata) );

   /* include primal heuristic */
   SCIP_CALL( SCIPincludeHeur(scip, HEUR_NAME, HEUR_DESC, HEUR_DISPCHAR, HEUR_PRIORITY, HEUR_FREQ, HEUR_FREQOFS,
         HEUR_MAXDEPTH, HEUR_PSEUDONODES, HEUR_DURINGPLUNGING, HEUR_DURINGLPLOOP, HEUR_AFTERNODE,
         heurFreeLocalbranching, heurInitLocalbranching, heurExitLocalbranching, 
         heurInitsolLocalbranching, heurExitsolLocalbranching, heurExecLocalbranching,
         heurdata) );

   /* add localbranching primal heuristic parameters */
   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/localbranching/nodesofs",
         "number of nodes added to the contingent of the total nodes",
         &heurdata->nodesofs, DEFAULT_NODESOFS, 0, INT_MAX, NULL, NULL) );
   
   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/localbranching/neighbourhoodsize",
         "radius (using Manhattan metric) of the incumbent's neighbourhood to be searched",
         &heurdata->neighbourhoodsize, DEFAULT_NEIGHBOURHOODSIZE, 1, INT_MAX, NULL, NULL) );
   
   SCIP_CALL( SCIPaddRealParam(scip, "heuristics/localbranching/nodesquot",
         "contingent of sub problem nodes in relation to the number of nodes of the original problem",
         &heurdata->nodesquot, DEFAULT_NODESQUOT, 0.0, 1.0, NULL, NULL) );
   
   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/localbranching/minnodes",
         "minimum number of nodes required to start the subproblem",
         &heurdata->minnodes, DEFAULT_MINNODES, 0, INT_MAX, NULL, NULL) );
   
   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/localbranching/maxnodes",
         "maximum number of nodes to regard in the subproblem",
         &heurdata->maxnodes, DEFAULT_MAXNODES, 0, INT_MAX, NULL, NULL) );

   return SCIP_OKAY;
}
