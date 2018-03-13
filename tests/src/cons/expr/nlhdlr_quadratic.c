/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2013 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   nlhdlr_quadratic.c
 * @brief  tests quadratic nonlinear handler methods
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <string.h>

/* XXX: need the consdata struct because we don't have getNlhdlrs or findNlhdlrs; I don't add those function because I'm unsure
 * we actually need them
 */
#define SCIP_PRIVATE_ROWPREP
#include "scip/cons_expr.c"
#include "scip/cons_expr_nlhdlr_quadratic.c"


/*
 * TEST
 */

#include "include/scip_test.h"

static SCIP* scip;
static SCIP_VAR* x;
static SCIP_VAR* y;
static SCIP_VAR* w;
static SCIP_VAR* z;

static SCIP_CONSHDLR* conshdlr;
static SCIP_CONSEXPR_NLHDLR* nlhdlr = NULL;

/* creates scip, problem, includes expression constraint handler, creates and adds variables */
static
void setup(void)
{
   int h;
   SCIP_CONSHDLRDATA* conshdlrdata;

   SCIP_CALL( SCIPcreate(&scip) );

   /* include cons_expr: this adds the operator handlers and nonlinear handlers; get quadratic handler and conshdlr */
   SCIP_CALL( SCIPincludeConshdlrExpr(scip) );

   conshdlr = SCIPfindConshdlr(scip, "expr");
   cr_assert_not_null(conshdlr);
   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   cr_assert_not_null(conshdlrdata);

   /* get nlhdlr */
   for( h = 0; h < conshdlrdata->nnlhdlrs; ++h )
      if( strcmp(SCIPgetConsExprNlhdlrName(conshdlrdata->nlhdlrs[h]), "quadratic") == 0 )
      {
         nlhdlr = conshdlrdata->nlhdlrs[h];
         break;
      }
   cr_assert_not_null(nlhdlr);


   /* create problem */
   SCIP_CALL( SCIPcreateProbBasic(scip, "test_problem") );

   /* go to PRESOLVING stage */
   SCIP_CALL( TESTscipSetStage(scip, SCIP_STAGE_PRESOLVING, TRUE) );

   SCIP_CALL( SCIPcreateVarBasic(scip, &x, "x", -1.0, 1.0, 0.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &y, "y", -1.0, 1.0, 0.0, SCIP_VARTYPE_INTEGER) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &w, "w", -1.0, 1.0, 0.0, SCIP_VARTYPE_INTEGER) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &z, "z", -1.0, 1.0, 0.0, SCIP_VARTYPE_INTEGER) );
   SCIP_CALL( SCIPaddVar(scip, x) );
   SCIP_CALL( SCIPaddVar(scip, y) );
   SCIP_CALL( SCIPaddVar(scip, w) );
   SCIP_CALL( SCIPaddVar(scip, z) );
}

/* releases variables, frees scip */
static
void teardown(void)
{
   SCIP_CALL( SCIPreleaseVar(scip, &x) );
   SCIP_CALL( SCIPreleaseVar(scip, &y) );
   SCIP_CALL( SCIPreleaseVar(scip, &w) );
   SCIP_CALL( SCIPreleaseVar(scip, &z) );
   SCIP_CALL( SCIPfree(&scip) );

   BMSdisplayMemory();
   //BMScheckEmptyMemory();
   cr_assert_eq(BMSgetMemoryUsed(), 0, "Memory is leaking!!");
}

