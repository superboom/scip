/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2016 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   struct_cons_expr.h
 * @brief  (public) data structures of expression constraints
 * @author Stefan Vigerske
 *
 * These are in particular data structures to manage the expressions in cons_expr
 * and that need to be accessed by the linear estimation plugins of cons_expr.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_STRUCT_CONS_EXPR_H__
#define __SCIP_STRUCT_CONS_EXPR_H__

#include "scip/type_cons_expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** generic data and callback methods of an expression handler */
struct SCIP_ConsExpr_ExprHdlr
{
   char*                          name;      /**< expression handler name */
   char*                          desc;      /**< expression handler description (can be NULL) */
   SCIP_CONSEXPR_EXPRHDLRDATA*    data;      /**< data of handler */

   SCIP_DECL_CONSEXPR_EXPRCOPYHDLR((*copyhdlr));  /**< handler copy callback (can be NULL) */
   SCIP_DECL_CONSEXPR_EXPRFREEHDLR((*freehdlr));  /**< handler free callback (can be NULL) */
   SCIP_DECL_CONSEXPR_EXPRCOPYDATA((*copydata));  /**< data copy callback, or NULL for expressions that have no data */
   SCIP_DECL_CONSEXPR_EXPRFREEDATA((*freedata));  /**< data free callback, or NULL for expressions that have no data or which data does not need to be freed */
   SCIP_DECL_CONSEXPR_EXPRPRINT((*print));        /**< print callback (can be NULL) */
};

/** union for storing one, two, or many children */
union SCIP_ConsExpr_Children
{
   SCIP_CONSEXPR_EXPR*   single;             /**< child expression of a univariate expression */
   SCIP_CONSEXPR_EXPR*   pair[2];            /**< children of a bivariate expression */
   struct
   {
      int                  nchildren;        /**< number of children of a multivariate expression */
      int                  childrensize;     /**< length of children array */
      SCIP_CONSEXPR_EXPR** children;         /**< children expressions of multivariate expression */
   } array;
};


/** a node in the expression graph that is handled by the expression constraint handler */
struct SCIP_ConsExpr_Expr
{
   SCIP_CONSEXPR_EXPRHDLR*    exprhdlr;      /**< expression type (as pointer to its handler) */
   SCIP_CONSEXPR_EXPRDATA*    exprdata;      /**< expression data */

   SCIP_CONSEXPR_VARIABILITY  variability;   /**< variability of expression (in-, uni-, bi-, multivariate) */
   SCIP_CONSEXPR_CHILDREN     children;      /**< children of expression, interpretation of union depends on variability */

   int                        nuses;         /**< reference counter */
};




#ifdef __cplusplus
}
#endif

#endif /* __SCIP_STRUCT_CONS_EXPR_H__ */
