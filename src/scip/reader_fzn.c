/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2008 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   reader_fzn.h
 * @ingroup FILEREADERS 
 * @brief  FlatZinc file reader
 * @author Timo Berthold
 * @author Stefan Heinz
 *
 *@todo Test for uniqueness of variable and constraint names (after cutting down).
 *@todo remove pushBufferToken() staff since it is not used in this reader
 *@todo remove swapTokenBuffer() staff since it is not used in this reader
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#else
#include <strings.h>
#endif
#include <ctype.h>

#include "scip/cons_and.h"
#include "scip/cons_knapsack.h"
#include "scip/cons_linear.h"
#include "scip/cons_logicor.h"
#include "scip/cons_or.h"
#include "scip/cons_setppc.h"
#include "scip/cons_varbound.h"
#include "scip/cons_xor.h"
#include "scip/pub_misc.h"
#include "scip/reader_fzn.h"

#define READER_NAME             "fznreader"
#define READER_DESC             "FlatZinc file reader"
#define READER_EXTENSION        "fzn"


#define FZN_BUFFERLEN         65536     /**< size of the line buffer for reading or writing */
#define FZN_MAX_PUSHEDTOKENS  1
#define FZN_INIT_COEFSSIZE    8192

/*
 * Data structures
 */

/** number types */
enum FznNumberType {
   FZN_BOOL, FZN_INT, FZN_FLOAT
};
typedef enum FznNumberType FZNNUMBERTYPE;

/** Expression type in FlatZinc File */
enum FznExpType {
   FZN_EXP_NONE, FZN_EXP_UNSIGNED, FZN_EXP_SIGNED
};
typedef enum FznExpType FZNEXPTYPE;

/** FlatZinc constant */
struct FznConstant
{
   const char*          name;           /**< constant name */
   FZNNUMBERTYPE        type;           /**< constant type */
   SCIP_Real            value;          /**< constant value */
};
typedef struct FznConstant FZNCONSTANT;

/** tries to creates and adds a constraint; sets parameter created to TRUE if method was successful 
 * 
 *  input:
 *  - scip            : SCIP main data structure
 *  - fzninput,       : FZN reading data
 *  - fname,          : functions identifier name
 *  - ftokens,        : function identifier tokens 
 *  - nftokens,       : number of function identifier tokes
 *
 *  output
 *  - created         : pointer to store whether a constraint was created or not
 */
#define CREATE_CONSTRAINT(x) SCIP_RETCODE x (SCIP* scip, FZNINPUT* fzninput, const char* fname, char** ftokens, int nftokens, SCIP_Bool* created)


/** FlatZinc reading data */
struct FznInput
{
   SCIP_FILE*           file;
   SCIP_HASHTABLE*      varHashtable;
   SCIP_HASHTABLE*      constantHashtable;
   FZNCONSTANT**        constants;
   char                 linebuf[FZN_BUFFERLEN];
   char*                token;
   char*                pushedtokens[FZN_MAX_PUSHEDTOKENS];
   int                  npushedtokens;
   int                  linenumber;
   int                  linepos;
   int                  bufpos;
   int                  nconstants;
   int                  sconstants;
   SCIP_OBJSENSE        objsense;
   SCIP_Bool            hasdot;
   SCIP_Bool            endline;
   SCIP_Bool            haserror;
   SCIP_Bool            valid;
};
typedef struct FznInput FZNINPUT;

/** FlatZinc writting data */
struct FznOutput
{
   char*                varbuffer;
   int                  varbufferlen;
   int                  varbufferpos;
   char*                castbuffer;
   int                  castbufferlen;
   int                  castbufferpos;
   char*                consbuffer;
   int                  consbufferlen;
   int                  consbufferpos;
   int                  nvars;
   SCIP_Bool*           varhasfloat;
};
typedef struct FznOutput FZNOUTPUT;

static const char delimchars[] = " \f\n\r\t\v";
static const char tokenchars[] = ":<>=;{}[],()";
static const char commentchars[] = "%";

/*
 * Hash functions
 */

/** gets the key (i.e. the name) of the given variable */
static
SCIP_DECL_HASHGETKEY(hashGetKeyVar)
{  /*lint --e{715}*/
   SCIP_VAR* var = (SCIP_VAR*) elem;

   assert(var != NULL);
   return (void*) SCIPvarGetName(var);
}

/** gets the key (i.e. the name) of the flatzinc constant */
static
SCIP_DECL_HASHGETKEY(hashGetKeyConstant)
{  /*lint --e{715}*/
   FZNCONSTANT* constant = (FZNCONSTANT*) elem;

   assert(constant != NULL);
   return (void*) constant->name;
}

/*
 * Local methods (for reading)
 */

/** issues an error message and marks the FlatZinc data to have errors */
static
void syntaxError(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           msg                 /**< error message */
   )
{
   assert(fzninput != NULL);

   SCIPverbMessage(scip, SCIP_VERBLEVEL_MINIMAL, NULL, "Syntax error in line %d: %s found <%s>\n",
      fzninput->linenumber, msg, fzninput->token);

   SCIPverbMessage(scip, SCIP_VERBLEVEL_MINIMAL, NULL, "  input: %s", fzninput->linebuf);
   SCIPverbMessage(scip, SCIP_VERBLEVEL_MINIMAL, NULL, "\n");

   fzninput->haserror = TRUE;
}

/** returns whether a syntax error was detected */
static
SCIP_Bool hasError(
   FZNINPUT*              fzninput             /**< FZN reading data */
   )
{
   assert(fzninput != NULL);

   return (fzninput->haserror || !fzninput->valid);
}

/** frees a given buffer char* array */
static
void freeStringBufferArray(
   SCIP*                 scip,               /**< SCIP data structure */
   char**                array,              /**< buffer array to free */
   int                   nelements           /**< number of elements */
   )
{
   int i;

   for( i = 0; i < nelements; ++i )
   {
      SCIPfreeBufferArray(scip, &array[i]);
   }
   
   SCIPfreeBufferArray(scip, &array);
}

/** returns whether the given character is a token delimiter */
static
SCIP_Bool isDelimChar(
   char                  c                   /**< input character */
   )
{
   return (c == '\0') || (strchr(delimchars, c) != NULL);
}

/** returns whether the given character is a single token */
static
SCIP_Bool isTokenChar(
   char                  c                   /**< input character */
   )
{
   return (strchr(tokenchars, c) != NULL);
}

/** check if the current token is equal to give char */
static
SCIP_Bool isChar(
   const char*            token,           /**< token to be checked */
   char                   c                /**< char to compare */
   )
{
   if( strlen(token) == 1 && *token == c )
      return TRUE;
   
   return FALSE;
}

/** check if the current token is Bool expression, this means false or true */
static
SCIP_Bool isBoolExp(
   const char*            name,                /**< name to check */
   SCIP_Bool*             value                /**< pointer to store the Bool value */
   )
{
   /* check if the identifier starts with an letter */
   if( strlen(name) == 4 || strncmp(name, "true", 4) )
   {
      *value = TRUE;
      return TRUE;
   }
   else if( strlen(name) == 5 || strncmp(name, "false", 5) )
   {
      *value = FALSE;
      return TRUE;
   }
   
   return FALSE;
}   


/** check if the current token is an identifier, this means [A-Za-z][A-Za-z0-9_]* */
static
SCIP_Bool isIdentifier(
   const char*            name                 /**< name to check */
   )
{
   int i;
   
   /* check if the identifier starts with a letter */
   if( strlen(name) == 0 || !isalpha(name[0]) )
      return FALSE;
   
   i = 1;
   while( name[i] )
   {
      if( !isalnum(name[i]) && name[i] != '_' )
         return FALSE;
      i++;
   }
   
   return TRUE;
}   

/** returns whether the current character is member of a value string */
static
SCIP_Bool isValueChar(
   char                  c,                  /**< input character */
   char                  nextc,              /**< next input character */
   SCIP_Bool             firstchar,          /**< is the given character the first char of the token? */
   SCIP_Bool*            hasdot,             /**< pointer to update the dot flag */
   FZNEXPTYPE*           exptype             /**< pointer to update the exponent type */
   )
{
   assert(hasdot != NULL);
   assert(exptype != NULL);

   if( isdigit(c) )
      return TRUE;
   else if( firstchar && (c == '+' || c == '-') )
      return TRUE;
   else if( (*exptype == FZN_EXP_NONE) && !(*hasdot) && (c == '.') && (isdigit(nextc)))
   {
      *hasdot = TRUE;
      return TRUE;
   }
   else if( !firstchar && (*exptype == FZN_EXP_NONE) && (c == 'e' || c == 'E') )
   {
      if( nextc == '+' || nextc == '-' )
      {
         *exptype = FZN_EXP_SIGNED;
         return TRUE;
      }
      else if( isdigit(nextc) )
      {
         *exptype = FZN_EXP_UNSIGNED;
         return TRUE;
      }
   }
   else if( (*exptype == FZN_EXP_SIGNED) && (c == '+' || c == '-') )
   {
      *exptype = FZN_EXP_UNSIGNED;
      return TRUE;
   }

   return FALSE;
}

/** compares two token if they are equal */
static
SCIP_Bool equalTokens(
   const char*            token1,              /**< first token */
   const char*            token2               /**< second token */
   )
{
   assert(token1 != NULL);
   assert(token2 != NULL);

   if( strlen(token1) != strlen(token2) )
      return FALSE;
   
   return !strncmp(token1, token2, strlen(token2) );
}

/** reads the next line from the input file into the line buffer; skips comments;
 *  returns whether a line could be read
 */
static
SCIP_Bool getNextLine(
   FZNINPUT*              fzninput             /**< FZN reading data */
   )
{
   int i;
   char* last;
  
   assert(fzninput != NULL);

   /* clear the line */
   BMSclearMemoryArray(fzninput->linebuf, FZN_BUFFERLEN);
   fzninput->linebuf[FZN_BUFFERLEN-2] = '\0';
   
   /* set line position */
   if( fzninput->endline )
   {
      fzninput->linepos = 0;
      fzninput->linenumber++;
   }
   else
      fzninput->linepos += FZN_BUFFERLEN - 2;
   
   if( SCIPfgets(fzninput->linebuf, sizeof(fzninput->linebuf), fzninput->file) == NULL )
      return FALSE;
   
   fzninput->bufpos = 0;
      
   if( fzninput->linebuf[FZN_BUFFERLEN-2] != '\0' )
   {
      /* buffer is full; erase last token since it might be incomplete */
      fzninput->endline = FALSE;
      last = strrchr(fzninput->linebuf, ' ');

      if( last == NULL )
      {
         SCIPwarningMessage("we read %d character from the file; these might indicates an corrupted input file!\n", 
            FZN_BUFFERLEN - 2);
         fzninput->linebuf[FZN_BUFFERLEN-2] = '\0';
         SCIPdebugMessage("the buffer might be currented\n");
      }
      else
      {
         SCIPfseek(fzninput->file, -(long) strlen(last), SEEK_CUR);
         *last = '\0';
         SCIPdebugMessage("correct buffer\n");
      }
   }
   else 
   {
      /* found end of line */
      fzninput->endline = TRUE;
   }
   
   fzninput->linebuf[FZN_BUFFERLEN-1] = '\0';
   fzninput->linebuf[FZN_BUFFERLEN-2] = '\0'; /* we want to use lookahead of one char -> we need two \0 at the end */

   /* skip characters after comment symbol */
   for( i = 0; commentchars[i] != '\0'; ++i )
   {
      char* commentstart;

      commentstart = strchr(fzninput->linebuf, commentchars[i]);
      if( commentstart != NULL )
      {
         *commentstart = '\0';
         *(commentstart+1) = '\0'; /* we want to use lookahead of one char -> we need two \0 at the end */
      }
   }
   
   return TRUE;
}

/** swaps the addresses of two pointers */
static
void swapPointers(
   char**                pointer1,           /**< first pointer */
   char**                pointer2            /**< second pointer */
   )
{
   char* tmp;

   tmp = *pointer1;
   *pointer1 = *pointer2;
   *pointer2 = tmp;
}