/* detects x^2 + x as quadratic expression */
Test(nlhdlrquadratic, detectandfree1, .init = setup, .fini = teardown)
{
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata = NULL;
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* simplified;
   SCIP_CONSEXPR_EXPRENFO_METHOD provided;
   SCIP_CONSEXPR_EXPRENFO_METHOD providedexpected;
   SCIP_Bool enforcebelow;
   SCIP_Bool enforceabove;
   SCIP_Bool success;
   SCIP_VAR* var;

   /* create expression and simplify it: note it fails if not simplified, the order matters! */
   SCIP_CALL( SCIPparseConsExprExpr(scip, conshdlr, (char*)"<x>^2 + <x>", NULL, &expr) );
   SCIP_CALL( SCIPsimplifyConsExprExpr(scip, expr, &simplified) );
   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
   expr = simplified;

   /* detect */
   provided = SCIP_CONSEXPR_EXPRENFO_NONE;
   enforcebelow = FALSE;
   enforceabove = FALSE;
   success = FALSE;
   SCIP_CALL( detectHdlrQuadratic(scip, conshdlr, nlhdlr, expr, &provided, &enforcebelow, &enforceabove, &success, &nlhdlrexprdata) );
   providedexpected = SCIP_CONSEXPR_EXPRENFO_SEPABELOW | SCIP_CONSEXPR_EXPRENFO_INTEVAL | SCIP_CONSEXPR_EXPRENFO_REVERSEPROP;
   cr_expect_eq(provided, providedexpected, "expecting %d got %d\n", providedexpected, provided);
   cr_assert(enforcebelow);
   cr_assert(!enforceabove);
   cr_assert(success);
   cr_assert_not_null(nlhdlrexprdata);

   cr_expect_eq(nlhdlrexprdata->nlinexprs, 0, "Expecting 0 linear expr, got %d\n", nlhdlrexprdata->nlinexprs);
   cr_expect_eq(nlhdlrexprdata->nquadexprs, 1, "Expecting 1 quadratic terms, got %d\n", nlhdlrexprdata->nquadexprs);
   cr_expect_eq(nlhdlrexprdata->nbilinexprterms, 0, "Expecting 0 bilinear terms, got %d\n", nlhdlrexprdata->nbilinexprterms);

   SCIP_QUADEXPRTERM quad;
   quad = nlhdlrexprdata->quadexprterms[0];
   cr_assert_not_null(quad.expr);
   var = SCIPgetConsExprExprAuxVar(quad.expr);
   fprintf(stderr, "x = %s, quad.expr's auxvar %s\n", SCIPvarGetName(x), SCIPvarGetName(var));
   cr_expect_eq(var, x, "Expecting var %s in quad term, got %s\n", SCIPvarGetName(x), SCIPvarGetName(var));
   cr_expect_eq(1.0, quad.lincoef, "Expecting lincoef %g in quad term, got %g\n", 1.0, quad.lincoef);
   cr_expect_eq(1.0, quad.sqrcoef, "Expecting sqrcoef %g in quad term, got %g\n", 1.0, quad.sqrcoef);

   /* register enforcer info in expr and free */
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &(expr->enfos), 1) );
   SCIP_CALL( SCIPallocBlockMemory(scip, &(expr->enfos[0])) );
   expr->enfos[0]->nlhdlr = nlhdlr;
   expr->enfos[0]->nlhdlrexprdata = nlhdlrexprdata;
   expr->nenfos = 1;
   expr->enfos[0]->issepainit = FALSE;

   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
}

/* detects x^2 + 2*x exp(y x^2) + exp(y x^2)^2 <= 1 as convex quadratic expression:
 * simplify yields x^2 + 2 * x exp(x^2 y) + exp(x^2 y)^2 <= 1 --> should detect x^2 + 2 x * w + w^2
 */
