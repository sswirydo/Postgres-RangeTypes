/*-------------------------------------------------------------------------
 *
 * geo_selfuncs.c
 *	  Selectivity routines registered in the operator catalog in the
 *	  "oprrest" and "oprjoin" attributes.
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/geo_selfuncs.c
 *
 *	XXX These are totally bogus.  Perhaps someone will make them do
 *	something reasonable, someday.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/builtins.h"
#include "utils/geo_decls.h"
#include "catalog/pg_operator.h"
#include "commands/vacuum.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"


/*
 *	Selectivity functions for geometric operators.  These are bogus -- unless
 *	we know the actual key distribution in the index, we can't make a good
 *	prediction of the selectivity of these operators.
 *
 *	Note: the values used here may look unreasonably small.  Perhaps they
 *	are.  For now, we want to make sure that the optimizer will make use
 *	of a geometric index if one is available, so the selectivity had better
 *	be fairly small.
 *
 *	In general, GiST needs to search multiple subtrees in order to guarantee
 *	that all occurrences of the same key have been found.  Because of this,
 *	the estimated cost for scanning the index ought to be higher than the
 *	output selectivity would indicate.  gistcostestimate(), over in selfuncs.c,
 *	ought to be adjusted accordingly --- but until we can generate somewhat
 *	realistic numbers here, it hardly matters...
 */


/*
 * Selectivity for operators that depend on area, such as "overlap".
 */
Datum
areasel(PG_FUNCTION_ARGS) //alex: on s'en fout de celui ci
{
	PG_RETURN_FLOAT8(0.005);
}







// alex: on utilise celui-ci pour des datums
Datum
areajoinsel(PG_FUNCTION_ARGS) // szymon: on veut celui-ci
{

	/* szymon: FOR MORE INFO, CHECK THE HEADER OF selfuncs.c
	*	Selectivity oprjoin (PlannerInfo *root,
    *						Oid operator,
    *						List *args,
    *						JoinType jointype,
    *						SpecialJoinInfo *sjinfo);
	*
	* WHERE
	* " root: general information about the query (rtable and RelOptInfo lists "
 	* " are particularly important for the estimator). "
	* " operator: OID of the specific operator in question. "
	* " args: argument list from the operator clause. "
	* others: " in the comments for
 	* clause_selectivity() --- the short version is that jointype is usually
 	* best ignored in favor of examining sjinfo. "
	*	
	* " Join selectivity for regular inner and outer joins is defined as the
	* fraction (0 to 1) of the cross product of the relations that is expected
	* to produce a TRUE result for the given operator.  For both semi and anti
	* joins, however, the selectivity is defined as the fraction of the left-hand
	* side relation's rows that are expected to have a match (ie, at least one
	* row with a TRUE result) in the right-hand side. "
	*
	* " For both oprrest and oprjoin functions, the operator's input collation OID
	* (if any) is passed using the standard fmgr mechanism, so that the estimator
	* function can fetch it with PG_GET_COLLATION().  Note, however, that all
	* statistics in pg_statistic are currently built using the relevant column's
	* collation. "
	*/

	
	// szymon: possibles arguments qu'on peut récup.
	PlannerInfo *root = (PlannerInfo *) PG_GETARG_POINTER(0);
	Oid			operator = PG_GETARG_OID(1);
	List	   *args = (List *) PG_GETARG_POINTER(2);
	JoinType	jointype = (JoinType) PG_GETARG_INT16(3);
	SpecialJoinInfo *sjinfo = (SpecialJoinInfo *) PG_GETARG_POINTER(4);

	// VacAttrStats *stats = (VacAttrStats *) PG_GETARG_POINTER(0);
	FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);
	/*
	for(int i = 0; i < 100; i++){
		printf("histogram2 %s\n",stats->stavalues[i]);
		fflush(stdout);
	}
	*/
	
	PG_RETURN_FLOAT8(0.005);
}











//szymon : osef de ce qu'il y a en dessous


/*
 *	positionsel
 *
 * How likely is a box to be strictly left of (right of, above, below)
 * a given box?
 */

Datum
positionsel(PG_FUNCTION_ARGS)
{
	PG_RETURN_FLOAT8(0.1);
}

Datum
positionjoinsel(PG_FUNCTION_ARGS)
{
	PG_RETURN_FLOAT8(0.1);
}

/*
 *	contsel -- How likely is a box to contain (be contained by) a given box?
 *
 * This is a tighter constraint than "overlap", so produce a smaller
 * estimate than areasel does.
 */

Datum
contsel(PG_FUNCTION_ARGS)
{
	PG_RETURN_FLOAT8(0.001);
}

Datum
contjoinsel(PG_FUNCTION_ARGS)
{
	PG_RETURN_FLOAT8(0.001);
}