/** reads the next token from the input file into the token buffer; returns whether a token was read */
static
SCIP_Bool getNextToken(
   FZNINPUT*              fzninput             /**< FZN reading data */
   )
{
   SCIP_Bool hasdot;
   FZNEXPTYPE exptype;
   char* buf;
   int tokenlen;

   assert(fzninput != NULL);
   assert(fzninput->bufpos < FZN_BUFFERLEN);

   /* check the token stack */
   if( fzninput->npushedtokens > 0 )
   {
      swapPointers(&fzninput->token, &fzninput->pushedtokens[fzninput->npushedtokens-1]);
      fzninput->npushedtokens--;
      SCIPdebugMessage("(line %d) read token again: '%s'\n", fzninput->linenumber, fzninput->token);
      return TRUE;
   }

   /* skip delimiters */
   buf = fzninput->linebuf;
   while( isDelimChar(buf[fzninput->bufpos]) )
   {
      if( buf[fzninput->bufpos] == '\0' )
      {
         if( !getNextLine(fzninput) )
         {
            SCIPdebugMessage("(line %d) end of file\n", fzninput->linenumber);
            return FALSE;
         }
         assert(fzninput->bufpos == 0);
      }
      else
      {
         fzninput->bufpos++;
         fzninput->linepos++;
      }
   }
   assert(fzninput->bufpos < FZN_BUFFERLEN);
   assert(!isDelimChar(buf[fzninput->bufpos]));

   hasdot = FALSE;
   exptype = FZN_EXP_NONE;

   if( buf[fzninput->bufpos] == '.' && buf[fzninput->bufpos+1] == '.')
   {
      /* found <..> which only occurs in Ranges and is a "keyword" */
      tokenlen = 2;
      fzninput->bufpos += 2;
      fzninput->linepos += 2;
      fzninput->token[0] = '.';
      fzninput->token[1] = '.';
   }
   else if( isValueChar(buf[fzninput->bufpos], buf[fzninput->bufpos+1], TRUE, &hasdot, &exptype) )
   {
      /* read value token */
      tokenlen = 0;
      do
      {
         assert(tokenlen < FZN_BUFFERLEN);
         assert(!isDelimChar(buf[fzninput->bufpos]));
         fzninput->token[tokenlen] = buf[fzninput->bufpos];
         tokenlen++;
         fzninput->bufpos++;
         fzninput->linepos++;
      }
      while( isValueChar(buf[fzninput->bufpos], buf[fzninput->bufpos+1], FALSE, &hasdot, &exptype) );
      
      fzninput->hasdot = hasdot;
   }
   else
   {
      /* read non-value token */
      tokenlen = 0;
      do
      {
         assert(tokenlen < FZN_BUFFERLEN);
         fzninput->token[tokenlen] = buf[fzninput->bufpos];
         tokenlen++;
         fzninput->bufpos++;
         fzninput->linepos++;

         /* check for annotations */
         if(tokenlen == 1 && fzninput->token[0] == ':' && buf[fzninput->bufpos] == ':')
         {      
            fzninput->token[tokenlen] = buf[fzninput->bufpos];
            tokenlen++;
            fzninput->bufpos++;
            fzninput->linepos++;
            break;
         }
            
         if( tokenlen == 1 && isTokenChar(fzninput->token[0]) )
            break;
      }
      while( !isDelimChar(buf[fzninput->bufpos]) && !isTokenChar(buf[fzninput->bufpos]) );
   }
   
   assert(tokenlen < FZN_BUFFERLEN);
   fzninput->token[tokenlen] = '\0';

   SCIPdebugMessage("(line %d) read token: '%s'\n", fzninput->linenumber, fzninput->token);

   return TRUE;
}

/** puts the current token on the token stack, such that it is read at the next call to getNextToken() */
static
void pushToken(
   FZNINPUT*              fzninput             /**< FZN reading data */
   )
{
   assert(fzninput != NULL);
   assert(fzninput->npushedtokens < FZN_MAX_PUSHEDTOKENS);

   swapPointers(&fzninput->pushedtokens[fzninput->npushedtokens], &fzninput->token);
   fzninput->npushedtokens++;
}

/** checks whether the current token is a semicolon which closes a statement */
static
SCIP_Bool isEndStatment(
   FZNINPUT*              fzninput             /**< FZN reading data */
   )
{
   assert(fzninput != NULL);
   
   return isChar(fzninput->token, ';');
}

/** returns whether the current token is a value */
static
SCIP_Bool isValue(
   const char*           token,              /**< token to check */
   SCIP_Real*            value               /**< pointer to store the value (unchanged, if token is no value) */
   )
{
   assert(value != NULL);

   double val;
   char* endptr;
   
   val = strtod(token, &endptr);
   if( endptr != token && *endptr == '\0' )
   {
      *value = val;
      return TRUE;
   }
   
   return FALSE;
}

/** creates, adds, and releases a linear constraint */
static
SCIP_RETCODE createLinearCons(
   SCIP*                 scip,               /**< SCIP data structure */
   const char*           name,               /**< name of constraint */
   int                   nvars,              /**< number of nonzeros in the constraint */
   SCIP_VAR**            vars,               /**< array with variables of constraint entries */
   SCIP_Real*            vals,               /**< array with coefficients of constraint entries */
   SCIP_Real             lhs,                /**< left hand side of constraint */
   SCIP_Real             rhs                 /**< right hand side of constraint */
   )   
{
   SCIP_CONS* cons;

   SCIP_CALL( SCIPcreateConsLinear(scip, &cons, name, nvars, vars, vals, lhs, rhs, 
         TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) ); 

   SCIPdebug( SCIPprintCons(scip, cons, NULL) );

   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   return SCIP_OKAY;
}

/** create a linking between the two given identifiers */ 
static
SCIP_RETCODE createLinking(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           consname,           /**< name of constraint */
   const char*           name1,              /**< name of first identifier */
   const char*           name2,              /**< name of second identifier */
   SCIP_Real             lhs,                /**< left hand side of the linkling */
   SCIP_Real             rhs                 /**< right hand side of the linkling */
   )
{
   SCIP_VAR** vars;
   SCIP_Real vals[] = {1.0,-1.0};
   SCIP_Real value;
   SCIP_Real sign;
   int nvars;

   nvars = 0;
   sign = -1.0;
   value = 0.0;

   SCIP_CALL( SCIPallocBufferArray(scip, &vars, 2) );
   
   vars[nvars] = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, (char*) name1);
   if( vars[nvars] != NULL )
   {
      nvars++;
      sign = 1.0;
   }
   else if( !isValue(name1, &value) )
   {
      FZNCONSTANT* constant;
      
      constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) name1);
      assert(constant != NULL);

      value = constant->value;
   }

   vars[nvars] = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, (char*) name2);
   if( vars[nvars] != NULL )
      nvars++;
   else if( !isValue(name2, &value) )
   {
      FZNCONSTANT* constant;
      
      constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) name2);
      assert(constant != NULL);
      
      value = constant->value;
   }
     
   assert( nvars > 0 );
   
   if( nvars == 2 )
   {
      SCIP_CALL( createLinearCons(scip, consname, 2, vars, vals, lhs, rhs) );
   }
   else
   {
      assert(nvars == 1);
      
      if( !SCIPisInfinity(scip, -lhs) )
         lhs += (sign * value);

      if( !SCIPisInfinity(scip, rhs) )
         rhs += (sign * value);

      SCIP_CALL( createLinearCons(scip, consname, 1, vars, vals, lhs, rhs) );
   }
   
   SCIPfreeBufferArray(scip, &vars);
   
   return SCIP_OKAY;
}

/** parse array index expression */
static
void parseArrayIndex(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   int*                  idx                 /**< pointer to store the array index */
   )
{
   SCIP_Real value;

   assert( isChar(fzninput->token, '[') );

   /* parse array index expresion */
   if( !getNextToken(fzninput) || isEndStatment(fzninput) )
   {
      syntaxError(scip, fzninput, "expecting array index expression");
      return;
   }   

   if( isIdentifier(fzninput->token) )
   {
      FZNCONSTANT* constant;

      /* identifier has to be one of a constant */
      constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, fzninput->token);

      assert(constant->type == FZN_INT);
      *idx = (int) constant->value;
   }
   else if( isValue(fzninput->token, &value) )
   {
      assert( fzninput->hasdot == FALSE );
      *idx = (int) value;
   }
   else
   {
      syntaxError(scip, fzninput, "expecting array index expression");
   }
}

/** unroll assignment if it is an array access one */
static
void flattenAssignment(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   char*                 assignment          /**< assignment to unroll */
   )
{
   assert(scip != NULL);
   assert(fzninput != NULL);

   SCIPdebugMessage("parse assignment expression\n");

   if( !getNextToken(fzninput) || isEndStatment(fzninput) )
   {
      syntaxError(scip, fzninput, "expecting more tokens");
      return;
   }   

   if( isIdentifier(fzninput->token) )
   {
      char name[FZN_BUFFERLEN];
      int idx;
      
      (void) SCIPsnprintf(name, FZN_BUFFERLEN, "%s", fzninput->token); 

      if( !getNextToken(fzninput) )
      {
         syntaxError(scip, fzninput, "expecting at least a semicolon to close the statement");
         return;
      }   
      
      /* check if it is an array access expression */
      if( isChar(fzninput->token, '[') )
      {
         idx = -1;
         parseArrayIndex(scip, fzninput, &idx);

         assert(idx >= 0);

         if( !getNextToken(fzninput) || !isChar(fzninput->token, ']') )
         {   
            syntaxError(scip, fzninput, "expecting token <]>");
            return;
         }

         /* put constant name or variable name together */
         (void) SCIPsnprintf(assignment, FZN_BUFFERLEN, "%s[%d]", name, idx);
      }
      else
      {
         (void) SCIPsnprintf(assignment, FZN_BUFFERLEN, "%s", name);

         /* push the current token back for latter evaluations */
         pushToken(fzninput);
      }
   }
   else
      (void) SCIPsnprintf(assignment, FZN_BUFFERLEN, "%s", fzninput->token);
}

/** computes w.r.t. to the given side value and relation the left and right side for a SCIP linear constraint */
static
void computeLinearConsSides(
   SCIP*                 scip,               /**< SCIP data structure */ 
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           name,               /**< name of the relation */
   SCIP_Real             sidevalue,          /**< parsed side value */
   SCIP_Real*            lhs,                /**< pointer to left hand side */
   SCIP_Real*            rhs                 /**< pointer to right hand side */
   )
{
   SCIPdebugMessage("check relation <%s>\n", name);

   /* compute left and right hand side of the linear constraint */
   if( equalTokens(name, "eq") )
   {
      *lhs = sidevalue;
      *rhs = sidevalue;
   }
   else if( equalTokens(name, "ge") )
   {
      *lhs = sidevalue;
      *rhs = SCIPinfinity(scip);
   }
   else if( equalTokens(name, "le") )
   {
      *lhs = -SCIPinfinity(scip);
      *rhs = sidevalue;
   }
   else if( equalTokens(name, "gt") )
   {
      /* greater than only works if there are not continuous variables are involved */
      *lhs = sidevalue + 1.0;
      *rhs = SCIPinfinity(scip);
   }
   else if( equalTokens(name, "lt") )
   {
      /* less than only works if there are not continuous variables are involved */
      *lhs = -SCIPinfinity(scip);
      *rhs = sidevalue - 1.0;
   }
   else
      syntaxError(scip, fzninput, "unknown relation in constraint identifier name");

   SCIPdebugMessage("lhs = %g, rhs = %g\n", *lhs, *rhs);
}

/** parse a list of elements */
static
SCIP_RETCODE parseList(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   char***               elements,           /**< pointer to char* array for storing the elements of the list */
   int*                  nelements,          /**< pointer to store the number of elements */
   int                   selements           /**< size of the elemnts char* array */
   )
{
   char assignment[FZN_BUFFERLEN];

   /* check if the list is not empty */
   if( getNextToken(fzninput) && !isChar(fzninput->token, ']') )
   {
      /* push back token */
      pushToken(fzninput);

      /* loop through the array */
      do
      {
         if(selements == *nelements)
         {
            selements *= 2;
            SCIP_CALL( SCIPreallocBufferArray(scip, elements, selements) );
         }
         
         /* parse and flatten assignment */
         flattenAssignment(scip, fzninput, assignment);
         
         if( hasError(fzninput) )
            break;
         
         /* strore assignment */
         SCIP_CALL( SCIPduplicateBufferArray(scip, &(*elements)[(*nelements)], assignment, (int) strlen(assignment) + 1) );
         
         (*nelements)++;
      }
      while( getNextToken(fzninput) && isChar(fzninput->token, ',') );
   }
   else
   {
      SCIPdebugMessage("list is empty\n");
   }

   
   /* push back ']' which closes the list */
   pushToken(fzninput);

   return SCIP_OKAY;
}

/** parse linking statement */
static
SCIP_RETCODE parseLinking(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           name,               /**< name of constraint */
   const char*           type,               /**< linear constraint type */
   SCIP_Real             sidevalue           /**< side value of constraint */
   )   
{
   char** names;
   SCIP_Real lhs;
   SCIP_Real rhs;
   int nnames;

   nnames = 0;
   SCIP_CALL( SCIPallocBufferArray(scip, &names, 2) );
   
   SCIP_CALL( parseList(scip, fzninput, &names, &nnames, 2) );
   assert(nnames == 2);
   
   if( hasError(fzninput) )
      goto TERMINATE;
   
   /* compute left and right side */
   computeLinearConsSides(scip, fzninput, type, sidevalue, &lhs, &rhs);
   
   if( hasError(fzninput) )
      goto TERMINATE;

   SCIP_CALL( createLinking(scip, fzninput, name, names[0], names[1], lhs, rhs) );
   
 TERMINATE:
   freeStringBufferArray(scip, names, nnames);
   
   return SCIP_OKAY;
}

/** parse identifier name without annotations */
static
void parseName(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   char*                 name                /**< pointer to store the name */
   )
{
   /* check for colon */
   if( !getNextToken(fzninput) || !isChar(fzninput->token, ':') )
   {
      syntaxError(scip, fzninput, "expecting colon <:>");
      return;
   }   
   
   /* parse identifier name */
   if( !getNextToken(fzninput) || !isIdentifier(fzninput->token) )
   {
      syntaxError(scip, fzninput, "expecting identifier name");
      return;
   }   
   
   /* copy identifier name */
   strncpy(name, fzninput->token, FZN_BUFFERLEN - 1);
   
   /* search for an assignment; therefore, skip annotations */
   do 
   {
      if( !getNextToken(fzninput) )
      {
         syntaxError(scip, fzninput, "expected at least a semicolon to close statement");
         return;
      }

      if( isEndStatment(fzninput) )
         break;
   }
   while( !isChar(fzninput->token, '=') );

   /* push back '=' or ';' */
   pushToken(fzninput);
}