Test(nlhdlrquadratic, detectandfree2, .init = setup, .fini = teardown)
{
   SCIP_CONSEXPR_EXPRENFO_METHOD provided;
   SCIP_CONSEXPR_EXPRENFO_METHOD providedexpected;
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata = NULL;
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* expexpr;
   SCIP_CONS* cons;
   SCIP_Bool enforcebelow;
   SCIP_Bool enforceabove;
   SCIP_Bool success;

   /* create expression, simplify it and find common subexpressions*/
   success = FALSE;
   SCIP_CALL( SCIPparseCons(scip, &cons, (char*)"[expr] <test>: <x>^2 + 2 * <x> * exp(<y> * <x>^2) + exp(<y> * <x>^2)^2 <= 1", TRUE, TRUE,
            TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, &success) );
   cr_assert(success);

   success = FALSE;
   SCIP_CALL( canonicalizeConstraints(scip, conshdlr, &cons, 1) );

   /* get expr and work with it */
   expr = SCIPgetExprConsExpr(scip, cons);

   /* get exponential expression */
   cr_assert_eq(SCIPgetConsExprExprNChildren(expr), 3);
   expexpr = SCIPgetConsExprExprChildren(expr)[1]; /*  x * exp(x^2 y) */
   expexpr = SCIPgetConsExprExprChildren(expexpr)[1]; /* exp(x^2 y) */
   cr_assert_str_eq(SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expexpr)), "exp", "expecting exp got %s\n",
         SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expexpr)));
   /* detect */
   provided = SCIP_CONSEXPR_EXPRENFO_NONE;
   enforcebelow = FALSE;
   enforceabove = FALSE;
   success = FALSE;
   SCIP_CALL( detectHdlrQuadratic(scip, conshdlr, nlhdlr, expr, &provided, &enforcebelow, &enforceabove, &success, &nlhdlrexprdata) );
   providedexpected = SCIP_CONSEXPR_EXPRENFO_SEPABELOW | SCIP_CONSEXPR_EXPRENFO_INTEVAL | SCIP_CONSEXPR_EXPRENFO_REVERSEPROP;
   cr_expect_eq(provided, providedexpected, "expecting %d got %d\n", providedexpected, provided);
   cr_assert(enforcebelow);
   cr_assert(!enforceabove);
   cr_assert(success);
   cr_assert_not_null(nlhdlrexprdata);

   cr_expect_eq(nlhdlrexprdata->nlinexprs, 0, "Expecting 0 linear vars, got %d\n", nlhdlrexprdata->nlinexprs);
   cr_expect_eq(nlhdlrexprdata->nquadexprs, 2, "Expecting 2 quadratic terms, got %d\n", nlhdlrexprdata->nquadexprs);
   cr_expect_eq(nlhdlrexprdata->nbilinexprterms, 1, "Expecting 1 bilinear terms, got %d\n", nlhdlrexprdata->nbilinexprterms);

   /* x var */
   SCIP_QUADEXPRTERM quad;
   quad = nlhdlrexprdata->quadexprterms[0];
   cr_assert_not_null(quad.expr);
   cr_expect_eq(x, SCIPgetConsExprExprAuxVar(quad.expr), "Expecting var %s in quad term, got %s\n",
         SCIPvarGetName(x), SCIPvarGetName(SCIPgetConsExprExprAuxVar(quad.expr)));
   cr_expect_eq(0.0, quad.lincoef, "Expecting lincoef %g in quad term, got %g\n", 0.0, quad.lincoef);
   cr_expect_eq(1.0, quad.sqrcoef, "Expecting sqrcoef %g in quad term, got %g\n", 1.0, quad.sqrcoef);

   /* expr exp(x^2 y) is quadratic */
   quad = nlhdlrexprdata->quadexprterms[1];
   cr_assert_not_null(quad.expr);
   cr_expect_eq(expexpr, quad.expr);
   cr_expect_eq(0.0, quad.lincoef, "Expecting lincoef %g in quad term, got %g\n", 0.0, quad.lincoef);
   cr_expect_eq(1.0, quad.sqrcoef, "Expecting sqrcoef %g in quad term, got %g\n", 0.0, quad.sqrcoef);
   cr_expect_not_null(SCIPgetConsExprExprAuxVar(quad.expr), "exp expr should have auxiliary variable!\n");


   SCIP_BILINEXPRTERM bilin;
   bilin = nlhdlrexprdata->bilinexprterms[0];
   cr_assert_not_null(bilin.expr1);
   cr_assert_not_null(bilin.expr2);
   cr_expect_eq(SCIPgetConsExprExprAuxVar(bilin.expr1), x, "Expecting expr's auxvar %s in bilin term, got %s\n",
         SCIPvarGetName(x), SCIPvarGetName(SCIPgetConsExprExprAuxVar(bilin.expr1)));
   cr_expect_eq(bilin.expr2, expexpr);
   cr_expect_eq(2.0, bilin.coef, "Expecting bilinear coef of %g, got %g\n", 2.0, bilin.coef);

   /* free auxvar(s) created by detect from above */
   SCIP_CALL( freeAuxVars(scip, conshdlr, &cons, 1) );

   /* register nlhdlr info in expr and free */
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &(expr->enfos), 1) );
   SCIP_CALL( SCIPallocBlockMemory(scip, &(expr->enfos[0])) );
   expr->enfos[0]->nlhdlr = nlhdlr;
   expr->enfos[0]->nlhdlrexprdata = nlhdlrexprdata;
   expr->nenfos = 1;
   expr->enfos[0]->issepainit = FALSE;

   /* if there is an nlhdlr, then there must also be an auxvar */
   SCIP_CALL( SCIPcreateConsExprExprAuxVar(scip, conshdlr, expr, NULL) );

   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );
}

