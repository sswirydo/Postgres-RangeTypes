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

// -- H-417 OUR FUNCTIONS -- //
Datum rangeoverlapsjoinsel(PG_FUNCTION_ARGS);
Selectivity rangeoverlapsjoinsel_inner(float* freq_values1, float* freq_values2, int freq_nb_intervals1, int freq_nb_intervals2, int rows1, int rows2, int min1, int min2, int max1, int max2);
static int roundUpDivision(int numerator, int divider);
static void _debug_print_frequencies(float8* frequencies_vals, int size);
// ------------------------- //


/*
 * Range Overlaps Join Selectivity.
 */
Datum
rangeoverlapsjoinsel(PG_FUNCTION_ARGS)
{
	FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);

    PlannerInfo *root = (PlannerInfo *) PG_GETARG_POINTER(0);
    Oid         operator = PG_GETARG_OID(1);
    List       *args = (List *) PG_GETARG_POINTER(2);
    JoinType    jointype = (JoinType) PG_GETARG_INT16(3);
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *) PG_GETARG_POINTER(4);
    Oid         collation = PG_GET_COLLATION();

    VariableStatData vardata1, vardata2;
    Oid         opfuncoid;
    AttStatsSlot sslot1, sslot2;
    int         nhist1, nhist2;
    int         i;
    Form_pg_statistic stats1 = NULL;
    Form_pg_statistic stats2 = NULL;
    TypeCacheEntry* typcache = NULL;
    bool        join_is_reversed; // TODO Pas ENCORE utilisé, ici, mais potentiellement important. 
    bool        empty;
    AttStatsSlot freq_sslot1, freq_sslot2;
    int         freq_nb_intervals1, freq_nb_intervals2;
    float*        freq_values1 = NULL
    float*        freq_values2 = NULL
    Selectivity selec = 0.005; // Selectivity is a double. It is named Selectivity just to make its purpose more obvious.

    // -- Retriving important variables. -- //
    get_join_variables(root, args, sjinfo, &vardata1, &vardata2, &join_is_reversed);
    typcache = range_get_typcache(fcinfo, vardata1.vartype);
    opfuncoid = get_opcode(operator);

    // -- Allocating memory for AttStatsSlot (stores our statistics) -- //
    memset(&sslot1, 0, sizeof(sslot1));
    memset(&sslot2, 0, sizeof(sslot2));
    memset(&freq_sslot1, 0, sizeof(freq_sslot1));
    memset(&freq_sslot2, 0, sizeof(freq_sslot2));

    // -- STATISTIC CHECK -- //
    if (!statistic_proc_security_check(&vardata1, opfuncoid))
        PG_RETURN_FLOAT8((float8) selec);
    if (!statistic_proc_security_check(&vardata2, opfuncoid))
        PG_RETURN_FLOAT8((float8) selec);

    // -- RETRIVING HISTOGGRAMS -- //
    if (HeapTupleIsValid(vardata1.statsTuple) && HeapTupleIsValid(vardata2.statsTuple))
    {
        if (! ((get_attstatsslot(&sslot1, vardata1.statsTuple,
                                        STATISTIC_KIND_BOUNDS_HISTOGRAM,
                                        InvalidOid, ATTSTATSSLOT_VALUES)) 
            && (get_attstatsslot(&sslot2, vardata2.statsTuple,
                                        STATISTIC_KIND_BOUNDS_HISTOGRAM,
                                        InvalidOid, ATTSTATSSLOT_VALUES))
            && (get_attstatsslot(&freq_sslot1, vardata1.statsTuple,
                             STATISTIC_KIND_FREQUENCY_HISTOGRAM,
                             InvalidOid, ATTSTATSSLOT_VALUES))
            && (get_attstatsslot(&freq_sslot2, vardata2.statsTuple,
                             STATISTIC_KIND_FREQUENCY_HISTOGRAM,
                             InvalidOid, ATTSTATSSLOT_VALUES))))
        {
            ReleaseVariableStats(vardata1); ReleaseVariableStats(vardata2);
            pfree(sslot1); pfree(sslot2); pfree(freq_sslot1); pfree(freq_sslot2);
            PG_RETURN_FLOAT8((float8) selec);
        }
    }
    
    //////////////////////
	// BOUNDS HISTOGRAM // --> All we need are the sizes of each histogram and the min and max values of each.
	//////////////////////
    
    RangeBound r_min1, r_min2, r_max1, r_max2;
    int min1, min2, max1, max2;
    RangeBound buffer;

    // -- Retriving the sizes of each histogram. i.e. the number of rows. -- //
    nhist1 = sslot1.nvalues;
    nhist2 = sslot2.nvalues;
    
    // -- Deserializing ranges in order to obtain RangeBounds. -- // --> RangeBounds are ordered from lowest to highest.
    range_deserialize(typcache, DatumGetRangeTypeP(sslot1.values[0]), &r_min1, &buffer, &empty); if (empty) elog(ERROR, "bounds histogram contains an empty range");
    range_deserialize(typcache, DatumGetRangeTypeP(sslot1.values[nhist1-1]), &buffer, &r_max1, &empty); if (empty) elog(ERROR, "bounds histogram contains an empty range");
    range_deserialize(typcache, DatumGetRangeTypeP(sslot1.values[0]), &r_min2, &buffer, &empty); if (empty) elog(ERROR, "bounds histogram contains an empty range");
    range_deserialize(typcache, DatumGetRangeTypeP(sslot2.values[nhist2-1]), &buffer, &r_max2, &empty); if (empty) elog(ERROR, "bounds histogram contains an empty range");

    // -- Getting the integer values from range bounds -- //
    min1 = r_min1.val;
    min2 = r_min2.val;
    max1 = r_max1.val;
    max1 = r_max2.val;

    /////////////////////////
	// FREQUENCY HISTOGRAM //
	/////////////////////////

    // -- Retriving the sizes of each histogram. i.e. the number of intervals. -- //
    freq_nb_intervals1 = freq_sslot1.nvalues;
    freq_nb_intervals2 = freq_sslot2.nvalues;

    // -- Allocating memory for the frequency histograms. -- //
    freq_values1 = (float *) palloc(sizeof(float) * freq_nb_intervals1);
    freq_values2 = (float *) palloc(sizeof(float) * freq_nb_intervals2);

    // -- Getting the float values for our frequencies. -- //
    for (i = 0; i < freq_nb_intervals1; ++i)
        freq_values1[i] = DatumGetFloat8(freq_sslot1.values[i]);
    for (i = 0; i < freq_nb_intervals2; ++i)
        freq_values2[i] = DatumGetFloat8(freq_sslot2.values[i]);

    _debug_print_frequencies(freq_values1, freq_nb_intervals1);
    _debug_print_frequencies(freq_values2, freq_nb_intervals2);

    ///////////////////////////////////////
	// OVERLAP JOIN SELECTIVY ESTIMATION //
	///////////////////////////////////////

    selec = rangeoverlapsjoinsel_inner(freq_values1, freq_values2, freq_nb_intervals1, freq_nb_intervals2, nhist1, nhist2);

    // -- FREE -- //
    ReleaseVariableStats(vardata1);
    ReleaseVariableStats(vardata2);
    free_attstatsslot(&sslot1);
    free_attstatsslot(&sslot2);
    free_attstatsslot(&freq_sslot1);
    free_attstatsslot(&freq_sslot2);
    pfree(freq_values1);
    pfree(freq_values2);
    
    CLAMP_PROBABILITY(selec);
    
    PG_RETURN_FLOAT8((float8) selec); // szymon: actually returning just selectivity should work
}

