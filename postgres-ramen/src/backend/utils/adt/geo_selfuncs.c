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

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_type.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"
#include "utils/selfuncs.h"
#include "utils/typcache.h"

static double
calc_hist_selectivity_calmar(TypeCacheEntry *typcache, const RangeBound *constbound,
							const RangeBound *hist, int hist_nvalues, bool equal);

static int
rbound_bsmart(TypeCacheEntry *typcache, const RangeBound *value, const RangeBound *hist,
			   int hist_length, bool equal);

static float8
get_pos_lol(TypeCacheEntry *typcache, const RangeBound *value, const RangeBound *hist1,
			 const RangeBound *hist2);

static float8
get_thisdance(TypeCacheEntry *typcache, const RangeBound *bound1, const RangeBound *bound2);

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

	FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);
	
	// szymon: possibles arguments qu'on peut récup.
	PlannerInfo *root = (PlannerInfo *) PG_GETARG_POINTER(0);
	Oid			operator = PG_GETARG_OID(1);
	List	   *args = (List *) PG_GETARG_POINTER(2);
	int			varRelid = PG_GETARG_INT32(3);
	VariableStatData vardata;
	Node	   *other;
	bool		varonleft;
	Selectivity selec;
	TypeCacheEntry *typcache = NULL;
	RangeType  *constrange = NULL;

	AttStatsSlot hslot;
	AttStatsSlot lslot;
	int			nhist;
	RangeBound *hist_lower;
	RangeBound *hist_upper;
	int			i;
	RangeBound	const_lower;
	RangeBound	const_upper;
	bool		empty;
	double		hist_selec;


	// szymon: j'ai piqué pas mal de trucs de rangetypes_selfuncs.c où ils font des estimations de selections, afin
	// de comprendre comment récuperer les hustogrammes etc. etc.
	// le truc
	// c'est qu'en select on travaille avec une table + constantes
	// donc faut faire gaffe à ce TRES important détail
	// car tout le rangetypes_selfuncs.c dépenad la dessus (dedans, on return direct si on se choppe autre chose que cste)

	printf("debug_START\n");

	get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft); printf("debug_1\n");

	typcache = range_get_typcache(fcinfo, vardata.vartype); printf("debug_2\n");

	float4		empty_frac,
				null_frac;

	//First look up the fraction of NULLs and empty ranges from pg_statistic.
	if (HeapTupleIsValid(vardata.statsTuple))
	{
		printf("debug_2.5\n");
		
		AttStatsSlot sslot;
		Form_pg_statistic stats = (Form_pg_statistic) GETSTRUCT(vardata.statsTuple); // szymon : si jamais c'est pas les memes stats que ds typeanalyze alex essaie meme pas (j'ai essayé)
		null_frac = stats->stanullfrac;
		//Try to get fraction of empty ranges
		if (get_attstatsslot(&sslot, vardata.statsTuple,
							 STATISTIC_KIND_RANGE_LENGTH_HISTOGRAM,
							 InvalidOid,
							 ATTSTATSSLOT_NUMBERS))
		{
			if (sslot.nnumbers != 1)
				elog(ERROR, "invalid empty fraction statistic");	//shouldn't happen // szymon: mais omg ce genre de commentaire who coded this
			empty_frac = sslot.numbers[0];
			free_attstatsslot(&sslot);
		}
		else
		{
			/* No empty fraction statistic. Assume no empty ranges. */
			empty_frac = 0.0;
			printf("debug_stats_?\n");
		}
	}
	else
	{
		//No stats are available.
		printf("debug_no_stats_bruh\n");

	}

	printf("debug_5\n");
	/* Try to get histogram of ranges */
	//if (false &&
	if (!(HeapTupleIsValid(vardata.statsTuple) &&
		  get_attstatsslot(&hslot, vardata.statsTuple,
						   STATISTIC_KIND_BOUNDS_HISTOGRAM, InvalidOid,
						   ATTSTATSSLOT_VALUES)))
						{
							printf("debugOMFFU\n");
							PG_RETURN_FLOAT8(0.005);
						}
	// check that it's a histogram, not just a dummy entry //
	if (hslot.nvalues < 2)
	{
		free_attstatsslot(&hslot);
		printf("debugRIP\n");
		PG_RETURN_FLOAT8(0.005);
	}
	
	printf("debug_10\n");

	/*
	 * Convert histogram of ranges into histograms of its lower and upper
	 * bounds.
	 */
	nhist = hslot.nvalues; printf("debug_10.5\n");

	hist_lower = (RangeBound *) palloc(sizeof(RangeBound) * nhist); printf("debug_11\n");
	hist_upper = (RangeBound *) palloc(sizeof(RangeBound) * nhist); printf("debug_11.5\n");
	for (i = 0; i < nhist; i++)
	{
		printf("debug_11.9\n");

		range_deserialize(typcache, DatumGetRangeTypeP(hslot.values[i]),
						  &hist_lower[i], &hist_upper[i], &empty);
		/* The histogram should not contain any empty ranges */

		if (empty)
			elog(ERROR, "bounds histogram contains an empty range");
	}

	for (i = 0; i < nhist; i++) // szymon: bingo?
	{
		printf("debug: hslotvalues i:%d  val:%d\n", i, hslot.values[i]); // szymon: j'sais pas c'est quoi mais ça print de la patate
		// szymon: ces deux là par contre renoient des valuers intéressantes ^^
		printf("debug: hslotvalues i:%d  val:%d\n", i, hist_lower[i]); 
		printf("debug: hslotvalues i:%d  val:%d\n\n", i, hist_upper[i]);
	}
	
	printf("debug_12\n");


	// SZYMON: PARTIE EN DESSOUS POURRAIT SERVIR A MIEUX COMPRENDRE, A VOIR.
	//case OID_RANGE_OVERLAP_OP: 
	//case OID_RANGE_CONTAINS_ELEM_OP:
		/*
		* A && B <=> NOT (A << B OR A >> B).
		*
		* Since A << B and A >> B are mutually exclusive events we can
		* sum their probabilities to find probability of (A << B OR A >>
		* B).
		*
		* "range @> elem" is equivalent to "range && [elem,elem]". The
		* caller already constructed the singular range from the element
		* constant, so just treat it the same as &&.
		*/
	/*
	hist_selec =
		calc_hist_selectivity_calmar(typcache, &const_lower, hist_upper,
										nhist, false); // szymon: ça va pas marcher tel quel ici car on fait app à const_lower hors on bosse pas avec des const
	hist_selec +=
		(1.0 - calc_hist_selectivity_calmar(typcache, &const_upper, hist_lower,
											nhist, true));
	hist_selec = 1.0 - hist_selec;
	printf("histselec %d\n", hist_selec); //result= -2137326560 xd
	*/


	// szymon: le contenu original de la fonction est ici en dessous si besoin xd
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