/** parse range expression */
static
void parseRange(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   FZNNUMBERTYPE*        type,               /**< pointer to store the number type */
   SCIP_Real*            lb,                 /**< pointer to store the lower bound */
   SCIP_Real*            ub                  /**< pointer to store the upper bound */
   )
{
   if( !getNextToken(fzninput) )
   {
      syntaxError(scip, fzninput, "expected left side of range");
      return;
   }

   /* current token should be the lower bound */
   if( !isValue(fzninput->token, lb) )
      syntaxError(scip, fzninput, "expected lower bound value");
   
   /* check if we have a float notation or an integer notation which defines the type of the variable */
   if( fzninput->hasdot )
      *type = FZN_FLOAT;
   else
      *type = FZN_INT;

   /* parse next token which should be <..> */
   if( !getNextToken(fzninput) || !equalTokens(fzninput->token, "..") )
   {
      syntaxError(scip, fzninput, "expected <..>");
      return;
   }
   
   /* parse upper bound */
   if( !getNextToken(fzninput) || !isValue(fzninput->token, ub) )
   {
      syntaxError(scip, fzninput, "expected upper bound value");
      return;
   }
   
   /* check if upper bound notation fits which lower bound notation */
   if( fzninput->hasdot != (*type == FZN_FLOAT) )
   {
      SCIPwarningMessage("lower bound and upper bound dismatch in vlaue type, assume %s variable type\n", 
         fzninput->hasdot ? "an integer" : "a continuous");
   }
}

/** parse variable/constant (array) type (integer, float, bool, or set) */
static
void parseType(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   FZNNUMBERTYPE*        type,               /**< pointer to store the number type */
   SCIP_Real*            lb,                 /**< pointer to store the lower bound */
   SCIP_Real*            ub                  /**< pointer to store the lower bound */
   )
{
   if( !getNextToken(fzninput) || isEndStatment(fzninput) )
   {
      syntaxError(scip, fzninput, "missing token");
      return;
   }      
   
   *lb = -SCIPinfinity(scip);
   *ub = SCIPinfinity(scip);
   
   /* parse variable type or bounds */
   if( equalTokens(fzninput->token, "bool") )
   {
      *type = FZN_BOOL;
      *lb = 0.0;
      *ub = 1.0;
   }
   else if( equalTokens(fzninput->token, "float") )
      *type = FZN_FLOAT;
   else if( equalTokens(fzninput->token, "int") )
      *type = FZN_INT;
   else if( equalTokens(fzninput->token, "set") || isChar(fzninput->token, '{') )
   {
      SCIPwarningMessage("sets are not supported yet\n");
      fzninput->valid = FALSE;
      return;
   }
   else
   {
      /* the type is not explicitly given; it is given through the a range
       * expression; therefore, push back the current token since it
       * belongs to the range expression */
      pushToken(fzninput);
      parseRange(scip, fzninput, type, lb, ub);
   }
   
   SCIPdebugMessage("range =  [%g,%g]\n", *lb, *ub);

   assert(*lb <= *ub);
}

/** applies assignment */
static
SCIP_RETCODE applyVariableAssignment(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   SCIP_VAR*             var,                /**< variable to assign something */
   const char*           assignment          /**< assignment */
   )
{
   FZNCONSTANT* constant;
   SCIP_VAR* linkVar;
   SCIP_Bool boolvalue;
   SCIP_Real realvalue;
   SCIP_Real fixvalue;
   SCIP_Real vals[] = {1.0,-1.0};
   
   linkVar = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, (char*) assignment);
   constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) assignment);
   
   fixvalue = 0.0;

   if( linkVar == NULL )
   {
      if( isBoolExp(assignment, &boolvalue) && SCIPvarGetType(var) == SCIP_VARTYPE_BINARY )
         fixvalue = (SCIP_Real) boolvalue;
      else if( isValue(assignment, &realvalue) && SCIPvarGetType(var) != SCIP_VARTYPE_BINARY )
         fixvalue = realvalue;
      else if( constant != NULL )
         fixvalue = constant->value;
      else
      {
         syntaxError(scip, fzninput, "assignment is not recognizable");
         return SCIP_OKAY;
      }

      /* create fixing constraint */
      SCIP_CALL( createLinearCons(scip, "fixing", 1, &var, vals, fixvalue, fixvalue) );
   }
   else
   {
      SCIP_VAR** vars;
      
      SCIP_CALL( SCIPallocBufferArray(scip, &vars, 2) );
      vars[0] = var;
      vars[1] = linkVar;

      SCIP_CALL( createLinearCons(scip, "link", 2, vars, vals, 0.0, 0.0) );

      SCIPfreeBufferArray(scip, &vars);
   }

   return SCIP_OKAY;
}

/** applies constant assignment expression */
static
SCIP_RETCODE applyConstantAssignment(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           name,               /**< constant name */
   FZNNUMBERTYPE         type,               /**< number type */     
   const char*           assignment          /**< assignment to apply */
   )
{
   FZNCONSTANT* constant;
   SCIP_Bool boolvalue;
   SCIP_Real realvalue;
   SCIP_Real value;

   constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) assignment);

   if( constant != NULL )
   {
      /* check if the consatnt type fit */
      if( type != constant->type )
      {
         syntaxError(scip, fzninput, "type error");
         return SCIP_OKAY;
      }
      
      value = constant->value;
   }
   else if( isBoolExp(assignment, &boolvalue) && type == FZN_BOOL )
   {
      value = (SCIP_Real) boolvalue;
   }
   else if( isValue(assignment, &realvalue) && type != FZN_BOOL )
   {
      value = realvalue;
   }
   else
   {
      syntaxError(scip, fzninput, "assignment is not recognizable");
      return SCIP_OKAY;
   }

   /* get buffer memory for FZNCONSTANT struct */
   SCIP_CALL( SCIPallocBuffer(scip, &constant) );
   
   constant->type = type;
   SCIP_CALL( SCIPduplicateBufferArray(scip, &constant->name, name, (int) strlen(name) + 1) );
   constant->value = value;
   
   /* store constant */
   if( fzninput->sconstants == fzninput->nconstants )
   {
      assert(fzninput->sconstants > 0);
      fzninput->sconstants *= 2;
      SCIP_CALL( SCIPreallocBufferArray(scip, &fzninput->constants, fzninput->sconstants) );
   }

   assert(fzninput->sconstants > fzninput->nconstants);
   fzninput->constants[fzninput->nconstants] = constant;
   fzninput->nconstants++;
   
   SCIP_CALL( SCIPhashtableInsert(fzninput->constantHashtable, (void*) constant) );
   
   return SCIP_OKAY;
}

/** parse array type ( (i) variable or constant; (ii) integer, float, bool, or set) */
static
void parseArrayType(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   SCIP_Bool*            isvararray,           /**< pointer to store if it is a variable or constant array */
   FZNNUMBERTYPE*        type,               /**< pointer to store number type */
   SCIP_Real*            lb,                 /**< pointer to store the lower bound */
   SCIP_Real*            ub                  /**< pointer to store the lower bound */
   )
{
   if( !getNextToken(fzninput) || !equalTokens(fzninput->token, "of") )
   {
      syntaxError(scip, fzninput, "expected keyword  <of>");
      return;
   }
   
   if( !getNextToken(fzninput) )
   {
      syntaxError(scip, fzninput, "expected more tokens");
      return;
   }
   
   /* check if it is a variable or constant array */
   if( equalTokens(fzninput->token, "var") )
      *isvararray = TRUE;
   else
   {
      /* push token back since it belongs to the type declaration */
      pushToken(fzninput);
      *isvararray = FALSE;
   }

   /* pares array type and range */
   parseType(scip, fzninput, type, lb, ub);
}

/** parse an array assignment */
static
SCIP_RETCODE parseArrayAssignment(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   char***               elements,
   int*                  nelements,
   int                   selements
   )
{
   assert(scip != NULL);
   assert(fzninput != NULL);
   assert(*nelements >= 0);
   assert(selements >= *nelements);
   
   /* check for opening brackets */
   if( !getNextToken(fzninput) ||  !isChar(fzninput->token, '[') )
   {
      syntaxError(scip, fzninput, "expected token <[>");
      return SCIP_OKAY;
   }

   SCIP_CALL( parseList(scip, fzninput, elements, nelements, selements) );

   if( hasError(fzninput) )
      return SCIP_OKAY;
   
   /* check for closing brackets */
   if( !getNextToken(fzninput) ||  !isChar(fzninput->token, ']') )
      syntaxError(scip, fzninput, "expected token <]>");

   return SCIP_OKAY;
}

/** parse array dimension */
static
void parseArrayDimension(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   int*                  nelements           /**< pointer to store the size of the array */
   )
{
   FZNNUMBERTYPE type;
   SCIP_Real left;
   SCIP_Real right;
   
   if( !getNextToken(fzninput) || !isChar(fzninput->token, '[') )
   {
      syntaxError(scip, fzninput, "expected token <[> for array dimension");
      return;
   }

   /* get array dimension */
   parseRange(scip, fzninput, &type, &left, &right);
   
   if( type != FZN_INT || left != 1.0  || right <= 0.0 )
   {
      syntaxError(scip, fzninput, "invalid array dimension format");
      return;
   }
   
   *nelements = (int) right;
   
   if( !getNextToken(fzninput) || !isChar(fzninput->token, ']') )
   {
      syntaxError(scip, fzninput, "expected token <]> for array dimension");
      return;
   }
}

/** creates and adds a variable to SCIP and stores it for latter use  in fzninput structure */
static
SCIP_RETCODE createVariable(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   SCIP_VAR**            var,                /**< pointer to hold the created variable, or NULL */
   const char*           name,               /**< name of the varibale */
   SCIP_Real             lb,                 /**< lower bound of the variable */
   SCIP_Real             ub,                 /**< upper bound of the variable */
   FZNNUMBERTYPE         type                /**< number type */
   )
{
   SCIP_VAR* varcopy;
   SCIP_VARTYPE vartype;

   assert(scip != NULL);
   assert(fzninput != NULL);
   assert(lb <= ub);

   switch(type)
   {
   case FZN_BOOL:
      vartype = SCIP_VARTYPE_BINARY;
      break;
   case FZN_INT:
      vartype = SCIP_VARTYPE_INTEGER;
      break;
   case FZN_FLOAT:
      vartype = SCIP_VARTYPE_CONTINUOUS;
      break;
   default:
      syntaxError(scip, fzninput, "unknown variable type");
      return SCIP_OKAY;
   }

   /* create variable */
   SCIP_CALL( SCIPcreateVar(scip, &varcopy, name, lb, ub, 0.0, vartype, TRUE, TRUE, NULL, NULL, NULL, NULL) );
   SCIP_CALL( SCIPaddVar(scip, varcopy) );

   SCIPdebugMessage("created variable ");
   SCIPdebug(SCIPprintVar(scip, varcopy, NULL) );
   
   /* variable name should not exist before */
   assert(SCIPhashtableRetrieve(fzninput->varHashtable, varcopy) == NULL);
   
   /* insert variable into the hashmap for later use in the constraint section */
   SCIP_CALL( SCIPhashtableInsert(fzninput->varHashtable, varcopy) );

   /* copy variable pointer before releasing the variable to keep the pointer to the variable */
   if( var != NULL )
      *var = varcopy;
   
   /* release variable */
   SCIP_CALL( SCIPreleaseVar(scip, &varcopy) );
   
   return SCIP_OKAY;
}


/** parse variable array assignment and create the variables */
static
SCIP_RETCODE parseVariableArray(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           name,               /**< array name */
   int                   nvars,              /**< number of variables */
   FZNNUMBERTYPE         type,               /**< number type */
   SCIP_Real             lb,                 /**< lower bound of the variables */
   SCIP_Real             ub                  /**< lower bound of the variables */
   )
{
   SCIP_VAR** vars;
   char varname[FZN_BUFFERLEN]; 
   int v;
   
   /* create variables and add them to the problem */
   SCIP_CALL( SCIPallocBufferArray(scip, &vars, nvars) );

   for( v = 0; v < nvars; ++v )
   {
      (void) SCIPsnprintf(varname, FZN_BUFFERLEN, "%s[%d]", name, v + 1);
      
      /* cretae variable */
      SCIP_CALL( createVariable(scip, fzninput, &vars[v], varname, lb, ub, type) );
   }
   
   if( !getNextToken(fzninput) )
   {
      syntaxError(scip, fzninput, "expected semicolon");
      return SCIP_OKAY;
   }
   
   if( isChar(fzninput->token, '=') )
   {
      char** assigns;
      int nassigns;

      SCIP_CALL( SCIPallocBufferArray(scip, &assigns, nvars) );
      nassigns = 0;

      SCIP_CALL( parseArrayAssignment(scip, fzninput, &assigns, &nassigns, nvars) );

      if(!hasError(fzninput) )
      {
         for( v = 0; v < nvars && !hasError(fzninput); ++v )
         {
            /* parse and apply assignment */
            SCIP_CALL( applyVariableAssignment(scip, fzninput, vars[v], assigns[v]) );
         }
      }
      
      freeStringBufferArray(scip, assigns, nassigns);
   }
   else
   {
      /* push back the ';' */
      assert( isEndStatment(fzninput) );
      pushToken(fzninput);
   }
   
   SCIPfreeBufferArray(scip, &vars);

   return SCIP_OKAY;
}
 
