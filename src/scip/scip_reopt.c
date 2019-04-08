/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   scip_reopt.c
 * @brief  public methods for reoptimization
 * @author Tobias Achterberg
 * @author Timo Berthold
 * @author Gerald Gamrath
 * @author Robert Lion Gottwald
 * @author Stefan Heinz
 * @author Gregor Hendel
 * @author Thorsten Koch
 * @author Alexander Martin
 * @author Marc Pfetsch
 * @author Michael Winkler
 * @author Kati Wolter
 *
 * @todo check all SCIP_STAGE_* switches, and include the new stages TRANSFORMED and INITSOLVE
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#else
#include <strings.h> /*lint --e{766}*/
#endif


#include "lpi/lpi.h"
#include "nlpi/exprinterpret.h"
#include "nlpi/nlpi.h"
#include "scip/benders.h"
#include "scip/benderscut.h"
#include "scip/branch.h"
#include "scip/branch_nodereopt.h"
#include "scip/clock.h"
#include "scip/compr.h"
#include "scip/concsolver.h"
#include "scip/concurrent.h"
#include "scip/conflict.h"
#include "scip/conflictstore.h"
#include "scip/cons.h"
#include "scip/cons_linear.h"
#include "scip/cutpool.h"
#include "scip/cuts.h"
#include "scip/debug.h"
#include "scip/def.h"
#include "scip/dialog.h"
#include "scip/dialog_default.h"
#include "scip/disp.h"
#include "scip/event.h"
#include "scip/heur.h"
#include "scip/heur_ofins.h"
#include "scip/heur_reoptsols.h"
#include "scip/heur_trivialnegation.h"
#include "scip/heuristics.h"
#include "scip/history.h"
#include "scip/implics.h"
#include "scip/interrupt.h"
#include "scip/lp.h"
#include "scip/mem.h"
#include "scip/message_default.h"
#include "scip/misc.h"
#include "scip/nlp.h"
#include "scip/nodesel.h"
#include "scip/paramset.h"
#include "scip/presol.h"
#include "scip/presolve.h"
#include "scip/pricer.h"
#include "scip/pricestore.h"
#include "scip/primal.h"
#include "scip/prob.h"
#include "scip/prop.h"
#include "scip/reader.h"
#include "scip/relax.h"
#include "scip/reopt.h"
#include "scip/retcode.h"
#include "scip/scipbuildflags.h"
#include "scip/scipcoreplugins.h"
#include "scip/scipgithash.h"
#include "scip/sepa.h"
#include "scip/sepastore.h"
#include "scip/set.h"
#include "scip/sol.h"
#include "scip/solve.h"
#include "scip/stat.h"
#include "scip/syncstore.h"
#include "scip/table.h"
#include "scip/tree.h"
#include "scip/var.h"
#include "scip/visual.h"
#include "xml/xml.h"

#include "scip/scip_reopt.h"
#include "scip/scip_tree.h"

#include "scip/pub_message.h"
#include "scip/pub_reopt.h"
#include "scip/pub_tree.h"


/* In debug mode, we include the SCIP's structure in scip.c, such that no one can access
 * this structure except the interface methods in scip.c.
 * In optimized mode, the structure is included in scip.h, because some of the methods
 * are implemented as defines for performance reasons (e.g. the numerical comparisons)
 */
#ifndef NDEBUG
#include "scip/struct_scip.h"
#endif

/** return the ids of child nodes stored in the reoptimization tree
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 *       - \ref SCIP_STAGE_SOLVING
 *       - \ref SCIP_STAGE_SOLVED
 */