/* properly detect quadratic expression in exp(abs(log(x^2 + 2 * x*y + y^2))) <= 1 */
Test(nlhdlrquadratic, detectandfree3, .init = setup, .fini = teardown)
{
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONS* cons;
   SCIP_Bool success;
   SCIP_Bool infeasible;

   /* create expression and simplify it */
   success = FALSE;
   SCIP_CALL( SCIPparseCons(scip, &cons, (char*)"[expr] <test>: exp(abs(log(<x>^2 + 2 * <x> * <y> + <y> + 2 * <y>^2))) <= 1", TRUE, TRUE,
            TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, &success) );
   cr_assert(success);

   SCIP_CALL( canonicalizeConstraints(scip, conshdlr, &cons, 1) );

   /* call detection method -> this registers the nlhdlr */
   SCIP_CALL( detectNlhdlrs(scip, conshdlr, &cons, 1, &infeasible) );
   cr_assert_not(infeasible);

   /* get expr and work with it */
   expr = SCIPgetExprConsExpr(scip, cons);

   /* expr is exponential expr */
   cr_assert_eq(SCIPgetConsExprExprNChildren(expr), 1);
   cr_assert_str_eq(SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)), "exp", "expecting exp got %s\n",
         SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)));
   cr_assert_not_null(expr->auxvar);

   /* expr is abs expr */
   expr = SCIPgetConsExprExprChildren(expr)[0];
   cr_assert_eq(SCIPgetConsExprExprNChildren(expr), 1);
   cr_assert_str_eq(SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)), "abs", "expecting abs got %s\n",
         SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)));
   cr_assert_not_null(expr->auxvar);

   /* expr is log expr */
   expr = SCIPgetConsExprExprChildren(expr)[0];
   cr_assert_eq(SCIPgetConsExprExprNChildren(expr), 1);
   cr_assert_str_eq(SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)), "log", "expecting log got %s\n",
         SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)));
   cr_assert_not_null(expr->auxvar);

   /* expr is sum expr */
   expr = SCIPgetConsExprExprChildren(expr)[0];
   cr_assert_eq(SCIPgetConsExprExprNChildren(expr), 4);
   cr_assert_str_eq(SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)), "sum", "expecting sum got %s\n",
         SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr)));
   cr_assert_not_null(expr->auxvar);

   /* TODO: I guess with the proper locks it should be identified that child should have aux vars */
#if 0
   for( int i = 0; i < SCIPgetConsExprExprNChildren(expr); ++i )
   {
      SCIP_CONSEXPR_EXPR* child = SCIPgetConsExprExprChildren(expr)[i];
      cr_expect_null(child->auxvar);
   }