/** parse constant array assignment and create the constants */
static
SCIP_RETCODE parseConstantArray(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           name,               /**< array name */
   int                   nconstants,         /**< number of constants */
   FZNNUMBERTYPE         type                /**< number type */
   )
{
   char** assigns;
   char constantname[FZN_BUFFERLEN];            \
   int nassigns;
   int c;
   
   if( !getNextToken(fzninput) || !isChar(fzninput->token, '=') )
   {
      syntaxError(scip, fzninput, "expected token <=>");
      return SCIP_OKAY;
   }
   
   SCIP_CALL( SCIPallocBufferArray(scip, &assigns, nconstants) );
   nassigns = 0;
   
   SCIP_CALL( parseArrayAssignment(scip, fzninput, &assigns, &nassigns, nconstants) );
   
   if( !hasError(fzninput) )
   {
      for( c = 0; c < nconstants; ++c )
      {
         (void) SCIPsnprintf(constantname, FZN_BUFFERLEN, "%s[%d]", name, c + 1);
         SCIP_CALL( applyConstantAssignment(scip, fzninput, constantname, type, assigns[c]) );
      }
   }

   freeStringBufferArray(scip, assigns, nassigns);
   
   return SCIP_OKAY;
}

/** parse array expression */
static
SCIP_RETCODE parseArray(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput            /**< FZN reading data */
   )
{
   FZNNUMBERTYPE type;
   int nelements;
   SCIP_Real lb;
   SCIP_Real ub;
   SCIP_Bool isvararray;
   char name[FZN_BUFFERLEN];

   assert(scip != NULL);
   assert(fzninput != NULL);

   isvararray = FALSE;

   SCIPdebugMessage("parse array expression\n");
   
   /* parse array dimension */
   parseArrayDimension(scip, fzninput, &nelements);
   
   if( hasError(fzninput) )
      return SCIP_OKAY;

   /* parse array type ( (i) variable or constant; (ii) integer, float, bool, or set) */
   parseArrayType(scip, fzninput, &isvararray, &type, &lb, &ub);

   if( hasError(fzninput) )
      return SCIP_OKAY;
   
   /* parse array name */
   parseName(scip, fzninput, name);
   
   if( hasError(fzninput) )
      return SCIP_OKAY;
   
   SCIPdebugMessage("found <%s> array named <%s> of type <%s> and size <%d> with bounds [%g,%g]\n",
      isvararray ? "variable" : "constant", name,
      type == FZN_BOOL ? "bool" : type == FZN_INT ? "integer" : "float", nelements, lb, ub);
   
   if( isvararray )
      SCIP_CALL( parseVariableArray(scip, fzninput, name, nelements, type, lb, ub) );
   else
      SCIP_CALL( parseConstantArray(scip, fzninput, name, nelements, type) );
   
   return SCIP_OKAY;
}

/** parse variable expression */
static
SCIP_RETCODE parseVariable(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput            /**< FZN reading data */
   )
{
   SCIP_VAR* var;
   FZNNUMBERTYPE type;
   SCIP_Real lb;
   SCIP_Real ub;
   char assignment[FZN_BUFFERLEN];
   char name[FZN_BUFFERLEN];
   
   assert(scip != NULL);
   assert(fzninput != NULL);

   SCIPdebugMessage("parse variable expression\n");
   
   /* pares variable type and range */
   parseType(scip, fzninput, &type, &lb, &ub);
   
   if( hasError(fzninput) )
      return SCIP_OKAY;
   
   /* parse variable name without annotations */
   parseName(scip, fzninput, name);
   
   if( hasError(fzninput) )
      return SCIP_OKAY;
   
   assert(type == FZN_BOOL || type == FZN_INT || type == FZN_FLOAT);

   /* cretae variable */
   SCIP_CALL( createVariable(scip, fzninput, &var, name, lb, ub, type) );

   if( !getNextToken(fzninput) )
   {
      syntaxError(scip, fzninput, "expected semicolon");
      return SCIP_OKAY;
   }

   if( isChar(fzninput->token, '=') )
   {
      /* parse and flatten assignment */
      flattenAssignment(scip, fzninput, assignment);
      
      /* apply assignment */
      SCIP_CALL( applyVariableAssignment(scip, fzninput, var, assignment) );
   }
   else
      pushToken(fzninput);

   return SCIP_OKAY;
}

/** parse constraint expression */
static
SCIP_RETCODE parseConstant(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   FZNNUMBERTYPE         type                /**< constant type */
   )
{
   char name[FZN_BUFFERLEN];
   char assignment[FZN_BUFFERLEN];

   assert(scip != NULL);
   assert(fzninput != NULL);
   assert(type == FZN_INT || type == FZN_INT || type == FZN_BOOL);

   SCIPdebugMessage("parse constant expression\n");
   
   /* parse name of the constant */
   parseName(scip, fzninput, name);
   
   if( hasError(fzninput) )
      return SCIP_OKAY;
   
   if( !getNextToken(fzninput) || !isChar(fzninput->token, '=') )
   {
      syntaxError(scip, fzninput, "expected token <=>");
      return SCIP_OKAY;
   }
   
   /* the assignment has to be an other constant or a suitable value */
   flattenAssignment(scip, fzninput, assignment);
   
   /* applies constant assignment and creates constant */
   SCIP_CALL( applyConstantAssignment(scip, fzninput, name, type, assignment) );

   return SCIP_OKAY;
} 

/** evaluates current token as constant */
static
void parseValue(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   SCIP_Real*            value,              /**< pointer to store value */
   const char*           assignment
   )
{
   if( isValue(assignment, value) )
      return;
   
   /* if it is an identifier name, it has to belong to a constant */
   if( isIdentifier(assignment) )
   {
      FZNCONSTANT* constant;
      
      /* identifier has to be one of a constant */
      constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) assignment);
      
      (*value) = constant->value;
   }
   else
      syntaxError(scip, fzninput, "expected constant expression");
}

/** parse array expression containing constants */
static
SCIP_RETCODE parseConstantArrayAssignment(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   SCIP_Real**           vals,               /**< pointer to value array */
   int*                  nvals,              /**< pointer to store the number if values */
   int                   sizevals            /**< size of the vals array */
   )
{
   char** elements;
   SCIP_Real value;
   int nelements;
   int c;

   assert(*nvals <= sizevals); 

   value = 0.0;

   SCIP_CALL( SCIPallocBufferArray(scip, &elements, sizevals) );
   nelements = 0;

   SCIP_CALL( parseArrayAssignment(scip, fzninput, &elements, &nelements, sizevals) );

   if( sizevals <= *nvals + nelements )
   {
      SCIP_CALL( SCIPreallocBufferArray(scip, vals, *nvals + nelements) );
   }

   for( c = 0; c < nelements && !hasError(fzninput); ++c )
   {
      parseValue(scip, fzninput, &value, elements[c]);
      (*vals)[(*nvals)] = value;
      (*nvals)++;
   }
   
   freeStringBufferArray(scip, elements, nelements);

   return SCIP_OKAY;
}

/** parse array expression containing variables */
static
SCIP_RETCODE parseVariableArrayAssignment(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   SCIP_VAR***           vars,               /**< pointer to variable array */
   int*                  nvars,              /**< pointer to store the number if variable */
   int                   sizevars            /**< size of the variable array */
   )
{
   char** elements;
   int nelements;
   int v;

   assert(*nvars <= sizevars);

   SCIP_CALL( SCIPallocBufferArray(scip, &elements, sizevars) );
   nelements = 0;

   SCIP_CALL( parseArrayAssignment(scip, fzninput, &elements, &nelements, sizevars) );

   if( sizevars <= *nvars + nelements )
   {
      SCIP_CALL( SCIPreallocBufferArray(scip, vars, *nvars + nelements) );
   }
   
   for( v = 0; v < nelements; ++v )
   {
      (*vars)[(*nvars)] = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, elements[v]);
      
      if( (*vars)[(*nvars)] == NULL )
      {
         /* since the given element does not correspond to an variable name
          * it might be the case that it is a constant which can be seen as
          * as a fixed variable */
         
         FZNCONSTANT* constant;
         SCIP_Real value;

         constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) elements[v]);

         if( constant != NULL )
         {
            assert(constant->type = FZN_FLOAT);
            value = constant->value;
         } 
         else if(!isValue(elements[v], &value) )
         {
            char* tmptoken;
         
            tmptoken = fzninput->token;
            fzninput->token = elements[v];
            syntaxError(scip, fzninput, "expected variable name or constant");
            
            fzninput->token = tmptoken;
            break;
         }
         
         /* create a fixed variable */
         SCIP_CALL( createVariable(scip, fzninput, &(*vars)[*nvars], elements[v], value, value, FZN_FLOAT) );
      }
      
      (*nvars)++;
   }

   freeStringBufferArray(scip, elements, nelements);
   
   return SCIP_OKAY;
}

/** creates a linear constraint for an array operation */
static
CREATE_CONSTRAINT(createCoercionOpCons)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(fzninput != NULL);
   
   /* check if the function identifier name is array operation */
   if( !equalTokens(fname, "int2float") && !equalTokens(fname, "bool2int") )
      return SCIP_OKAY;
   
   SCIP_CALL( parseLinking(scip, fzninput, fname, "eq", 0.0) );
   
   *created = TRUE;
   
   return SCIP_OKAY;
}

/** creates a linear constraint for an array operation */
static
CREATE_CONSTRAINT(createSetOpCons)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(fzninput != NULL);

   /* check if the function identifier name is array operation */
   if( !equalTokens(ftokens[0], "set") )
      return SCIP_OKAY;
   
   fzninput->valid = FALSE;
   SCIPwarningMessage("set operation are not supported yet\n");
   
   return SCIP_OKAY;
}

/** creates linear constraint for an array operation */
static
CREATE_CONSTRAINT(createArrayOpCons)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(fzninput != NULL);

   /* check if the function identifier name is array operation */
   if( !equalTokens(ftokens[0], "array") )
      return SCIP_OKAY;
   
   fzninput->valid = FALSE;
   SCIPwarningMessage("array operation are not supported yet\n");
   
   return SCIP_OKAY;
}

/** creates a linear constraint for a logical operation */
static
CREATE_CONSTRAINT(createLogicalOpCons)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(fzninput != NULL);

   /* check if the function identifier name is array operation */
   if(nftokens < 2)
      return SCIP_OKAY;
   
   if(equalTokens(ftokens[0], "bool") && nftokens == 2 )
   {
      char** elements;
      int nelements;
      
      /* the bool_eq constraint is processed in createComparisonOpCons() */
      if( equalTokens(ftokens[1], "eq") )
         return SCIP_OKAY;
      
      SCIP_CALL( SCIPallocBufferArray(scip, &elements, 3) );
      nelements = 0;
      
      SCIP_CALL( parseList(scip, fzninput, &elements, &nelements, 3) );
      
      if( !hasError(fzninput) )
      {
         SCIP_CONS* cons;
         SCIP_VAR** vars;
         int v;

         SCIP_CALL( SCIPallocBufferArray(scip, &vars, 3) );

         /* collect variable if constraint identifier is a variable */
         for( v = 0; v < 3; ++v )
         {
            vars[v] = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, (char*) elements[v]);
            
            if( vars[v] == NULL )
            {
               syntaxError(scip, fzninput, "unknown variable identifier name");
               goto TERMINATE;
            }
         }   

         if( equalTokens(ftokens[1], "or" ) )
         {
            SCIP_CALL( SCIPcreateConsOr(scip, &cons, fname, vars[2], 2, vars, 
                  TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) ); 
           
            *created = TRUE;
         }
         else if( equalTokens(ftokens[1], "and") )
         {
            SCIP_CALL( SCIPcreateConsAnd(scip, &cons, fname, vars[2], 2, vars, 
                  TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) ); 
            
            *created = TRUE;
         }
         else if( equalTokens(ftokens[1], "xor") )
         {
            SCIP_VAR* tmpVar;

            /* swap resultant to front */
            tmpVar = vars[2];
            vars[2] = vars[0];
            vars[0] = tmpVar;

            SCIP_CALL( SCIPcreateConsXor(scip, &cons, fname, FALSE, 3, vars, 
                  TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) ); 
            
            *created = TRUE;
         }
         else
         {
            fzninput->valid = FALSE;
            SCIPwarningMessage("logical operation <%s> is not supported yet\n", fname);
            goto TERMINATE;
         }
         
         SCIPdebug( SCIPprintCons(scip, cons, NULL) );

         SCIP_CALL( SCIPaddCons(scip, cons) );
         SCIP_CALL( SCIPreleaseCons(scip, &cons) );
         
      TERMINATE:
         SCIPfreeBufferArray(scip, &vars);
      }

      /* free elements array */
      freeStringBufferArray(scip, elements, nelements);
      
   }
   else if(equalTokens(ftokens[1], "bool") )   
   {
      fzninput->valid = FALSE;
      SCIPwarningMessage("logical operation <%s> is not supported yet\n", fname);
   }   
   else
      return SCIP_OKAY;
   
   return SCIP_OKAY;
}