// ------ SZYMON: OSEF DES FCTS CI DESSOSU C'ETAIT SURTOUT POUR COMPRENDRE ET DEBUG ------ //







// rbound_bsearch
/*
 * Binary search on an array of range bounds. Returns greatest index of range
 * bound in array which is less(less or equal) than given range bound. If all
 * range bounds in array are greater or equal(greater) than given range bound,
 * return -1. When "equal" flag is set conditions in brackets are used.
 *
 * This function is used in scalar operator selectivity estimation. Another
 * goal of this function is to find a histogram bin where to stop
 * interpolation of portion of bounds which are less than or equal to given bound.
 */
static int
rbound_bsmart(TypeCacheEntry *typcache, const RangeBound *value, const RangeBound *hist,
			   int hist_length, bool equal)
{
	int			lower = -1,
				upper = hist_length - 1,
				cmp,
				middle;

	while (lower < upper)
	{
		middle = (lower + upper + 1) / 2;
		cmp = range_cmp_bounds(typcache, &hist[middle], value);

		if (cmp < 0 || (equal && cmp == 0))
			lower = middle;
		else
			upper = middle - 1;
	}
	return lower;
}


//calc_hist_selectivity_scalar(TypeCacheEntry *typcache, const RangeBound *constbound,
							//const RangeBound *hist, int hist_nvalues, bool equal)

static double
calc_hist_selectivity_calmar(TypeCacheEntry *typcache, const RangeBound *constbound,
							const RangeBound *hist, int hist_nvalues, bool equal)
{
	Selectivity selec;
	int			index;

	/*
	* Find the histogram bin the given constant falls into. Estimate
	* selectivity as the number of preceding whole bins.
	*/
	index = rbound_bsmart(typcache, constbound, hist, hist_nvalues, equal);
	selec = (Selectivity) (Max(index, 0)) / (Selectivity) (hist_nvalues - 1);

	/* Adjust using linear interpolation within the bin */
	if (index >= 0 && index < hist_nvalues - 1)
		selec += get_pos_lol(typcache, constbound, &hist[index],
							&hist[index + 1]) / (Selectivity) (hist_nvalues - 1);

	return selec;
}



