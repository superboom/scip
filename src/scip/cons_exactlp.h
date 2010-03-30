/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2010 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: cons_exactlp.h,v 1.1.2.5 2010/03/30 20:33:26 bzfwolte Exp $"

/**@file   cons_exactlp.h
 * @brief  constraint handler for exactlp constraints
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_CONS_EXACTLP_H__
#define __SCIP_CONS_EXACTLP_H__


#include <mpfr.h> /* mpfr.h has to be included before gmp.h */ /* todo: only necessary because of gmp<->fp functions (which maybe move) ?????? */
#include <gmp.h>
#include "scip/scip.h"
#include "scip/lpiex.h"
#include "scip/solex.h"

#ifdef __cplusplus
extern "C" {
#endif


/** returns value treated as negative infinite in exactlp constraint handler */
extern
const mpq_t* negInfinity(
   SCIP_CONSHDLRDATA*    conshdlrdata        /**< exactlp constraint handler data */
   );

/** checks if value is treated as positive infinite in exactlp constraint handler */
extern
const mpq_t* posInfinity(
   SCIP_CONSHDLRDATA*    conshdlrdata        /**< exactlp constraint handler data */
   );

/** checks if value is treated as negative infinite in exactlp constraint handler */
extern
SCIP_Bool isNegInfinity(
   SCIP_CONSHDLRDATA*    conshdlrdata,       /**< exactlp constraint handler data */
   const mpq_t           val                 /**< value to be compared against infinity */
   );

/** checks if value is treated as positive infinite in exactlp constraint handler */
extern
SCIP_Bool isPosInfinity(
   SCIP_CONSHDLRDATA*    conshdlrdata,       /**< exactlp constraint handler data */
   const mpq_t           val                 /**< value to be compared against infinity */
   );

/** returns whether given rational number can be stored as FP number without roundinf errors */
extern
SCIP_Bool mpqIsReal(
   SCIP*                 scip,               /**< SCIP data structure */
   mpq_t                 val                 /**< given rational number */
   );

/** converts given rational number into an FP number; uses given rounding mode during conversion 
 * (should be used to construct an FP relaxation of a constraint) 
 */
extern
SCIP_Real mpqGetRealRelax(
   SCIP*                 scip,               /**< SCIP data structure */
   const mpq_t           val,                /**< given rational number */
   mp_rnd_t              roundmode           /**< rounding mode to be used for the conversion */
   );

/** converts given rational number into an FP number; uses default rounding mode during conversion 
 * (should be used to construct an FP approximation of a constraint) 
 */
extern
SCIP_Real mpqGetRealApprox(
   SCIP*                 scip,               /**< SCIP data structure */
   const mpq_t           val                 /**< given rational number */
   );

/** creates the handler for exactlp constraints and includes it in SCIP */
extern
SCIP_RETCODE SCIPincludeConshdlrExactlp(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** creates and captures a exactlp constraint */
extern
SCIP_RETCODE SCIPcreateConsExactlp(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS**           cons,               /**< pointer to hold the created constraint */
   const char*           name,               /**< name of constraint */
   SCIP_OBJSEN           objsen,             /**< objective sense */
   int                   nvars,              /**< number of variables */
   mpq_t*                obj,                /**< objective function values of variables */
   mpq_t*                lb,                 /**< lower bounds of variables */
   mpq_t*                ub,                 /**< upper bounds of variables */
   int                   nconss,             /**< number of constraints */
   mpq_t*                lhs,                /**< left hand sides of constraints */
   mpq_t*                rhs,                /**< right hand sides of constraints */
   int                   nnonz,              /**< number of nonzero elements in the constraint matrix */
   int*                  beg,                /**< start index of each constraint in ind and val array */
   int*                  len,                /**< number of nonzeros in val array corresponding to constraint */
   int*                  ind,                /**< variable indices (var->probindex) of constraint matrix entries */
   mpq_t*                val,                /**< values of nonzero constraint matrix entries (and some zeros) */
   SCIP_Bool             objneedscaling,     /**< do objective function values need to be scaled because some are not FP representable? */
   SCIP_Bool             initial,            /**< should the LP relaxation of constraint be in the initial LP?
                                              *   Usually set to TRUE. Set to FALSE for 'lazy constraints'. */
   SCIP_Bool             separate,           /**< should the constraint be separated during LP processing?
                                              *   Usually set to TRUE. */
   SCIP_Bool             enforce,            /**< should the constraint be enforced during node processing?
                                              *   TRUE for model constraints, FALSE for additional, redundant constraints. */
   SCIP_Bool             check,              /**< should the constraint be checked for feasibility?
                                              *   TRUE for model constraints, FALSE for additional, redundant constraints. */
   SCIP_Bool             propagate,          /**< should the constraint be propagated during node processing?
                                              *   Usually set to TRUE. */
   SCIP_Bool             local,              /**< is constraint only valid locally?
                                              *   Usually set to FALSE. Has to be set to TRUE, e.g., for branching constraints. */
   SCIP_Bool             modifiable,         /**< is constraint modifiable (subject to column generation)?
                                              *   Usually set to FALSE. In column generation applications, set to TRUE if pricing
                                              *   adds coefficients to this constraint. */
   SCIP_Bool             dynamic,            /**< is constraint subject to aging?
                                              *   Usually set to FALSE. Set to TRUE for own cuts which 
                                              *   are seperated as constraints. */
   SCIP_Bool             removable,          /**< should the relaxation be removed from the LP due to aging or cleanup?
                                              *   Usually set to FALSE. Set to TRUE for 'lazy constraints' and 'user cuts'. */
   SCIP_Bool             stickingatnode      /**< should the constraint always be kept at the node where it was added, even
                                              *   if it may be moved to a more global node?
                                              *   Usually set to FALSE. Set to TRUE to for constraints that represent node data. */
   );

/** checks if value is treated as positive infinite in exactlp constraint handler */
extern
SCIP_Bool SCIPisPosInfinityExactlp(
   SCIP*                 scip,               /**< SCIP data structure */
   const mpq_t           val                 /**< value to be compared against infinity */
   );

/** checks if value is treated as negative infinite in exactlp constraint handler */
extern
SCIP_Bool SCIPisNegInfinityExactlp(
   SCIP*                 scip,               /**< SCIP data structure */
   const mpq_t           val                 /**< value to be compared against infinity */
   );

/** returns a safe external value of the given exact internal objective value, i.e., a lower and an upper approximation 
 *  if given value is a lower and an upper bound on the optimal objective value, respectively
 */
extern
SCIP_Real SCIPgetExternSafeObjval(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons,               /**< exactlp constraint data */
   SCIP_Real             objval,             /**< safe internal objective value */
   SCIP_Bool             lowerbound          /**< TRUE if objval is a lower bound; FALSE, if it is an upper bound */
   );

/** gets number of feasible exact primal solutions stored in the exact solution storage */
extern
int SCIPgetNSolexs(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** gets best feasible exact primal solution found so far, or NULL if no solution has been found */
extern
SCIP_SOLEX* SCIPgetBestSolex(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** returns objective value of exact primal CIP solution w.r.t. original problem */
extern
void SCIPgetSolexOrigObj(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons,               /**< exactlp constraint data */
   SCIP_SOLEX*           sol,                /**< exact primal solution */
   mpq_t                 obj                 /**< pointer to store objective value */ 
   );

/** returns transformed objective value of exact primal CIP solution */
extern
void SCIPgetSolexTransObj(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_SOLEX*           sol,                /**< exact primal solution */
   mpq_t                 obj                 /**< pointer to store objective value */ 
   );

/** returns objective value of best exact primal CIP solution found so far w.r.t. original problem */
extern
void SCIPgetBestSolexObj(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons,               /**< exactlp constraint data */
   mpq_t                 obj                 /**< pointer to store objective value */ 
   );

/** outputs non-zero variables of exact solution in original problem space to file stream */
extern
SCIP_RETCODE SCIPprintSolex(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons,               /**< exactlp constraint data */
   SCIP_SOLEX*           sol,                /**< exact primal solution */
   FILE*                 file,               /**< output file (or NULL for standard output) */
   SCIP_Bool             printzeros          /**< should variables set to zero be printed? */
   );

/** outputs non-zero variables of exact solution in original problem space in transformed problem space to file stream */
extern
SCIP_RETCODE SCIPprintTransSolex(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_SOLEX*           sol,                /**< exact primal solution */
   FILE*                 file,               /**< output file (or NULL for standard output) */
   SCIP_Bool             printzeros          /**< should variables set to zero be printed? */
   );

/** outputs best feasible exact primal solution found so far to file stream */
extern
SCIP_RETCODE SCIPprintBestSolex(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons,               /**< exactlp constraint data */
   FILE*                 file,               /**< output file (or NULL for standard output) */
   SCIP_Bool             printzeros          /**< should variables set to zero be printed? */
   );

/** outputs best feasible exact primal solution found so far in transformed problem space to file stream */
extern
SCIP_RETCODE SCIPprintBestTransSolex(
   SCIP*                 scip,               /**< SCIP data structure */
   FILE*                 file,               /**< output file (or NULL for standard output) */
   SCIP_Bool             printzeros          /**< should variables set to zero be printed? */
   );

/** outputs value of variable in best feasible exact primal solution found so far to file stream */
extern
SCIP_RETCODE SCIPprintBestSolexVar(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_VAR*             var,                /**< problem variable for which solution value should be printed */
   FILE*                 file                /**< output file (or NULL for standard output) */
   );

/** checks best exact primal solution for feasibility without adding it to the solution store;
 *  called for original exactlp constraints the method is used to double check the best exact solution in order to 
 *  validate the presolving process
 */
extern
SCIP_RETCODE SCIPcheckBestSolex(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONS*            cons,               /**< constraint data */
   SCIP_Bool*            feasible,           /**< stores whether given solution is feasible */
   SCIP_Bool             printreason         /**< should the reason for the violation be printed? */
   );

/** gets exact objective function value of variable */
extern
void SCIPvarGetObjExactlp(
   SCIP_CONS*            cons,               /**< constraint data */
   SCIP_VAR*             var,                /**< problem variable */
   mpq_t                 obj                 /**< pointer to store objective value */
   );

/** gets exact global lower bound of variable */
extern
void SCIPvarGetLbGlobalExactlp(
   SCIP_CONS*            cons,               /**< constraint data */
   SCIP_VAR*             var,                /**< problem variable */
   mpq_t                 lb                  /**< pointer to store global lower bounds */
   );

/** gets exact global upper bound of variable */
void SCIPvarGetUbGlobalExactlp(
   SCIP_CONS*            cons,               /**< constraint data */
   SCIP_VAR*             var,                /**< problem variable */
   mpq_t                 ub                  /**< pointer to store global upper bound */
   );

#ifdef __cplusplus
}
#endif

#endif