#endif

   /* quadratic terms */
   SCIP_QUADEXPRTERM quad;
   cr_expect_eq(2, expr->enfos[0]->nlhdlrexprdata->nquadexprs);

   /* x var */
   quad = expr->enfos[0]->nlhdlrexprdata->quadexprterms[0];
   cr_assert_not_null(quad.expr);
   cr_expect_eq(x, SCIPgetConsExprExprAuxVar(quad.expr), "Expecting expr auxvar %s in quad term, got %s\n",
         SCIPvarGetName(x), SCIPvarGetName(SCIPgetConsExprExprAuxVar(quad.expr)));
   cr_expect_eq(0.0, quad.lincoef, "Expecting lincoef %g in quad term, got %g\n", 0.0, quad.lincoef);
   cr_expect_eq(1.0, quad.sqrcoef, "Expecting sqrcoef %g in quad term, got %g\n", 1.0, quad.sqrcoef);

   /* y var */
   quad = expr->enfos[0]->nlhdlrexprdata->quadexprterms[1];
   cr_assert_not_null(quad.expr);
   cr_expect_eq(y, SCIPgetConsExprExprAuxVar(quad.expr), "Expecting expr auxvar %s in quad term, got %s\n",
         SCIPvarGetName(y), SCIPvarGetName(SCIPgetConsExprExprAuxVar(quad.expr)));
   cr_expect_eq(1.0, quad.lincoef, "Expecting lincoef %g in quad term, got %g\n", 0.0, quad.lincoef);
   cr_expect_eq(2.0, quad.sqrcoef, "Expecting sqrcoef %g in quad term, got %g\n", 1.0, quad.sqrcoef);

   /* bilinear term */
   SCIP_BILINEXPRTERM bilin;
   cr_expect_eq(1, expr->enfos[0]->nlhdlrexprdata->nbilinexprterms);
   bilin = expr->enfos[0]->nlhdlrexprdata->bilinexprterms[0];
   cr_assert_not_null(bilin.expr1);
   cr_assert_not_null(bilin.expr2);
   cr_expect_eq(2.0, bilin.coef, "Expecting bilincoef %g in quad term, got %g\n", 2.0, bilin.coef);
   cr_expect_eq(SCIPgetConsExprExprAuxVar(bilin.expr1), x);
   cr_expect_eq(SCIPgetConsExprExprAuxVar(bilin.expr2), y);

   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );
}

/* x^2 + y^2 + w*z should not be handled by this nlhandler */
Test(nlhdlrquadratic, noproperquadratic1, .init = setup, .fini = teardown)
{
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata = NULL;
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* simplified;
   SCIP_CONSEXPR_EXPRENFO_METHOD provided;
   SCIP_Bool enforcebelow;
   SCIP_Bool enforceabove;
   SCIP_Bool success;

   /* create expression and simplify it: note it fails if not simplified, the order matters! */
   SCIP_CALL( SCIPparseConsExprExpr(scip, conshdlr, (char*)"<x>^2 + <y>^2 + <w>*<z>", NULL, &expr) );
   SCIP_CALL( SCIPsimplifyConsExprExpr(scip, expr, &simplified) );
   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
   expr = simplified;

   /* detect */
   provided = SCIP_CONSEXPR_EXPRENFO_NONE;
   enforcebelow = FALSE;
   enforceabove = FALSE;
   success = FALSE;
   SCIP_CALL( detectHdlrQuadratic(scip, conshdlr, nlhdlr, expr, &provided, &enforcebelow, &enforceabove, &success, &nlhdlrexprdata) );

   /* shouldn't have detected anything -> provides nothing */
   cr_expect_eq(provided, SCIP_CONSEXPR_EXPRENFO_NONE);
   cr_assert(!enforcebelow);
   cr_assert(!enforceabove);
   cr_assert(!success);
   cr_expect_null(nlhdlrexprdata);

   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
}