Selectivity rangeoverlapsjoinsel_inner(float* freq_values1, float* freq_values2, int freq_nb_intervals1, int freq_nb_intervals2, int rows1, int rows2, int min1, int min2, int max1, int max2) {
    Selectivity selec;
    selec = 0.005; // temporary, fixme

    /*
    // Interval's length
    int length1; 
    int length2;
    
    length1 = roundUpDivision(max1 - min1, freq_nb_intervals1);
    length2 = roundUpDivision(max2 - min2, freq_nb_intervals2);
    
    // Indexes
    int idx1 = 0;
    int idx2 = 0;

    // Main
    if (min1 < min2) {
        for (in)
        // calculer 'j'
        // récupère min
    }
    j = 0
    for idx in nbr_interval:
        if ()

    */

    return selec;
}

static int 
roundUpDivision(int numerator, int divider){
    int div;
    div = numerator / divider;
	if (numerator % divider)
		div += 1;
	return div
}

static bool IsInRange(int challenge_low, int challenge_up, int low_bound, int up_bound)
{
	// A && B <=> NOT (A << B OR A >> B)
	return ! (challenge_up < low_bound || challenge_low > up_bound);
}

static void _debug_print_frequencies(float8* frequencies_vals, int size)
{
	printf("Frequencies = [");
    for (i = 0; i < size; i++){
        printf("%f", (frequencies_vals[i]));
        if (i < size - 1)
        	printf(", ");
    }
    printf("]\n");
    fflush(stdout);
}




/*
 * Selectivity for operators that depend on area, such as "overlap".
 */
Datum
areasel(PG_FUNCTION_ARGS) //alex: on s'en fout de celui ci
{
	PG_RETURN_FLOAT8(0.005);
}


Datum
areajoinsel(PG_FUNCTION_ARGS) // szymon: on veut celui-ci
{
	PG_RETURN_FLOAT8(0.005);
}


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