/** creates a linear constraint for an comparison operation */
static
CREATE_CONSTRAINT(createComparisonOpCons)
{  /*lint --e{715}*/
   char assignment[FZN_BUFFERLEN];
   SCIP_Real lhs;
   SCIP_Real rhs;

   assert(scip != NULL);
   assert(fzninput != NULL);

   /* check if the function name ends of "reif" (reified constraint) which SCIP does not support yet */
   if( equalTokens(ftokens[nftokens - 1], "reif") )
   {
      SCIPwarningMessage("reified constraints are not supported\n");
      fzninput->valid = FALSE;
      return SCIP_OKAY;
   }
   
   /* the last token can only be 
    * 'eq' -- equal
    * 'ne' -- not equal
    * 'lt' -- less than
    * 'gt' -- greater than
    * 'le' -- less or equal than 
    * 'ge' -- greater or equal than 
    */
   if( strlen(ftokens[nftokens - 1]) != 2)
      return SCIP_OKAY;

   /* check if any sets are involved in the constraint */
   if( equalTokens(ftokens[0], "set") )
   {
      SCIPwarningMessage("constraints using sets are not supported\n");
      fzninput->valid = FALSE;
      return SCIP_OKAY;
   }
   
   /* check if the constraint is a 'not equal' one */
   if( equalTokens(ftokens[nftokens - 1], "ne") )
   {
      SCIPwarningMessage("constraints with 'not equal' relation are not supported\n");
      fzninput->valid = FALSE;
      return SCIP_OKAY;
   }

   /* check if the constraint contains float variable and coefficients and '<' or '>' relation */
   if( equalTokens(ftokens[0], "float") && 
      (equalTokens(ftokens[nftokens - 1], "lt") || equalTokens(ftokens[nftokens - 1], "gt") ) )
   {
      SCIPwarningMessage("constraints with '<' or '>' relation and continuous variables are not supported\n");
      fzninput->valid = FALSE;
      return SCIP_OKAY;
   }
   
   if( equalTokens(ftokens[1], "lin") )
   {
      SCIP_VAR** vars;
      SCIP_Real* vals;
      SCIP_Real sidevalue;
      int nvars;
      int nvals;
      int size;

      assert(nftokens == 3);
      
      size = 10;
      nvars = 0;
      nvals = 0;

      SCIP_CALL( SCIPallocBufferArray(scip, &vars, size) );
      SCIP_CALL( SCIPallocBufferArray(scip, &vals, size) );
      
      SCIPdebugMessage("found linear constraint <%s>\n", fname);
      
      /* pares coefficients array */
      SCIP_CALL( parseConstantArrayAssignment(scip, fzninput, &vals, &nvals, size) );  

      /* check error and for the komma between the coefficient and variable array */
      if( hasError(fzninput) || !getNextToken(fzninput) || !isChar(fzninput->token, ',') )
      {
         if( !hasError(fzninput) )            
            syntaxError(scip, fzninput, "expected token <,>");
         
         goto TERMINATE;
      }
      
      /* pares variable array */
      SCIP_CALL( parseVariableArrayAssignment(scip, fzninput, &vars, &nvars, size) );  
      
      /* check error and for the komma between the variable array and side value */
      if( hasError(fzninput) || !getNextToken(fzninput) || !isChar(fzninput->token, ',') )
      {
         if( !hasError(fzninput) )            
            syntaxError(scip, fzninput, "expected token <,>");
         
         goto TERMINATE;
      }

      /* pares sidevalue */
      flattenAssignment(scip, fzninput, assignment);
      parseValue(scip, fzninput, &sidevalue, assignment);
   
      if( !hasError(fzninput) )
      {
         /* compute left and right side */
         computeLinearConsSides(scip, fzninput, ftokens[2], sidevalue, &lhs, &rhs);
         
         if( hasError(fzninput) )
            goto TERMINATE;
         
         SCIP_CALL( createLinearCons(scip, fname, nvars, vars, vals, lhs, rhs) );
      }
      
   TERMINATE:
      SCIPfreeBufferArray(scip, &vars);
      SCIPfreeBufferArray(scip, &vals);
   }
   else
   {
      assert(nftokens == 2);
      SCIP_CALL( parseLinking(scip, fzninput, fname, ftokens[1], 0.0) );
   }

   *created = TRUE;

   return SCIP_OKAY;
}
   
/** parse constraint expression */
static
SCIP_RETCODE parseConstraint(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput            /**< FZN reading data */
   )
{
   /* function pointer array containing all function which can create a constraint */
   CREATE_CONSTRAINT((*constypes[])) =  {
      createCoercionOpCons,
      createSetOpCons,
      createLogicalOpCons,
      createArrayOpCons,
      createComparisonOpCons,
   };
   int nconstypes = 5;

   SCIP_VAR* var;
   char* tokens[4];
   char* token;
   char* nexttoken;
   char name[FZN_BUFFERLEN];
   char fname[FZN_BUFFERLEN];
   SCIP_Bool created;
   int ntokens;
   int i;
   int c;
   
   assert(scip != NULL);
   assert(fzninput != NULL);

   SCIPdebugMessage("parse constraint expression\n");
   
   /* get next token already flatten */ 
   flattenAssignment(scip, fzninput, name);
   
   /* check if constraint identifier is a variable */
   var = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, (char*) name);

   if( var != NULL )
   {
      SCIP_Real vals[] = {1.0};

      /* create fixing constraint */
      SCIP_CALL( createLinearCons(scip, "fixing", 1, &var, vals, 1.0, 1.0) );
      return SCIP_OKAY;
   }
   
   /* check constraint identifier name */
   if( !isIdentifier(name) )
   {
      syntaxError(scip, fzninput, "expected constraint identifier name");
      return SCIP_OKAY;
   }
   
   /* check if we have a opening parenthesis */
   if( !getNextToken(fzninput) || !isChar(fzninput->token, '(') )
   {
      syntaxError(scip, fzninput, "expected token <(>");
      return SCIP_OKAY;
   }
    
   /* copy function name */
   (void) SCIPsnprintf(fname, FZN_BUFFERLEN, "%s", name);
   
   /* truncate the function identifier name in separate tokes */
   token = SCIPstrtok(name, "_", &nexttoken);
   ntokens = 0;
   while( token != NULL )
   {
      if( ntokens == 4 )
         break;
      
      SCIP_CALL( SCIPduplicateBufferArray(scip, &tokens[ntokens], token, (int) strlen(token) + 1) );
      ntokens++;
      
      token = SCIPstrtok(NULL, "_", &nexttoken);
   }
   
   SCIPdebugMessage("%s", tokens[0]);
   for( i = 1; i < ntokens; ++i )
   {
      SCIPdebugPrintf(" %s", tokens[i]);
   }
   SCIPdebugPrintf("\n");
   
   created = FALSE;
   
   /* loop over all methods which can create a constraint */
   for( c = 0; c < nconstypes && !created && !hasError(fzninput); ++c )
   {
      SCIP_CALL( constypes[c](scip, fzninput, fname, tokens, ntokens, &created) );
   }
   
   /* check if a contraint was created */
   if( !hasError(fzninput) && !created )
   {
      fzninput->valid = FALSE;
      SCIPwarningMessage("constraint <%s> is not supported yet\n", fname);
   }
   
   /* free memory */
   for( i = 0; i < ntokens; ++i )
      SCIPfreeBufferArray(scip, &tokens[i]);
   
   /* check for the closing parenthesis */
   if( !hasError(fzninput) && ( !getNextToken(fzninput) || !isChar(fzninput->token, ')')) )
      syntaxError(scip, fzninput, "expected token <)>");
   
   return SCIP_OKAY;
}

/** parse solve item expression */
static
SCIP_RETCODE parseSolveItem(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput            /**< FZN reading data */
   )
{
   assert(scip != NULL);
   assert(fzninput != NULL);

   SCIPdebugMessage("parse solve item expression\n");
   
   if( !getNextToken(fzninput) )
   {
      syntaxError(scip, fzninput, "expected solving specification");
      return SCIP_OKAY;
   }

   /* check for annotations */
   if( equalTokens(fzninput->token, "::") )
   {
      /* skip the annotation */
      do
      {
         if( !getNextToken(fzninput) )
            syntaxError(scip, fzninput, "expected more tokens");
      }
      while( !equalTokens(fzninput->token, "satisfy") 
         && !equalTokens(fzninput->token, "minimize") 
         && !equalTokens(fzninput->token, "maximize") );
   }
   
   if( equalTokens(fzninput->token, "satisfy") )
   {
      SCIPdebugMessage("detected a satisfiability problem\n");
   }
   else 
   {
      SCIP_VAR* var;
      FZNCONSTANT* constant;
      char name[FZN_BUFFERLEN];

      if( equalTokens(fzninput->token, "minimize") )
      {
         fzninput->objsense = SCIP_OBJSENSE_MINIMIZE; 
         SCIPdebugMessage("detected a minimization problem\n");
      }
      else
      {
         assert(equalTokens(fzninput->token, "maximize"));
         fzninput->objsense = SCIP_OBJSENSE_MAXIMIZE; 
         SCIPdebugMessage("detected a maximization problem");
      }

      /* parse objective coefficients */

      /* parse and flatten assignment */
      flattenAssignment(scip, fzninput, name);

      var = (SCIP_VAR*) SCIPhashtableRetrieve(fzninput->varHashtable, (char*) name);
      constant = (FZNCONSTANT*) SCIPhashtableRetrieve(fzninput->constantHashtable, (char*) name);

      if( var != NULL )
      {
         SCIP_CALL(SCIPchgVarObj(scip, var, 1.0) );
      }
      else if( constant != NULL )
      {
         SCIPdebugMessage("optimizing a constant is equal to a satisfiability problem!\n");
      }
      else if( equalTokens(name, "int_float_lin") )
      {
         SCIP_VAR** vars;
         SCIP_Real* vals;
         int nvars;
         int nvals;
         int size;
         int v;

         nvars = 0;
         nvals = 0;
         size = 10;

         SCIP_CALL( SCIPallocBufferArray(scip, &vars, size) );
         SCIP_CALL( SCIPallocBufferArray(scip, &vals, size) );
         
         SCIPdebugMessage("found linear objective\n");
         
         if( !getNextToken(fzninput) || !isChar(fzninput->token, '(') )
         {
            syntaxError(scip, fzninput, "expected token <(>");
            goto TERMINATE;
         }
         
         /* pares coefficients array for integer variables */
         SCIP_CALL( parseConstantArrayAssignment(scip, fzninput, &vals, &nvals, size) );  
      
         /* check error and for the komma between the coefficient and variable array */
         if( hasError(fzninput) || !getNextToken(fzninput) || !isChar(fzninput->token, ',') )
         {
            if( !hasError(fzninput) )            
               syntaxError(scip, fzninput, "expected token <,>");
            
            goto TERMINATE;
         }
         
         /* pares coefficients array for continuous variables */
         SCIP_CALL( parseConstantArrayAssignment(scip, fzninput, &vals, &nvals, MAX(size, nvals)) );  
         
         /* check error and for the komma between the coefficient and variable array */
         if( hasError(fzninput) || !getNextToken(fzninput) || !isChar(fzninput->token, ',') )
         {
            if( !hasError(fzninput) )            
               syntaxError(scip, fzninput, "expected token <,>");
            
            goto TERMINATE;
         }

         /* pares integer variable array */
         SCIP_CALL( parseVariableArrayAssignment(scip, fzninput, &vars, &nvars, size) );  

         /* check error and for the komma between the variable array and side value */
         if( hasError(fzninput) || !getNextToken(fzninput) || !isChar(fzninput->token, ',') )
         {
            if( !hasError(fzninput) )            
               syntaxError(scip, fzninput, "expected token <,>");
            
            goto TERMINATE;
         }

         assert(nvars <= nvals);
         
         /* pares continuous variable array */
         SCIP_CALL( parseVariableArrayAssignment(scip, fzninput, &vars, &nvars, MAX(size, nvars)) );  

         /* check error and for the ')' */
         if( hasError(fzninput) || !getNextToken(fzninput) || !isChar(fzninput->token, ')') )
         {
            if( !hasError(fzninput) )            
               syntaxError(scip, fzninput, "expected token <)>");
            
            goto TERMINATE;
         }
         
         assert( nvars == nvals );
         
         for( v = 0; v < nvars; ++v )
         {
            SCIP_CALL(SCIPchgVarObj(scip, vars[v], vals[v]) );
         }
         
      TERMINATE:
         SCIPfreeBufferArray(scip, &vars);
         SCIPfreeBufferArray(scip, &vals);
      }
      else
      {
         syntaxError(scip, fzninput, "unknown identifier expresion for a objective function");
      }
   }
   
   return SCIP_OKAY;
}