SCIP_RETCODE SCIPgetReoptChildIDs(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_NODE*            node,               /**< node of the search tree */
   unsigned int*         ids,                /**< array of ids */
   int                   idssize,            /**< allocated memory */
   int*                  nids                /**< number of child nodes */
   )
{
   assert(scip != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPgetReoptChildIDs", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE) );

   (*nids) = 0;

   if( !scip->set->reopt_enable )
      return SCIP_OKAY;

   SCIP_CALL( SCIPreoptGetChildIDs(scip->reopt, scip->set, scip->mem->probmem, node, ids, idssize, nids) );

   return SCIP_OKAY;
}

/** return the ids of all leave nodes store in the reoptimization tree induced by the given node
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 *       - \ref SCIP_STAGE_SOLVING
 *       - \ref SCIP_STAGE_SOLVED
 */
SCIP_RETCODE SCIPgetReoptLeaveIDs(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_NODE*            node,               /**< node of the search tree */
   unsigned int*         ids,                /**< array of ids */
   int                   idssize,            /**< size of ids array */
   int*                  nids                /**< number of child nodes */
   )
{
   assert(scip != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPgetReoptLeaveIDs", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE) );

   (*nids) = 0;

   if( idssize == 0 || !scip->set->reopt_enable )
      return SCIP_OKAY;

   SCIP_CALL( SCIPreoptGetLeaves(scip->reopt, node, ids, idssize, nids) );

   return SCIP_OKAY;
}

/** returns the number of nodes in the reoptimization tree induced by @p node; if @p node == NULL the method
 *  returns the number of nodes of the whole reoptimization tree.
 */
int SCIPgetNReoptnodes(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_NODE*            node                /**< node of the search tree */
   )
{
   assert(scip != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   return SCIPreoptGetNNodes(scip->reopt, node);
}

/** returns the number of leaf nodes of the subtree induced by @p node; if @p node == NULL, the method
 *  returns the number of leaf nodes of the whole reoptimization tree.
 */
int SCIPgetNReoptLeaves(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_NODE*            node                /**< node of the search tree */
   )
{
   assert(scip != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   return SCIPreoptGetNLeaves(scip->reopt, node);
}

/** gets the node of the reoptimization tree corresponding to the unique @p id */
SCIP_REOPTNODE* SCIPgetReoptnode(
   SCIP*                 scip,               /**< SCIP data structure */
   unsigned int          id                  /**< unique id */
   )
{
   assert(scip != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   return SCIPreoptGetReoptnode(scip->reopt, id);
}

/** add a variable bound change to a given reoptnode
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 *       - \ref SCIP_STAGE_SOLVING
 *       - \ref SCIP_STAGE_SOLVED
 */
SCIP_RETCODE SCIPaddReoptnodeBndchg(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE*       reoptnode,          /**< node of the reoptimization tree */
   SCIP_VAR*             var,                /**< variable pointer */
   SCIP_Real             bound,              /**< variable bound to add */
   SCIP_BOUNDTYPE        boundtype           /**< bound type of the variable value */
   )
{
   assert(scip != NULL);
   assert(reoptnode != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPaddReoptnodeBndchg", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptnodeAddBndchg(reoptnode, scip->set, scip->mem->probmem, var, bound, boundtype) );

   return SCIP_OKAY;
}

/** set the @p representation as the new search frontier
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 */
SCIP_RETCODE SCIPsetReoptCompression(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE**      representation,     /**< array of representatives */
   int                   nrepresentatives,   /**< number of representatives */
   SCIP_Bool*            success             /**< pointer to store the result */
   )
{
   assert(scip != NULL);
   assert(representation != NULL);
   assert(nrepresentatives > 0);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPsetReoptCompression", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptApplyCompression(scip->reopt, scip->set, scip->mem->probmem, representation, nrepresentatives, success) );

   return SCIP_OKAY;
}

/** add stored constraint to a reoptimization node
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 */
SCIP_RETCODE SCIPaddReoptnodeCons(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE*       reoptnode,          /**< node of the reoptimization tree */
   SCIP_VAR**            vars,               /**< array of variables */
   SCIP_Real*            vals,               /**< array of variable bounds */
   SCIP_BOUNDTYPE*       boundtypes,         /**< array of variable boundtypes */
   SCIP_Real             lhs,                /**< lhs of the constraint */
   SCIP_Real             rhs,                /**< rhs of the constraint */
   int                   nvars,              /**< number of variables */
   REOPT_CONSTYPE        constype,           /**< type of the constraint */
   SCIP_Bool             linear              /**< the given constraint has a linear representation */
   )
{
   assert(scip != NULL);
   assert(reoptnode != NULL);
   assert(vars != NULL);
   assert(vals != NULL);
   assert(nvars >= 0);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPaddReoptnodeCons", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptnodeAddCons(reoptnode, scip->set, scip->mem->probmem, vars, vals, boundtypes, lhs, rhs, nvars,
         constype, linear) );

   return SCIP_OKAY;
}

/** return the branching path stored in the reoptree at ID id */
void SCIPgetReoptnodePath(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE*       reoptnode,          /**< node of the reoptimization tree */
   SCIP_VAR**            vars,               /**< array of variables */
   SCIP_Real*            vals,               /**< array of variable bounds */
   SCIP_BOUNDTYPE*       boundtypes,         /**< array of bound types */
   int                   mem,                /**< allocated memory */
   int*                  nvars,              /**< number of variables */
   int*                  nafterdualvars      /**< number of variables directly after the first based on dual information */
   )
{
   assert(scip != NULL);
   assert(vars != NULL);
   assert(vals != NULL);
   assert(boundtypes != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   SCIPreoptnodeGetPath(scip->reopt, reoptnode, vars, vals, boundtypes, mem, nvars, nafterdualvars);
}

/** initialize a set of empty reoptimization nodes
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 */
SCIP_RETCODE SCIPinitRepresentation(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE**      representatives,    /**< array of representatives */
   int                   nrepresentatives    /**< number of representatives */
   )
{
   int r;

   assert(scip != NULL);
   assert(representatives != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPinitRepresentation", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE) );

   for( r = 0; r < nrepresentatives; r++ )
   {
      SCIP_CALL( SCIPallocBlockMemory(scip, &representatives[r]) ); /*lint !e866*/
      SCIPreoptnodeInit(representatives[r], scip->set);
   }

   return SCIP_OKAY;
}

/** reset a set of initialized reoptimization nodes
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 */
SCIP_RETCODE SCIPresetRepresentation(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE**      representatives,    /**< array of representatives */
   int                   nrepresentatives    /**< number of representatives */
   )
{
   int r;

   assert(scip != NULL);
   assert(representatives != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPresetRepresentation", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE) );

   for( r = 0; r < nrepresentatives; r++ )
   {
      SCIP_CALL( SCIPreoptnodeReset(scip->reopt, scip->set, scip->mem->probmem, representatives[r]) );
   }

   return SCIP_OKAY;
}

/** free a set of initialized reoptimization nodes
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 */
SCIP_RETCODE SCIPfreeRepresentation(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE**      representatives,    /**< array of representatives */
   int                   nrepresentatives    /**< number of representatives */
   )
{
   int r;

   assert(scip != NULL);
   assert(representatives != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPfreeRepresentation", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE) );

   for( r = 0; r < nrepresentatives; r++ )
   {
      if( representatives[r] != NULL )
      {
         SCIP_CALL( SCIPreoptnodeDelete(&representatives[r], scip->mem->probmem) );
         assert(representatives[r] == NULL);
      }
   }

   return SCIP_OKAY;
}

/** reactivate the given @p reoptnode and split them into several nodes if necessary
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_SOLVING
 *       - \ref SCIP_STAGE_SOLVED
 */
SCIP_RETCODE SCIPapplyReopt(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE*       reoptnode,          /**< node to reactivate */
   unsigned int          id,                 /**< unique id of the reoptimization node */
   SCIP_Real             estimate,           /**< estimate of the child nodes that should be created */
   SCIP_NODE**           childnodes,         /**< array to store the created child nodes */
   int*                  ncreatedchilds,     /**< pointer to store number of created child nodes */
   int*                  naddedconss,        /**< pointer to store number of generated constraints */
   int                   childnodessize,     /**< available size of childnodes array */
   SCIP_Bool*            success             /**< pointer store the result*/
   )
{
   assert(scip != NULL);
   assert(reoptnode != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPapplyReopt", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptApply(scip->reopt, scip, scip->set, scip->stat, scip->transprob, scip->origprob, scip->tree,
         scip->lp, scip->branchcand, scip->eventqueue, scip->cliquetable, scip->mem->probmem, reoptnode, id, estimate,
         childnodes, ncreatedchilds, naddedconss, childnodessize, success) );

   return SCIP_OKAY;
}

/** return the similarity between two objective functions */
SCIP_Real SCIPgetReoptSimilarity(
   SCIP*                 scip,               /**< SCIP data structure */
   int                   run1,               /**< number of run */
   int                   run2                /**< number of run */
   )
{
   assert(scip != NULL);
   assert(run1 > 0 && run1 <= scip->stat->nreoptruns);
   assert(run2 > 0 && run2 <= scip->stat->nreoptruns);

   if( (run1 == scip->stat->nreoptruns && run2 == run1-1) || (run2 == scip->stat->nreoptruns && run1 == run2-1) )
      return SCIPreoptGetSimToPrevious(scip->reopt);
   else
      return SCIPreoptGetSimilarity(scip->reopt, scip->set, run1, run2, scip->origprob->vars, scip->origprob->nvars);
}

/** returns if a node should be reoptimized */
SCIP_Bool SCIPreoptimizeNode(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_NODE*            node                /**< node of the search tree */
   )
{
   assert(scip != NULL);
   assert(node != NULL);

   if( scip->set->reopt_enable )
   {
      SCIP_REOPTNODE* reoptnode;
      unsigned int id;

      assert(scip->reopt != NULL);

      id = SCIPnodeGetReoptID(node);

      if( id == 0 && node != SCIPgetRootNode(scip) )
         return FALSE;
      else
      {
         reoptnode = SCIPgetReoptnode(scip, id);
         assert(reoptnode != NULL);

         return SCIPreoptnodeGetNChildren(reoptnode) > 0;
      }
   }
   else
      return FALSE;
}

/** deletes the given reoptimization node
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_TRANSFORMED
 *       - \ref SCIP_STAGE_SOLVING
 */
SCIP_RETCODE SCIPdeleteReoptnode(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_REOPTNODE**      reoptnode           /**< node of the reoptimization tree */
   )
{
   assert(scip != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);
   assert((*reoptnode) != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPdeleteReoptnode", FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptnodeDelete(reoptnode, scip->mem->probmem) );

   return SCIP_OKAY;
}

/** splits the root into several nodes and moves the child nodes of the root to one of the created nodes
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_SOLVING
 */
SCIP_RETCODE SCIPsplitReoptRoot(
   SCIP*                 scip,               /**< SCIP data structure */
   int*                  ncreatedchilds,     /**< pointer to store the number of created nodes */
   int*                  naddedconss         /**< pointer to store the number added constraints */
   )
{
   assert(scip != NULL);
   assert(scip->set->reopt_enable);
   assert(scip->reopt != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPsplitReoptRoot", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptSplitRoot(scip->reopt, scip->tree, scip->set, scip->stat, scip->mem->probmem, ncreatedchilds,
         naddedconss) );

   return SCIP_OKAY;
}

/** remove the stored information about bound changes based in dual information
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_SOLVING
 *       - \ref SCIP_STAGE_SOLVED
 */
SCIP_RETCODE SCIPresetReoptnodeDualcons(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_NODE*            node                /**< node of the search tree */
   )
{
   assert(scip != NULL);
   assert(scip->set->reopt_enable);
   assert(node != NULL);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPresetReoptnodeDualcons", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, FALSE) );

   SCIP_CALL( SCIPreoptResetDualBndchgs(scip->reopt, node, scip->mem->probmem) );

   return SCIP_OKAY;
}
