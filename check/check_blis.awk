#!/usr/bin/env awk -f
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
#*                                                                           *
#*                  This file is part of the program and library             *
#*         SCIP --- Solving Constraint Integer Programs                      *
#*                                                                           *
#*    Copyright (C) 2002-2009 Konrad-Zuse-Zentrum                            *
#*                            fuer Informationstechnik Berlin                *
#*                                                                           *
#*  SCIP is distributed under the terms of the ZIB Academic License.         *
#*                                                                           *
#*  You should have received a copy of the ZIB Academic License              *
#*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      *
#*                                                                           *
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# $Id: check_blis.awk,v 1.1 2009/12/04 15:51:37 bzfwanie Exp $
#
#@file    check_blis.awk
#@brief   BLIS Check Report Generator
#@author  Thorsten Koch
#@author  Tobias Achterberg
#@author  Robert Waniek
#
function abs(x)
{
    return x < 0 ? -x : x;
}
function max(x,y)
{
    return (x) > (y) ? (x) : (y);
}
BEGIN {
   timegeomshift = 60.0;
   nodegeomshift = 1000.0;
   onlyinsolufile = 0;  # should only instances be reported that are included in the .solu file?
   useshortnames = 1;   # should problem name be truncated to fit into column?
   infty = +1e+20;

   printf("\\documentclass[leqno]{article}\n")                      >TEXFILE;
   printf("\\usepackage{a4wide}\n")                                 >TEXFILE;
   printf("\\usepackage{amsmath,amsfonts,amssymb,booktabs}\n")      >TEXFILE;
   printf("\\pagestyle{empty}\n\n")                                 >TEXFILE;
   printf("\\begin{document}\n\n")                                  >TEXFILE;
   printf("\\begin{table}[p]\n")                                    >TEXFILE;
   printf("\\begin{center}\n")                                      >TEXFILE;
   printf("\\setlength{\\tabcolsep}{2pt}\n")                        >TEXFILE;
   printf("\\newcommand{\\g}{\\raisebox{0.25ex}{\\tiny $>$}}\n")    >TEXFILE;
   printf("\\begin{tabular*}{\\textwidth}{@{\\extracolsep{\\fill}}lrrrrrrrrrrrr@{}}\n") >TEXFILE;
   printf("\\toprule\n")                                            >TEXFILE;
   printf("Name                &  Conss &   Vars &     Dual Bound &   Primal Bound &  Gap\\% &     Nodes &     Time \\\\\n") > TEXFILE;
   printf("\\midrule\n")                                            >TEXFILE;

   printf("------------------+-------+------+----------------+----------------+------+---------+--------+-------+-------\n");
   printf("Name              | Conss | Vars |   Dual Bound   |  Primal Bound  | Gap% |   Iters |  Nodes |  Time |       \n");
   printf("------------------+-------+------+----------------+----------------+------+---------+--------+-------+-------\n");

   nprobs   = 0;
   sbab     = 0;
   scut     = 0;
   stottime = 0.0;
   sgap     = 0.0;
   nodegeom = 0.0;
   timegeom = 0.0;
   shiftednodegeom = nodegeomshift;
   shiftedtimegeom = timegeomshift;
   failtime = 0.0;
   timeouttime = 0.0;
   fail     = 0;
   pass     = 0;
   timeouts = 0;
   settings = "default";
   version = "?";
}
/=opt=/  { solstatus[$2] = "opt"; sol[$2] = $3; }  # get optimum
/=inf=/  { solstatus[$2] = "inf"; sol[$2] = 0.0; } # problem infeasible
/=best=/ { solstatus[$2] = "best"; sol[$2] = $3; } # get best known solution value
/=unkn=/ { solstatus[$2] = "unkn"; }               # no feasible solution known
#
# problem name
#
/^@01/ { 
   n  = split ($2, a, "/");
   m = split(a[n], b, ".");
   prob = b[1];
   if( b[m] == "gz" || b[m] == "z" || b[m] == "GZ" || b[m] == "Z" )
      m--;
   for( i = 2; i < m; ++i )
      prob = prob "." b[i];

   if( useshortnames && length(prob) > 18 )
      shortprob = substr(prob, length(prob)-17, 18);
   else
      shortprob = prob;

   # Escape _ for TeX
   n = split(prob, a, "_");
   pprob = a[1];
   for( i = 2; i <= n; i++ )
      pprob = pprob "\\_" a[i];
   vars       = 0;
   cons       = 0;
   timeout    = 0;
   opti       = 0;
   feasible   = 1;
   cuts       = 0;
   pb         = +infty;
   db         = -infty;
   mipgap     = 1e-4;
   absmipgap  = 1e-6;
   bbnodes    = 0;
   iters      = 0;
   primlps    = 0;
   primiter   = 0;
   duallps    = 0;
   dualiter   = 0;
   sblps      = 0;
   sbiter     = 0;
   tottime    = 0.0;
   aborted    = 1;
   logging    = 0;
}
/^Blis0001I BLIS version/ {version = $4;}
#
# problem size
#
/^Coin0002I Problem/ {
   cons = $5;
   vars = $7;
}
#
# solving process
#
/^Clp0000I Optimal - objective value/ { logging = 1; }
/%/ {
   if ( $4 != "gap" )
   {
      db = $4;
      logging = 0;
   }
}
/ / {
   if ( logging == 1 )
   {
      db = $3;
   }
}
/^Alps/ { logging = 0; }
#
# solution
#
/^Alps0230I Reached time limit./ { 
   aborted = 0;
   timeout = 1;
   bbnodes = $5;
}
/^Alps0208I Found an optimal solution./ {
   aborted = 0;
   bbnodes = $6;
   opti = 1;
}
/^Alps0202I Problem is infeasible./ {
   db = +infty;
   pb = +infty;
   absgap = 0.0;
   feasible = 0;
   aborted = 0;
   bbnodes = $5;
}
/Alps0260I Quality of the best solution found: / { pb = $8; }
/Alps0264I No solution found/ { 
   if ( feasible == 1 ) 
      pb = +infty; 
}
/Blis0057I Relative optimality gap is infinity because no solution was found/ { 
   if ( feasible == 1 ) 
      pb = +infty; 
}
/ALPS did not find a solution./ { 
   if ( feasible == 1 ) 
      pb = +infty; 
}
/Alps0267I Number of nodes processed: / { bbnodes = $6; }
/Alps0278I Search wall-clock time: / { tottime = $5; }
/Blis0053I Called Clique cut generator/ { cuts += $9; }
/Blis0053I Called Flow Cover cut generator/ { cuts += $10; }
/Blis0053I Called Knapsack cut generator/ { cuts += $9; }
/Blis0053I Called Gomory cut generator/ { cuts += $9; }
/Cost = / { 
   pb = $3; 
   if ( opti == 1 )
      db = pb;
}
#iters not displayed (version 0.91)
/^=ready=/ {
   if( !onlyinsolufile || solstatus[prob] != "" )
   {
      bbnodes = max(bbnodes, 1);

      nprobs++;
    
      optimal = 0;
      markersym = "\\g";
      if( abs(pb - db) < 1e-06 && pb < infty)
      {
         gap = 0.0;
         optimal = 1;
         markersym = "  ";
      }
      else if( abs(db)*1.0 < 1e-06 )
         gap = -1.0;
      else if( pb*db*1.0 < 0.0 )
         gap = -1.0;
      else if( abs(db)*1.0 >= +infty )
         gap = -1.0;
      else if( abs(pb)*1.0 >= +infty )
         gap = -1.0;
      else
         gap = 100.0*abs((pb-db)*1.0/(1.0*db));

      if( gap < 0.0 )
         gapstr = "  --  ";
      else if( gap < 1e+04 )
         gapstr = sprintf("%6.1f", gap);
      else
         gapstr = " Large";

      printf("%-19s & %6d & %6d & %14.9g & %14.9g & %6s &%s%8d &%s%7.1f \\\\\n",
         pprob, cons, vars, db, pb, gapstr, markersym, bbnodes, markersym, tottime) >TEXFILE;

      printf("%-19s %6d %6d %16.9g %16.9g %6s %9d %8d %7.1f ",
             shortprob, cons, vars, db, pb, gapstr, iters, bbnodes, tottime);

      if( aborted )
      {
         printf("abort\n");
         failtime += tottime;
         fail++;
      }
      else if( solstatus[prob] == "opt" )
      {
 	 reltol = max(mipgap, 1e-5) * max(abs(pb),1.0);
	 abstol = max(absmipgap, 1e-4);
         if( (abs(pb - db) > max(abstol, reltol)) || (abs(pb - sol[prob]) > reltol) )
         {
            if (timeout)
            {
               printf("timeout\n");
               timeouttime += tottime;
               timeouts++;
            }
            else
            {
               printf("fail\n");
               failtime += tottime;
               fail++;
            }
         }
         else
         {
            printf("ok\n");
            pass++;
         }
      }
      else if( solstatus[prob] == "inf" )
      {
         if (feasible)
         {
            if (timeout)
            {
               printf("timeout\n");
               timeouttime += tottime;
               timeouts++;
            }
            else
            {
               printf("fail\n");
               failtime += tottime;
               fail++;
            }
         }
         else
         {
            printf("ok\n");
            pass++;
         }
      }
      else
      {
         if (timeout)
         {
            printf("timeout\n");
            timeouttime += tottime;
            timeouts++;
         }
         else
            printf("unknown\n");
      }
   
      sbab     += bbnodes;
      scut     += cuts;
      stottime += tottime;
      timegeom = timegeom^((nprobs-1)/nprobs) * max(tottime, 1.0)^(1.0/nprobs);
      nodegeom = nodegeom^((nprobs-1)/nprobs) * max(bbnodes, 1.0)^(1.0/nprobs);
      shiftedtimegeom = shiftedtimegeom^((nprobs-1)/nprobs) * max(tottime+timegeomshift, 1.0)^(1.0/nprobs);
      shiftednodegeom = shiftednodegeom^((nprobs-1)/nprobs) * max(bbnodes+nodegeomshift, 1.0)^(1.0/nprobs);
   }
}
END {
   shiftednodegeom -= nodegeomshift;
   shiftedtimegeom -= timegeomshift;

   printf("\\midrule\n")                                                 >TEXFILE;
   printf("%-14s (%2d) &        &        &                &                &        & %9d & %8.1f \\\\\n",
      "Total", nprobs, sbab, stottime) >TEXFILE;
   printf("%-14s      &        &        &                &                &        & %9d & %8.1f \\\\\n",
      "Geom. Mean", nodegeom, timegeom) >TEXFILE;
   printf("%-14s      &        &        &                &                &        & %9d & %8.1f \\\\\n",
      "Shifted Geom.", shiftednodegeom, shiftedtimegeom) >TEXFILE;
   printf("\\bottomrule\n")                                              >TEXFILE;
   printf("\\noalign{\\vspace{6pt}}\n")                                  >TEXFILE;
   printf("\\end{tabular*}\n")                                           >TEXFILE;
   printf("\\caption{BLIS with default settings}\n")                   >TEXFILE;
   printf("\\end{center}\n")                                             >TEXFILE;
   printf("\\end{table}\n")                                              >TEXFILE;
   printf("\\end{document}\n")                                           >TEXFILE;

   printf("------------------+-------+------+----------------+----------------+------+---------+--------+-------+-------\n");

   printf("\n");
   printf("------------------------------[Nodes]---------------[Time]------\n");
   printf("  Cnt  Pass  Time  Fail  total(k)     geom.     total     geom. \n");
   printf("----------------------------------------------------------------\n");
   printf("%5d %5d %5d %5d %9d %9.1f %9.1f %9.1f\n",
      nprobs, pass, timeouts, fail, sbab / 1000, nodegeom, stottime, timegeom);
   printf(" shifted geom. [%5d/%5.1f]      %9.1f           %9.1f\n",
      nodegeomshift, timegeomshift, shiftednodegeom, shiftedtimegeom);
   printf("----------------------------------------------------------------\n");

   printf("@01 BLIS(%s):%s\n", version, settings);
}