/** reads an FlatZinc model */
static
SCIP_RETCODE readFZNFile(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNINPUT*             fzninput,           /**< FZN reading data */
   const char*           filename            /**< name of the input file */
   )
{
   assert(fzninput != NULL);

   /* open file */
   fzninput->file = SCIPfopen(filename, "r");
   if( fzninput->file == NULL )
   {
      SCIPerrorMessage("cannot open file <%s> for reading\n", filename);
      SCIPprintSysError(filename);
      return SCIP_NOFILE;
   }

   /* create problem */
   SCIP_CALL( SCIPcreateProb(scip, filename, NULL, NULL, NULL, NULL, NULL, NULL) );
   
   /* create two auxiliary variable for true and false values */
   SCIP_CALL( createVariable(scip, fzninput, NULL, "true", 1.0, 1.0, FZN_BOOL) );
   SCIP_CALL( createVariable(scip, fzninput, NULL, "false", 0.0, 0.0, FZN_BOOL) );

   /* parse through statements one-by-one */
   while( !SCIPfeof( fzninput->file ) && !hasError(fzninput) )
   {
      /* read the first token (keyword) of  a new statment */
      if( getNextToken(fzninput) )
      {  
         if( equalTokens(fzninput->token, "array") )
         {
            /* parse array expression containing constants or variables */
            SCIP_CALL( parseArray(scip, fzninput) );
         }
         else if( equalTokens(fzninput->token, "constraint") )
         {
            /* parse a constraint */
            SCIP_CALL( parseConstraint(scip, fzninput) );
         }
         else if( equalTokens(fzninput->token, "int") )
         {
            /* parse an integer constant */
            SCIP_CALL( parseConstant(scip, fzninput, FZN_INT) );
         }
         else if( equalTokens(fzninput->token, "float") )
         {
            /* parse a float constant */
            SCIP_CALL( parseConstant(scip, fzninput, FZN_FLOAT) );
         }
         else if( equalTokens(fzninput->token, "bool") )
         {
            /* parse a bool constant */
            SCIP_CALL( parseConstant(scip, fzninput, FZN_BOOL) );
         }
         else if( equalTokens(fzninput->token, "set") )
         {
            /* deal with sets */
            SCIPwarningMessage("sets are not supported yet\n");
            fzninput->valid = FALSE;
            break;
         }
         else if( equalTokens(fzninput->token, "solve") )
         {
            /* parse solve item (objective sense and objective function) */
            SCIP_CALL( parseSolveItem(scip, fzninput) );
         }
         else if( equalTokens(fzninput->token, "var") )
         {
            /* parse variables */
            SCIP_CALL( parseVariable(scip, fzninput) );
         }
         else if( equalTokens(fzninput->token, "output") )
         {
            /* the output section is the last section in the flatzinc model and can be skipped */
            SCIPdebugMessage("skip ouput section\n");
            break;
         }
         else 
         {
            FZNNUMBERTYPE type;
            SCIP_Real lb;
            SCIP_Real ub;
            
            /* check if the new statement starts with a range expression
             * which indicates a constant; therefore, push back the current token
             * since it belongs to the range expression */
            pushToken(fzninput);
            
            /* parse range to detect constant type */
            parseRange(scip, fzninput, &type, &lb, &ub);
            
            /* parse the remaining constant statement */
            parseConstant(scip, fzninput, type);

            if( hasError(fzninput) )
            {
               SCIPwarningMessage("unknown keyword <%s> skip statment\n", fzninput->token);
               return SCIP_OKAY;
            }
         }        
         
         if( hasError(fzninput) )
            break;

         /* each statement should be closed with a semicolon */
         if( !getNextToken(fzninput) )
            syntaxError(scip, fzninput, "expected semicolon");

         /* check for annotations */
         if( equalTokens(fzninput->token, "::") )
         {
            /* skip the annotation */
            do
            {
               if( !getNextToken(fzninput) )
                  syntaxError(scip, fzninput, "expected more tokens");
            }
            while( !isEndStatment(fzninput) );
         }
            
         if( !isEndStatment(fzninput) ) 
            syntaxError(scip, fzninput, "expected semicolon");
      }
   }
   
   /* close file */
   SCIPfclose(fzninput->file);
   
   if( hasError(fzninput) )
   {
      SCIP_CALL( SCIPfreeProb(scip) );

      /* create empty problem */
      SCIP_CALL( SCIPcreateProb(scip, filename, NULL, NULL, NULL, NULL, NULL, NULL) );
   }
   else
   {
      SCIP_CALL( SCIPsetObjsense(scip, fzninput->objsense) );
   }
   
   return SCIP_OKAY;
}


/*
 * Local methods (for writing)
 */


/** transforms given variables, scalars, and constant to the corresponding active variables, scalars, and constant */
static
SCIP_RETCODE getActiveVariables(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_VAR**            vars,               /**< vars array to get active variables for */
   SCIP_Real*            scalars,            /**< scalars a_1, ..., a_n in linear sum a_1*x_1 + ... + a_n*x_n + c */
   int*                  nvars,              /**< pointer to number of variables and values in vars and vals array */
   SCIP_Real*            constant,           /**< pointer to constant c in linear sum a_1*x_1 + ... + a_n*x_n + c  */
   SCIP_Bool             transformed         /**< transformed constraint? */
   )
{
   int requiredsize;
   int v;

   assert( scip != NULL );
   assert( scalars != NULL );
   assert( nvars != NULL );
   assert( vars != NULL || *nvars == 0 );
   assert( constant != NULL );

   if( transformed )
   {
      SCIP_CALL( SCIPgetProbvarLinearSum(scip, vars, scalars, nvars, *nvars, constant, &requiredsize, TRUE) );

      if( requiredsize > *nvars )
      {
         *nvars = requiredsize;
         SCIP_CALL( SCIPreallocBufferArray(scip, &vars, *nvars ) );
         SCIP_CALL( SCIPreallocBufferArray(scip, &scalars, *nvars ) );

         SCIP_CALL( SCIPgetProbvarLinearSum(scip, vars, scalars, nvars, *nvars, constant, &requiredsize, TRUE) );
         assert( requiredsize <= *nvars );
      }
   }
   else
      for( v = 0; v < *nvars; ++v )
         SCIP_CALL( SCIPvarGetOrigvarSum(&vars[v], &scalars[v], constant) );
   
   return SCIP_OKAY;
}
#if 0

/** clears the given line buffer */
static
void clearBuffer(
   char*                 linebuffer,         /**< line */
   int*                  linecnt             /**< number of charaters in line */
   )
{
   assert( linebuffer != NULL );
   assert( linecnt != NULL );

   (*linecnt) = 0;
   linebuffer[0] = '\0';
}
#endif

/** ends the given line with '\\0' and prints it to the given file stream */
static
void writeBuffer(
   SCIP*                 scip,               /**< SCIP data structure */
   FILE*                 file,               /**< output file (or NULL for standard output) */
   char*                 buffer,             /**< line */
   int                   bufferpos           /**< number of characters in buffer */
   )
{
   assert( scip != NULL );
   assert( buffer != NULL );
   
   if( bufferpos > 0 )
   {
      int i; 
      int ntokens;

      buffer[bufferpos] = '\0';
      ntokens = bufferpos / (SCIP_MAXSTRLEN-1);
      for( i =0; i<=ntokens; i++)
         SCIPinfoMessage(scip, file, "%s", buffer+i*(SCIP_MAXSTRLEN-1));
   }
}

/** appends extension to line and prints it to the give file stream if the line buffer get full */
static
SCIP_RETCODE appendBuffer(
   SCIP*                 scip,               /**< SCIP data structure */
   char**                buffer,             /**< buffer which should be extended */
   int*                  bufferlen,          /**< length of the buffer */
   int*                  bufferpos,          /**< current position in  the buffer */
   const char*           extension           /**< string to extend the line */
   )
{
   int newpos;

   assert( scip != NULL );
   assert( buffer != NULL );
   assert( bufferlen != NULL );
   assert( bufferpos != NULL );
   assert( extension != NULL );
   
   newpos = (*bufferpos) + strlen(extension); 
   if( newpos >= (*bufferlen) )
   { 
      *bufferlen = MAX( newpos, 2*(*bufferlen) );

      SCIP_CALL( SCIPreallocBufferArray(scip, buffer, (*bufferlen)));
   }
   
   /* append extension to linebuffer */
   strncpy((*buffer)+(*bufferpos), extension, (*bufferlen)-(*bufferpos));
   *bufferpos = newpos;

   return SCIP_OKAY;
}

static 
void flattenFloat(
   SCIP*                 scip,               /**< SCIP data structure */   
   SCIP_Real             val,                /**< value to flatten */
   char*                 buffer
   ) 
{
   if( SCIPisIntegral(scip, val) )
      (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%.1f",val);
   else
      (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%f",val);
 
}

/* print row in FZN format to file stream */
static
SCIP_RETCODE printRow(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNOUTPUT*            fznoutput,          /**< output data structure containing the buffers to write to */
   const char*           type,               /**< row type ("eq", "le" or "ge") */
   SCIP_VAR**            vars,               /**< array of variables */
   SCIP_Real*            vals,               /**< array of values */
   int                   nvars,              /**< number of variables */
   SCIP_Real             rhs,                /**< right hand side */
   SCIP_Bool             hasfloats           /**< are there continuous varibales or coefficients in the constraint? */
      )
{
   SCIP_VAR* var;
   char buffer[FZN_BUFFERLEN];
   char buffy[FZN_BUFFERLEN];
   int v;

   assert( scip != NULL );
   assert( strcmp(type, "eq") == 0 || strcmp(type, "le") == 0 || strcmp(type, "ge") == 0 );
  
   SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),"constraint ") );
   if( hasfloats )
      (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "float_lin_%s([",type);
   else
      (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "int_lin_%s([",type);
   SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),buffer) );

   /* print coefficients */
   for( v = 0; v < nvars-1; ++v )
   {
      if( hasfloats )
      {
         flattenFloat(scip,vals[v],buffy);
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s, ",buffy);
      }
      else
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%.f, ",vals[v]);
      SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),buffer) );
   }
   
   if( nvars > 0 )
   { 
      if( hasfloats )
      {
         flattenFloat(scip,vals[nvars-1],buffy);
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s",buffy);
      }
      else
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%.f",vals[nvars-1]);

      SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),buffer) );
   }   

   SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos), "], [") );
   
   for( v = 0; v < nvars-1; ++v )
   {
      var = vars[v];
      assert( var != NULL );
   
      if( hasfloats )
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s%s, ", SCIPvarGetName(var), SCIPvarGetProbindex(var) < fznoutput->nvars ? "_float" : "");
      else
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s, ", SCIPvarGetName(var) );
      SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),buffer) );
   }

   if( nvars > 0 )
   { 
      if( hasfloats )
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s%s",SCIPvarGetName(vars[nvars-1]), 
            SCIPvarGetProbindex(vars[nvars-1]) < fznoutput->nvars ? "_float" : "");
      else
         (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s",SCIPvarGetName(vars[nvars-1]));

      SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),buffer) );
   }
   
   SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos), "], ") );

   /* print right hand side */
   if( SCIPisZero(scip, rhs) )
      rhs = 0.0;
   
   if( hasfloats )
   {
      flattenFloat(scip,rhs,buffy);
      (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%s);\n",buffy);
   }
   else
      (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "%.f);\n",rhs);
   SCIP_CALL( appendBuffer(scip, &(fznoutput->consbuffer), &(fznoutput->consbufferlen), &(fznoutput->consbufferpos),buffer) );
    
   return SCIP_OKAY;
}

/** prints given linear constraint information in FZN format to file stream */
static
SCIP_RETCODE printLinearCons(
   SCIP*                 scip,               /**< SCIP data structure */
   FZNOUTPUT*            fznoutput,          /**< output data structure containing the buffers to write to */
   SCIP_VAR**            vars,               /**< array of variables */
   SCIP_Real*            vals,               /**< array of coefficients values (or NULL if all coefficient values are 1) */
   int                   nvars,              /**< number of variables */
   SCIP_Real             lhs,                /**< left hand side */
   SCIP_Real             rhs,                /**< right hand side */
   SCIP_Bool             transformed,        /**< transformed constraint? */
   SCIP_Bool             mayhavefloats       /**< may there be continuous variables in the constraint? */ 
   )
{
   int v;
   SCIP_VAR** activevars;
   SCIP_Real* activevals;
   int nactivevars;
   SCIP_Real activeconstant = 0.0;
   char buffer[FZN_BUFFERLEN];
   SCIP_Bool hasfloats;
   
   assert( scip != NULL );
   assert( vars != NULL || nvars == 0 );
   assert( fznoutput != NULL );
   assert( lhs <= rhs );

   if( SCIPisInfinity(scip, -lhs) && SCIPisInfinity(scip, rhs) )
      return SCIP_OKAY;
   
   /* duplicate variable and value array */
   nactivevars = nvars;
   hasfloats = FALSE;
   activevars = NULL;
   
   if( vars != NULL )
   {
      SCIP_CALL( SCIPduplicateBufferArray(scip, &activevars, vars, nactivevars ) );
   }   
   
   if( vals != NULL )
      SCIP_CALL( SCIPduplicateBufferArray(scip, &activevals, vals, nactivevars ) );
   else
   {
      SCIP_CALL( SCIPallocBufferArray(scip, &activevals, nactivevars) );
      
      for( v = 0; v < nactivevars; ++v )
         activevals[v] = 1.0;
   }
   
   /* retransform given variables to active variables */
   SCIP_CALL( getActiveVariables(scip, activevars, activevals, &nactivevars, &activeconstant, transformed) );
   if( mayhavefloats )
   {
      if( !SCIPisInfinity(scip, -lhs) )
         hasfloats = hasfloats || !SCIPisIntegral(scip,lhs-activeconstant);
      if( !SCIPisInfinity(scip, rhs) )
         hasfloats = hasfloats || !SCIPisIntegral(scip,rhs-activeconstant);

      for( v = 0; v < nactivevars && !hasfloats; v++ )
      {
         SCIP_VAR* var;
         var = activevars[v];         
            
         hasfloats = hasfloats || (SCIPvarGetType(var) != SCIP_VARTYPE_BINARY &&  SCIPvarGetType(var) != SCIP_VARTYPE_INTEGER);
         hasfloats = hasfloats || !SCIPisIntegral(scip,activevals[v]);
      }
    
      if( hasfloats )
      {
         for( v = 0; v < nactivevars; v++ )
         {
            SCIP_VAR* var;
            int idx;

            var = activevars[v];         
            idx = SCIPvarGetProbindex(var);
            assert( idx >= 0);

            if( idx < fznoutput->nvars && !fznoutput->varhasfloat[idx] )
            {
               assert(SCIPvarGetType(var) == SCIP_VARTYPE_BINARY || SCIPvarGetType(var) == SCIP_VARTYPE_INTEGER);
            
               (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "var float: %s_float;\n", SCIPvarGetName(var));
               SCIP_CALL( appendBuffer(scip, &(fznoutput->varbuffer), &(fznoutput->varbufferlen), &(fznoutput->varbufferpos),buffer) );

               (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "constraint int2float(%s, %s_float);\n", SCIPvarGetName(var), SCIPvarGetName(var));
               SCIP_CALL( appendBuffer(scip, &(fznoutput->castbuffer), &(fznoutput->castbufferlen), &(fznoutput->castbufferpos),buffer) );

               fznoutput->varhasfloat[idx] = TRUE;
            }
         }
      }
   }

   
   if( SCIPisEQ(scip, lhs, rhs) )
   {
      assert( !SCIPisInfinity(scip, rhs) );

      /* equality constraint */
      SCIP_CALL( printRow(scip, fznoutput, "eq", activevars, activevals, nactivevars, rhs - activeconstant, hasfloats) );
   }
   else
   { 
      if( !SCIPisInfinity(scip, -lhs) )
      {
         /* print inequality ">=" */
         SCIP_CALL( printRow(scip, fznoutput, "ge", activevars, activevals, nactivevars, lhs - activeconstant, hasfloats) );
      }

      
      if( !SCIPisInfinity(scip, rhs) )
      {
         /* print inequality "<=" */
         SCIP_CALL( printRow(scip, fznoutput, "le", activevars, activevals, nactivevars, rhs - activeconstant, hasfloats) );
      }
   }
   
   /* free buffer arrays */
   if( activevars != NULL )
      SCIPfreeBufferArray(scip, &activevars);
   SCIPfreeBufferArray(scip, &activevals);
   
   return SCIP_OKAY;
}