// get_position
/*
 * Get relative position of value in histogram bin in [0,1] range.
 */
static float8
get_pos_lol(TypeCacheEntry *typcache, const RangeBound *value, const RangeBound *hist1,
			 const RangeBound *hist2)
{
	bool		has_subdiff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);
	float8		position;

	if (!hist1->infinite && !hist2->infinite)
	{
		float8		bin_width;

		/*
		 * Both bounds are finite. Assuming the subtype's comparison function
		 * works sanely, the value must be finite, too, because it lies
		 * somewhere between the bounds.  If it doesn't, arbitrarily return
		 * 0.5.
		 */
		if (value->infinite)
			return 0.5;

		/* Can't interpolate without subdiff function */
		if (!has_subdiff)
			return 0.5;

		/* Calculate relative position using subdiff function. */
		bin_width = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo,
													 typcache->rng_collation,
													 hist2->val,
													 hist1->val));
		if (isnan(bin_width) || bin_width <= 0.0)
			return 0.5;			/* punt for NaN or zero-width bin */

		position = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo,
													typcache->rng_collation,
													value->val,
													hist1->val))
			/ bin_width;

		if (isnan(position))
			return 0.5;			/* punt for NaN from subdiff, Inf/Inf, etc */

		/* Relative position must be in [0,1] range */
		position = Max(position, 0.0);
		position = Min(position, 1.0);
		return position;
	}
	else if (hist1->infinite && !hist2->infinite)
	{
		/*
		 * Lower bin boundary is -infinite, upper is finite. If the value is
		 * -infinite, return 0.0 to indicate it's equal to the lower bound.
		 * Otherwise return 1.0 to indicate it's infinitely far from the lower
		 * bound.
		 */
		return ((value->infinite && value->lower) ? 0.0 : 1.0);
	}
	else if (!hist1->infinite && hist2->infinite)
	{
		/* same as above, but in reverse */
		return ((value->infinite && !value->lower) ? 1.0 : 0.0);
	}
	else
	{
		/*
		 * If both bin boundaries are infinite, they should be equal to each
		 * other, and the value should also be infinite and equal to both
		 * bounds. (But don't Assert that, to avoid crashing if a user creates
		 * a datatype with a broken comparison function).
		 *
		 * Assume the value to lie in the middle of the infinite bounds.
		 */
		return 0.5;
	}
}



//get_distance
/*
 * Measure distance between two range bounds.
 */
static float8
get_thisdance(TypeCacheEntry *typcache, const RangeBound *bound1, const RangeBound *bound2)
{
	bool		has_subdiff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);

	if (!bound1->infinite && !bound2->infinite)
	{
		/*
		 * Neither bound is infinite, use subdiff function or return default
		 * value of 1.0 if no subdiff is available.
		 */
		if (has_subdiff)
		{
			float8		res;

			res = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo,
												   typcache->rng_collation,
												   bound2->val,
												   bound1->val));
			/* Reject possible NaN result, also negative result */
			if (isnan(res) || res < 0.0)
				return 1.0;
			else
				return res;
		}
		else
			return 1.0;
	}
	else if (bound1->infinite && bound2->infinite)
	{
		/* Both bounds are infinite */
		if (bound1->lower == bound2->lower)
			return 0.0;
		else
			return get_float8_infinity();
	}
	else
	{
		/* One bound is infinite, the other is not */
		return get_float8_infinity();
	}
}


/*
// Return data from examine_variable and friends
typedef struct VariableStatData
{
	Node	   *var;			// the Var or expression tree //
	RelOptInfo *rel;			// Relation, or NULL if not identifiable //
	HeapTuple	statsTuple;		// pg_statistic tuple, or NULL if none //
	/* NB: if statsTuple!=NULL, it must be freed when caller is done //
	void		(*freefunc) (HeapTuple tuple);	/* how to free statsTuple //
	Oid			vartype;		// exposed type of expression //
	Oid			atttype;		// actual type (after stripping relabel) //
	int32		atttypmod;		// actual typmod (after stripping relabel) //
	bool		isunique;		// matches unique index or DISTINCT clause //
	bool		acl_ok;			// result of ACL check on table or column //
} VariableStatData;
*/