/* log^2 x + sin^2 y + cos^2 z should not be handled by this nlhandler */
Test(nlhdlrquadratic, noproperquadratic2, .init = setup, .fini = teardown)
{
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata = NULL;
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* simplified;
   SCIP_CONSEXPR_EXPRENFO_METHOD provided;
   SCIP_Bool enforcebelow;
   SCIP_Bool enforceabove;
   SCIP_Bool success;

   /* create expression and simplify it: note it fails if not simplified, the order matters! */
   SCIP_CALL( SCIPparseConsExprExpr(scip, conshdlr, (char*)"log(<x>)^2 + sin(<y>)^2 + cos(<z>)^2", NULL, &expr) );
   SCIP_CALL( SCIPsimplifyConsExprExpr(scip, expr, &simplified) );
   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
   expr = simplified;

   /* detect */
   provided = SCIP_CONSEXPR_EXPRENFO_NONE;
   enforcebelow = FALSE;
   enforceabove = FALSE;
   success = FALSE;
   SCIP_CALL( detectHdlrQuadratic(scip, conshdlr, nlhdlr, expr, &provided, &enforcebelow, &enforceabove, &success, &nlhdlrexprdata) );

   /* shouldn't have detected anything -> provides nothing */
   cr_expect_eq(provided, SCIP_CONSEXPR_EXPRENFO_NONE);
   cr_assert(!enforcebelow);
   cr_assert(!enforceabove);
   cr_assert(!success);
   cr_expect_null(nlhdlrexprdata);

   /* no auxiliary variables */
   cr_expect_eq(3, SCIPgetConsExprExprNChildren(expr));
   for( int i = 0; i < SCIPgetConsExprExprNChildren(expr); i++ )
      cr_expect_null(SCIPgetConsExprExprAuxVar(SCIPgetConsExprExprChildren(expr)[i]));

   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
}

/* x^2 + y^2 + z^2 * x, should only provide propagation:
 * Note: we use this expression because variables are automatically detected to be
 * common subexpressions. Since we cannot call detect common subexpression to a given expression
 * as easily as calling simplify, we content with this work around
 * The alternative would be to create a constraint and canonilize it, then get the expression
 * and call the detection method of the quadratic to this expression. This is the cleanest way
 * and probably the way it should be done (TODO)
 */
Test(nlhdlrquadratic, onlyPropagation, .init = setup, .fini = teardown)
{
   SCIP_CONSEXPR_NLHDLREXPRDATA* nlhdlrexprdata = NULL;
   SCIP_CONSEXPR_EXPR* expr;
   SCIP_CONSEXPR_EXPR* simplified;
   SCIP_CONSEXPR_EXPRENFO_METHOD provided;
   SCIP_Bool enforcebelow;
   SCIP_Bool enforceabove;
   SCIP_Bool success;

   /* create expression and simplify it: note it fails if not simplified, the order matters! */
   SCIP_CALL( SCIPparseConsExprExpr(scip, conshdlr, (char*)"<x>^2 + <y>^2 + <z>^2 * <x>", NULL, &expr) );
   SCIP_CALL( SCIPprintConsExprExpr(scip, expr, NULL) );
   SCIPinfoMessage(scip, NULL, "\n");
   SCIP_CALL( SCIPsimplifyConsExprExpr(scip, expr, &simplified) );
   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
   expr = simplified;
   SCIP_CALL( SCIPprintConsExprExpr(scip, expr, NULL) );
   SCIPinfoMessage(scip, NULL, "\n");

   /* detect */
   provided = SCIP_CONSEXPR_EXPRENFO_NONE;
   enforcebelow = FALSE;
   enforceabove = FALSE;
   success = FALSE;
   SCIP_CALL( detectHdlrQuadratic(scip, conshdlr, nlhdlr, expr, &provided, &enforcebelow, &enforceabove, &success, &nlhdlrexprdata) );

   cr_expect_eq(provided, SCIP_CONSEXPR_EXPRENFO_INTEVAL | SCIP_CONSEXPR_EXPRENFO_REVERSEPROP, "got %d\n", provided);
   cr_assert(!enforcebelow);
   cr_assert(!enforceabove);
   cr_assert(success);
   cr_expect_not_null(nlhdlrexprdata);

   /* no auxiliary variables should have been created */
   cr_expect_eq(4, SCIPgetNVars(scip), "got %d\n", SCIPgetNVars(scip));

   /* register enforcer info in expr and free */
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &(expr->enfos), 1) );
   SCIP_CALL( SCIPallocBlockMemory(scip, &(expr->enfos[0])) );
   expr->enfos[0]->nlhdlr = nlhdlr;
   expr->enfos[0]->nlhdlrexprdata = nlhdlrexprdata;
   expr->nenfos = 1;
   expr->enfos[0]->issepainit = FALSE;

   SCIP_CALL( SCIPreleaseConsExprExpr(scip, &expr) );
}