/* writes problem to file */
static
SCIP_RETCODE writeFzn(
   SCIP*              scip,               /**< SCIP data structure */
   FILE*              file,               /**< output file, or NULL if standard output should be used */
   const char*        name,               /**< problem name */
   SCIP_Bool          transformed,        /**< TRUE iff problem is the transformed problem */
   SCIP_OBJSENSE      objsense,           /**< objective sense */
   SCIP_Real          objscale,           /**< scalar applied to objective function; external objective value is
					     extobj = objsense * objscale * (intobj + objoffset) */
   SCIP_Real          objoffset,          /**< objective offset from bound shifting and fixing */
   SCIP_VAR**         vars,               /**< array with active variables ordered binary, integer, implicit, continuous */
   int                nvars,              /**< number of mutable variables in the problem */
   int                nbinvars,           /**< number of binary variables */
   int                nintvars,           /**< number of general integer variables */
   int                nimplvars,          /**< number of implicit integer variables */
   int                ncontvars,          /**< number of continuous variables */
   SCIP_CONS**        conss,              /**< array with constraints of the problem */
   int                nconss,             /**< number of constraints in the problem */
   SCIP_RESULT*       result              /**< pointer to store the result of the file writing call */
   )
{
   int c;
   int v;

   SCIP_CONSHDLR* conshdlr;
   const char* conshdlrname;
   SCIP_CONS* cons;
   
   SCIP_VAR** consvars;
   SCIP_Real* consvals;
   int nconsvars;

   int* boundedvars;
   SCIP_BOUNDTYPE* boundtypes; 
   int nboundedvars;
   int ndiscretevars;

   SCIP_VAR* var;

   SCIP_Real lb;
   SCIP_Real ub;
   char varname[SCIP_MAXSTRLEN];
   char buffer[FZN_BUFFERLEN];
   char buffy[FZN_BUFFERLEN];
        
   int* intobjvars;
   int* floatobjvars;
   int nintobjvars;
   int nfloatobjvars;

   FZNOUTPUT fznoutput;

   assert( scip != NULL );
   
   /* print statistics as comment to file */
   SCIPinfoMessage(scip, file, "%% SCIP STATISTICS\n");
   SCIPinfoMessage(scip, file, "%% Problem name     : %s\n", name);
   SCIPinfoMessage(scip, file, "%% Variables        : %d (%d binary, %d integer, %d implicit integer, %d continuous)\n",
      nvars, nbinvars, nintvars, nimplvars, ncontvars);
   SCIPinfoMessage(scip, file, "%% Constraints      : %d\n", nconss);
      
   SCIP_CALL( SCIPallocBufferArray(scip, &boundedvars, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &boundtypes, nvars) );
   nboundedvars = 0;
   ndiscretevars = nbinvars+nintvars;

   if( nvars > 0 )
      SCIPinfoMessage(scip, file, "\n%%%%%%%%%%%% Problem variables %%%%%%%%%%%%\n");   

   for( v = 0; v < nvars; v++ )
   {
      var = vars[v];
      assert( var != NULL );
      (void) SCIPsnprintf(varname, SCIP_MAXSTRLEN, "%s", SCIPvarGetName(var) );
     
      if( transformed )
      {
         /* in case the transformed is written only local bounds are posted which are valid in the current node */
         lb = SCIPvarGetLbLocal(var);
         ub = SCIPvarGetUbLocal(var);
      }
      else
      {
         lb = SCIPvarGetLbOriginal(var);
         ub = SCIPvarGetUbOriginal(var);
      }

      if( !SCIPisInfinity(scip, -lb) && !SCIPisInfinity(scip, ub) )
      {
         SCIP_Bool fixed;
         fixed = FALSE;

         if( SCIPisEQ(scip,lb,ub) )
            fixed = TRUE;

         if( v < ndiscretevars )
         {
	    assert( SCIPisIntegral(scip,lb) && SCIPisIntegral(scip,ub) );
	    
	    if( fixed )
               SCIPinfoMessage(scip, file, "var int: %s = %.f;\n", varname, lb);
	    else
               SCIPinfoMessage(scip, file, "var %.f..%.f: %s;\n", lb, ub, varname);
         }
         else
         {
	    if( fixed )
            { 
               flattenFloat(scip,lb,buffy);
               SCIPinfoMessage(scip, file, "var float: %s = %s;\n", varname, buffy);
	    }
            else
            {
               char buffy2[FZN_BUFFERLEN];
  
               flattenFloat(scip,lb,buffy);
               flattenFloat(scip,ub,buffy2);
               SCIPinfoMessage(scip, file,  "var %s..%s: %s;\n", buffy, buffy2, varname);
            }
         }
      }
      else
      {
         assert(SCIPvarGetType(var) != SCIP_VARTYPE_BINARY);
         assert( v >= nbinvars );

         if( v < nintvars )
	    SCIPinfoMessage(scip, file, "var int: %s;\n", varname);
         else 
	    SCIPinfoMessage(scip, file, "var float: %s;\n", varname);
	   
         if( SCIPisInfinity(scip, ub) ) 
         {
            boundedvars[nboundedvars] = v;
            boundtypes[nboundedvars] = SCIP_BOUNDTYPE_LOWER;
            nboundedvars++;
         }
         if( SCIPisInfinity(scip, -lb) ) 
         {
            boundedvars[nboundedvars] = v;
            boundtypes[nboundedvars] = SCIP_BOUNDTYPE_UPPER;
            nboundedvars++;
         }
      }
   }

   fznoutput.nvars = ndiscretevars; 
   fznoutput.varbufferpos = 0;
   fznoutput.consbufferpos = 0;
   fznoutput.castbufferpos = 0;
   
   SCIP_CALL( SCIPallocBufferArray(scip,&fznoutput.varhasfloat,ndiscretevars) );
   SCIP_CALL( SCIPallocBufferArray(scip,&fznoutput.varbuffer,FZN_BUFFERLEN) );
   SCIP_CALL( SCIPallocBufferArray(scip,&fznoutput.castbuffer,FZN_BUFFERLEN) );
   SCIP_CALL( SCIPallocBufferArray(scip,&fznoutput.consbuffer,FZN_BUFFERLEN) );
   fznoutput.consbufferlen = FZN_BUFFERLEN;
   fznoutput.varbufferlen = FZN_BUFFERLEN;
   fznoutput.castbufferlen = FZN_BUFFERLEN;
   
   for( v = 0; v < ndiscretevars; v++ )
      fznoutput.varhasfloat[v] = FALSE;
   fznoutput.varbuffer[0] = '\0';
   fznoutput.consbuffer[0] = '\0';
   fznoutput.castbuffer[0] = '\0';

   for( c = 0; c < nconss; c++ )
   {
      cons = conss[c];
      assert( cons != NULL);
      
      /* in case the transformed is written only constraint are posted which are enabled in the current node */
      if( transformed && !SCIPconsIsEnabled(cons) )
         continue;
      
      conshdlr = SCIPconsGetHdlr(cons);
      assert( conshdlr != NULL );

      conshdlrname = SCIPconshdlrGetName(conshdlr);
      assert( transformed == SCIPconsIsTransformed(cons) );

      if( strcmp(conshdlrname, "linear") == 0 )
      {
         SCIP_CALL( printLinearCons(scip, &fznoutput,
               SCIPgetVarsLinear(scip, cons), SCIPgetValsLinear(scip, cons), SCIPgetNVarsLinear(scip, cons),
               SCIPgetLhsLinear(scip, cons),  SCIPgetRhsLinear(scip, cons), transformed, TRUE) );
      }
      else if( strcmp(conshdlrname, "setppc") == 0 )
      {
         consvars = SCIPgetVarsSetppc(scip, cons);
         nconsvars = SCIPgetNVarsSetppc(scip, cons);

         switch ( SCIPgetTypeSetppc(scip, cons) )
         {
         case SCIP_SETPPCTYPE_PARTITIONING :
            SCIP_CALL( printLinearCons(scip, &fznoutput,
                  consvars, NULL, nconsvars, 1.0, 1.0, transformed, FALSE) );
            break;
         case SCIP_SETPPCTYPE_PACKING :
            SCIP_CALL( printLinearCons(scip, &fznoutput,
                  consvars, NULL, nconsvars, -SCIPinfinity(scip), 1.0, transformed, FALSE) );
            break;
         case SCIP_SETPPCTYPE_COVERING :
            SCIP_CALL( printLinearCons(scip, &fznoutput,
                  consvars, NULL, nconsvars, 1.0, SCIPinfinity(scip), transformed, FALSE) );
            break;
         }
      }
      else if ( strcmp(conshdlrname, "logicor") == 0 )
      {
         SCIP_CALL( printLinearCons(scip, &fznoutput,
               SCIPgetVarsLogicor(scip, cons), NULL, SCIPgetNVarsLogicor(scip, cons),
               1.0, SCIPinfinity(scip), transformed, FALSE) );
      }
      else if ( strcmp(conshdlrname, "knapsack") == 0 )
      {
	 SCIP_Longint* weights;

         consvars = SCIPgetVarsKnapsack(scip, cons);
         nconsvars = SCIPgetNVarsKnapsack(scip, cons);

         /* copy Longint array to SCIP_Real array */
         weights = SCIPgetWeightsKnapsack(scip, cons);
         SCIP_CALL( SCIPallocBufferArray(scip, &consvals, nconsvars) );
         for( v = 0; v < nconsvars; ++v )
            consvals[v] = weights[v];

         SCIP_CALL( printLinearCons(scip, &fznoutput, consvars, consvals, nconsvars, -SCIPinfinity(scip), 
				    (SCIP_Real) SCIPgetCapacityKnapsack(scip, cons), transformed, FALSE) );

         SCIPfreeBufferArray(scip, &consvals);
      }
      else if ( strcmp(conshdlrname, "varbound") == 0 )
      {
         SCIP_CALL( SCIPallocBufferArray(scip, &consvars, 2) );
         SCIP_CALL( SCIPallocBufferArray(scip, &consvals, 2) );

         consvars[0] = SCIPgetVarVarbound(scip, cons);
         consvars[1] = SCIPgetVbdvarVarbound(scip, cons);

         consvals[0] = 1.0;
         consvals[1] = SCIPgetVbdcoefVarbound(scip, cons);

         SCIP_CALL( printLinearCons(scip, &fznoutput,
               consvars, consvals, 2,
               SCIPgetLhsVarbound(scip, cons), SCIPgetRhsVarbound(scip, cons), transformed, TRUE) );

         SCIPfreeBufferArray(scip, &consvars);
         SCIPfreeBufferArray(scip, &consvals);
      }
      else
      {
         SCIPwarningMessage("constraint handler <%s> can not print flatzinc format\n", conshdlrname );
      }
   }

   SCIP_CALL( SCIPallocBufferArray(scip,&intobjvars,ndiscretevars) );
   SCIP_CALL( SCIPallocBufferArray(scip,&floatobjvars,nvars) );
   nintobjvars = 0;
   nfloatobjvars = 0;

   /* scan objective function */
   for( v = 0; v < nvars; v++ )
   {
      SCIP_Real obj;

      var = vars[v];
      obj = SCIPvarGetObj(var);

      if( !SCIPisZero(scip,obj) )
      {
         if( v < ndiscretevars && SCIPisIntegral(scip, objscale*obj) )
         {
            intobjvars[nintobjvars] = v;
            SCIPdebugMessage("variable <%s> at pos <%d,%d> has an integral obj: %f=%f*%f\n",
               SCIPvarGetName(var),nintobjvars,v,obj,objscale,SCIPvarGetObj(var));
            nintobjvars++;
         }
         else
         {
            if( v < ndiscretevars && !fznoutput.varhasfloat[v] )
            {
               assert(SCIPvarGetType(var) == SCIP_VARTYPE_BINARY || SCIPvarGetType(var) == SCIP_VARTYPE_INTEGER);
            
               (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "var float: %s_float;\n", SCIPvarGetName(var));
               SCIP_CALL( appendBuffer(scip, &(fznoutput.varbuffer), &(fznoutput.varbufferlen), &(fznoutput.varbufferpos),buffer) );

               (void) SCIPsnprintf(buffer, FZN_BUFFERLEN, "constraint int2float(%s, %s_float);\n", SCIPvarGetName(var), SCIPvarGetName(var));
               SCIP_CALL( appendBuffer(scip, &(fznoutput.castbuffer), &(fznoutput.castbufferlen), &(fznoutput.castbufferpos),buffer) );

               fznoutput.varhasfloat[v] = TRUE;
            }

            floatobjvars[nfloatobjvars] = v;
            nfloatobjvars++;
         }
      }
   }

   if( fznoutput.varbufferpos > 0 )
   {
      SCIPinfoMessage(scip, file, "\n%%%%%%%%%%%% Auxiliary variables %%%%%%%%%%%%\n");
      writeBuffer(scip, file, fznoutput.varbuffer, fznoutput.varbufferpos );
   }

   if( fznoutput.castbufferpos > 0 )
   {
      SCIPinfoMessage(scip, file, "\n%%%%%%%%%%%% Variable conversions %%%%%%%%%%%%\n");
      writeBuffer(scip, file, fznoutput.castbuffer, fznoutput.castbufferpos );
   }

   if( nboundedvars > 0 )
      SCIPinfoMessage(scip, file, "\n%%%%%%%%%%%% Variable bounds %%%%%%%%%%%%\n");
   
   for( v = 0; v < nboundedvars; v++ )
   {
      var = vars[boundedvars[v]];
       
      if( SCIPvarGetType(var) == SCIP_VARTYPE_INTEGER )
      {
         if( boundtypes[v] == SCIP_BOUNDTYPE_LOWER )
            SCIPinfoMessage(scip, file,"constraint int_ge(%s, %.f);\n",SCIPvarGetName(var),
               transformed ? SCIPvarGetLbLocal(var) : SCIPvarGetLbOriginal(var));
         else
         {
            assert( boundtypes[v] == SCIP_BOUNDTYPE_UPPER );
            SCIPinfoMessage(scip, file,"constraint int_le(%s, %.f);\n",SCIPvarGetName(var),
               transformed ? SCIPvarGetUbLocal(var) : SCIPvarGetUbOriginal(var));
         }
      }
      else
      {
         assert(SCIPvarGetType(var) == SCIP_VARTYPE_IMPLINT || SCIPvarGetType(var) == SCIP_VARTYPE_CONTINUOUS);
	  
         if( boundtypes[v] == SCIP_BOUNDTYPE_LOWER )
         {
            flattenFloat(scip,transformed ? SCIPvarGetLbLocal(var) : SCIPvarGetLbOriginal(var), buffy);
            SCIPinfoMessage(scip, file,"constraint float_ge(%s, %s);\n", SCIPvarGetName(var), buffy);
         }
         else
         {
            assert( boundtypes[v] == SCIP_BOUNDTYPE_UPPER );
            flattenFloat(scip,transformed ? SCIPvarGetUbLocal(var) : SCIPvarGetUbOriginal(var), buffy);
            SCIPinfoMessage(scip, file,"constraint float_le(%s, %s);\n",SCIPvarGetName(var), buffy);
         } 
      }
   }

   if( fznoutput.consbufferpos > 0 )
   {
      SCIPinfoMessage(scip, file, "\n%%%%%%%%%%%% Problem constraints %%%%%%%%%%%%\n");
      writeBuffer(scip, file, fznoutput.consbuffer, fznoutput.consbufferpos );
   }

   SCIPinfoMessage(scip, file, "\n%%%%%%%%%%%% Objective function %%%%%%%%%%%%\n");
   
   if( nintobjvars > 0 || nfloatobjvars > 0 )
   { 
      SCIPinfoMessage(scip, file, "solve %s int_float_lin([", objsense == SCIP_OBJSENSE_MINIMIZE ? "minimize" : "maximize" );
     
      for( v = 0; v < nintobjvars; v++ )
      {
         SCIP_Real obj;
         var = vars[intobjvars[v]];
         obj = objscale*SCIPvarGetObj(var);
         SCIPdebugMessage("variable <%s> at pos <%d,%d> has an integral obj: %f=%f*%f\n",SCIPvarGetName(var),v,intobjvars[v],obj,objscale,SCIPvarGetObj(var));

         assert( SCIPisIntegral(scip, obj) );
         flattenFloat(scip, obj, buffy);
         SCIPinfoMessage(scip, file, "%s%s", buffy, v < nintobjvars-1 ? ", " : "" );
      }

      SCIPinfoMessage(scip, file, "], [");
      for( v = 0; v < nfloatobjvars; v++ )
      {
         SCIP_Real obj;
         obj = objscale*SCIPvarGetObj(vars[floatobjvars[v]]);
         flattenFloat(scip, obj, buffy);
         assert( !SCIPisIntegral(scip, obj) || SCIPvarGetType(vars[floatobjvars[v]]) == SCIP_VARTYPE_CONTINUOUS 
            || SCIPvarGetType(vars[floatobjvars[v]]) == SCIP_VARTYPE_IMPLINT);
         SCIPinfoMessage(scip, file, "%s%s", buffy, v < nfloatobjvars-1 ? ", " : "" );
      }

      if( !SCIPisZero(scip, objoffset) )
      {
         flattenFloat(scip, objoffset, buffy);
         SCIPinfoMessage(scip, file, "%s%s", nfloatobjvars == 0 ? "" : ", ", buffy );
      }

      SCIPinfoMessage(scip, file, "], [");
      for( v = 0; v < nintobjvars; v++ )
         SCIPinfoMessage(scip, file, "%s%s", SCIPvarGetName(vars[intobjvars[v]]), v < nintobjvars-1 ? ", " : "" );
      
      SCIPinfoMessage(scip, file, "], [");
      for( v = 0; v < nfloatobjvars; v++ )
         SCIPinfoMessage(scip, file, "%s%s%s", SCIPvarGetName(vars[floatobjvars[v]]), floatobjvars[v] < ndiscretevars ? "_float" : "", v < nfloatobjvars-1 ? ", " : "" );
    
      if( !SCIPisZero(scip, objoffset) )
         SCIPinfoMessage(scip, file, "%s%.1f", nfloatobjvars == 0 ? "" : ", ", 1.0 );
      SCIPinfoMessage(scip, file, "]);\n");
   }
   else
      SCIPinfoMessage(scip, file, "solve satisfy;\n");
   
   SCIPfreeBufferArray(scip, &fznoutput.castbuffer);
   SCIPfreeBufferArray(scip, &fznoutput.consbuffer);
   SCIPfreeBufferArray(scip, &fznoutput.varbuffer);
   
   SCIPfreeBufferArray(scip, &boundtypes);
   SCIPfreeBufferArray(scip, &boundedvars);
   SCIPfreeBufferArray(scip, &floatobjvars);
   SCIPfreeBufferArray(scip, &intobjvars);  
   SCIPfreeBufferArray(scip, &fznoutput.varhasfloat);

   *result = SCIP_SUCCESS;
   return  SCIP_OKAY;
}

/*
 * Callback methods of reader
 */

/** destructor of reader to free user data (called when SCIP is exiting) */
#define readerFreeFzn NULL


/** problem reading method of reader */
static
SCIP_DECL_READERREAD(readerReadFzn)
{  /*lint --e{715}*/

   FZNINPUT fzninput;
   int i;

   /* initialize FZN input data */
   fzninput.file = NULL;
   fzninput.linebuf[0] = '\0';
   SCIP_CALL( SCIPallocBufferArray(scip, &fzninput.token, FZN_BUFFERLEN) );
   fzninput.token[0] = '\0';

   for( i = 0; i < FZN_MAX_PUSHEDTOKENS; ++i )
   {
      SCIP_CALL( SCIPallocBufferArray(scip, &fzninput.pushedtokens[i], FZN_BUFFERLEN) );
   }

   fzninput.npushedtokens = 0;
   fzninput.linenumber = 1;
   fzninput.bufpos = 0;
   fzninput.linepos = 0;
   fzninput.objsense = SCIP_OBJSENSE_MINIMIZE;
   fzninput.endline = FALSE;
   fzninput.haserror = FALSE;
   fzninput.valid = TRUE;
   
   SCIP_CALL( SCIPhashtableCreate(&fzninput.varHashtable, SCIPblkmem(scip), SCIP_HASHSIZE_NAMES,
         hashGetKeyVar, SCIPhashKeyEqString, SCIPhashKeyValString, NULL) );
   
   SCIP_CALL( SCIPhashtableCreate(&fzninput.constantHashtable, SCIPblkmem(scip), SCIP_HASHSIZE_NAMES,
         hashGetKeyConstant, SCIPhashKeyEqString, SCIPhashKeyValString, NULL) );
   SCIP_CALL( SCIPallocBufferArray(scip, &fzninput.constants, 10) );

   fzninput.nconstants = 0;
   fzninput.sconstants = 10;

   /* read the file */
   SCIP_CALL( readFZNFile(scip, &fzninput, filename) );
   
   /* free dynamically allocated memory */
   SCIPfreeBufferArrayNull(scip, &fzninput.token);
   for( i = 0; i < FZN_MAX_PUSHEDTOKENS; ++i )
   {
      SCIPfreeBufferArrayNull(scip, &fzninput.pushedtokens[i]);
   }

   /* free buffer memory */
   for( i = 0; i < fzninput.nconstants; ++i )
   {
      SCIPfreeBufferArray(scip, &fzninput.constants[i]->name);
      SCIPfreeBuffer(scip, &fzninput.constants[i]);
   }
   SCIPhashtableFree(&fzninput.varHashtable);
   SCIPhashtableFree(&fzninput.constantHashtable);
   SCIPfreeBufferArray(scip, &fzninput.constants);

   /* evaluate the result */
   if( fzninput.haserror )
      return SCIP_PARSEERROR;

   *result = SCIP_SUCCESS;

   return SCIP_OKAY;
}


/** problem writing method of reader */
static
SCIP_DECL_READERWRITE(readerWriteFzn)
{  /*lint --e{715}*/
   if( genericnames )
   {
      SCIP_CALL( writeFzn(scip, file, name, transformed, objsense, objscale, objoffset, vars,
            nvars, nbinvars, nintvars, nimplvars, ncontvars, conss, nconss, result) );
   }
   else
   {
      int i;
      SCIP_Bool legal;

      legal = TRUE;

      for( i = 0; i < nvars; i++ )
      {
         const char* varname;
         size_t length;

         varname = SCIPvarGetName(vars[i]);
         length = strlen(varname);
         legal = legal && isIdentifier(varname); 
         if( !legal )
         {
            SCIPwarningMessage("The name of variable <%d>: \"%s\" is not conform to the fzn standard.\n", i, varname);
            break;
         }

         if( length >= 7 )
            legal = legal && (strncmp(&varname[length-6],"_float",6) != 0);
         if( !legal )
         {
            SCIPwarningMessage("The name of variable <%d>: \"%s\" ends with \"_float\" which is not supported.\n", i, varname);
            break;
         }
      }
      
      if( legal )
      {
         SCIP_CALL( writeFzn(scip, file, name, transformed, objsense, objscale, objoffset, vars,
               nvars, nbinvars, nintvars, nimplvars, ncontvars, conss, nconss, result) );
      }
      else if( transformed )
      {
         SCIPwarningMessage("Write transformed problem with generic variable names.\n");
         SCIP_CALL( SCIPprintTransProblem(scip, file, "fzn", TRUE) );
      }
      else
      {
         SCIPwarningMessage("Write original problem with generic variable names.\n");
         SCIP_CALL( SCIPprintOrigProblem(scip, file, "fzn", TRUE) );
      }
   }

   *result = SCIP_SUCCESS;
   
   return SCIP_OKAY;
}

/*
 * reader specific interface methods
 */

/** includes the fzn file reader in SCIP */
SCIP_RETCODE SCIPincludeReaderFzn(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_READERDATA* readerdata;

   /* create fzn reader data */
   readerdata = NULL;

   /* include fzn reader */
   SCIP_CALL( SCIPincludeReader(scip, READER_NAME, READER_DESC, READER_EXTENSION,
         readerFreeFzn, readerReadFzn, readerWriteFzn, readerdata) );

   return SCIP_OKAY;
}
