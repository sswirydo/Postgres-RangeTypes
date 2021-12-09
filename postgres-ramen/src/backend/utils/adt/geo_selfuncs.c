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
static int roundUpDivision(int numerator, int divider);
float8 rangeoverlapsjoinsel_inner(float8* freq_values1, float8* freq_values2, int freq_nb_intervals1, int freq_nb_intervals2, int rows1, int rows2, int min1, int min2, int max1, int max2);
float8 computeSelectivity(float8* trunc_freq1, float8* trunc_freq2, int size1, int size2, int interval_length1, int interval_length2, int min1, int min2);
static bool IsInRange(int challenge_low, int challenge_up, int low_bound, int up_bound);
static void _debug_print_frequencies(float8* frequencies_vals, int size);
// ------------------------- //


/*
 * Range Overlaps Join Selectivity.
 */
Datum rangeoverlapsjoinsel(PG_FUNCTION_ARGS)
{
	// FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);
    
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
    float8*        freq_values1 = NULL;
    float8*        freq_values2 = NULL;
    float8 selec = 0.005;

    ///////////
	// INITs //
	///////////

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
            free_attstatsslot(&sslot1); free_attstatsslot(&sslot2); free_attstatsslot(&freq_sslot1); free_attstatsslot(&freq_sslot2);
            PG_RETURN_FLOAT8(selec);
        }
    }
    else {
        ReleaseVariableStats(vardata1); ReleaseVariableStats(vardata2);
        free_attstatsslot(&sslot1); free_attstatsslot(&sslot2); free_attstatsslot(&freq_sslot1); free_attstatsslot(&freq_sslot2);
        PG_RETURN_FLOAT8(selec);
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
    range_deserialize(typcache, DatumGetRangeTypeP(sslot2.values[0]), &r_min2, &buffer, &empty); if (empty) elog(ERROR, "bounds histogram contains an empty range");
    range_deserialize(typcache, DatumGetRangeTypeP(sslot2.values[nhist2-1]), &buffer, &r_max2, &empty); if (empty) elog(ERROR, "bounds histogram contains an empty range");

    // -- Getting the integer values from range bounds -- //
    min1 = r_min1.val;
    min2 = r_min2.val;
    max1 = r_max1.val;
    max2 = r_max2.val;

    /////////////////////////
	// FREQUENCY HISTOGRAM //
	/////////////////////////

    // -- Retriving the sizes of each histogram. i.e. the number of intervals. -- //
    freq_nb_intervals1 = freq_sslot1.nvalues;
    freq_nb_intervals2 = freq_sslot2.nvalues;

    // -- Allocating memory for the frequency histograms. -- //

    freq_values1 = (float8 *) palloc(sizeof(float8) * freq_nb_intervals1);
    freq_values2 = (float8 *) palloc(sizeof(float8) * freq_nb_intervals2);

    // -- Getting the float values for our frequencies. -- //
    for (i = 0; i < freq_nb_intervals1; ++i)
        freq_values1[i] = DatumGetFloat8(freq_sslot1.values[i]);
    for (i = 0; i < freq_nb_intervals2; ++i)
        freq_values2[i] = DatumGetFloat8(freq_sslot2.values[i]);

    // _debug_print_frequencies(freq_values1, freq_nb_intervals1);
    // _debug_print_frequencies(freq_values2, freq_nb_intervals2);

    ///////////////////////////////////////
	// OVERLAP JOIN SELECTIVY ESTIMATION //
	///////////////////////////////////////

    if (IsInRange(min1, max1, min2, max2)){
        selec = rangeoverlapsjoinsel_inner(freq_values1, freq_values2, freq_nb_intervals1, freq_nb_intervals2, nhist1, nhist2, min1, min2, max1, max2);
    }

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

    fflush(stdout);

    PG_RETURN_FLOAT8(selec);
}

float8 rangeoverlapsjoinsel_inner(float8* freq_values1, float8* freq_values2, int freq_nb_intervals1, int freq_nb_intervals2, int rows1, int rows2, int min1, int min2, int max1, int max2)
{
    float8 result = 0.005;
    
    int interval_length1 = roundUpDivision(max1 - min1, freq_nb_intervals1);
    int interval_length2 = roundUpDivision(max2 - min2, freq_nb_intervals2);

    
    int x_low = 0;
    int y_low = 0;
    int x_high = freq_nb_intervals1 - 1;
    int y_high = freq_nb_intervals2 - 1;
    int min_val1 = min1;
    int min_val2 = min2;
    int max_val1 = max1;
    int max_val2 = max2;
    int stop = 0;
    int i = 0;
    int new_size1;
    int new_size2;

    // -- LOW -- //
    while (! IsInRange(min_val1, min_val1 + interval_length1, min_val2, min_val2 + interval_length2)) {
        if (min_val1 < min_val2){
            min_val1 += interval_length1;
            ++x_low;
        }
        else if (min_val2 < min_val1){
            min_val2 += interval_length2;
            ++y_low;
        }
        else {printf("???"); ++x_low;  ++y_low;}

        if (x_low == freq_nb_intervals1 | y_low == freq_nb_intervals2) {
            printf(">>> STOP LOW.\n");
            stop++;
            break;
        }
    }
    
    // -- HIGH -- //
    while (! IsInRange(max_val1 - interval_length1, max_val1, max_val2 - interval_length2, max_val2)) {
        if (max_val1 > max_val2){
            max_val1 -= interval_length1;
            --x_high;
        }
        else if (max_val2 > max_val1){
            max_val2 -= interval_length2;
            --y_high;
        }
        else {printf("???"); --x_high; --y_high;}

        if (x_high == 0 | y_high == 0) {
            printf(">>> STOP HIGH.\n");
            stop++;
            break;
        }
    }

    if (! stop)
        result = computeSelectivity(freq_values1 + x_low, freq_values2 + y_low, (x_high - x_low) + 1, (y_high - y_low) + 1, interval_length1, interval_length2, min_val1, min_val2);

    return result;
}

//alex: on tronque les tables lorsqu'on a les bornes mins et max comme ca on part de 0 pour les 2 on a alors juste:
//static Selectivity calculateSelectivity(float8* trunc_freq1, float8* trunc_freq2, int size1, int size2)
//on les parcourt alors de 0 jusqu'à max(size1,size2) et on applique l'algo en bas 
//sinon lorsque valeures trop différentes problème pour la limit
//O(max(size1,size2))
float8 computeSelectivity(float8* trunc_freq1, float8* trunc_freq2, int size1, int size2, int interval_length1, int interval_length2, int min1, int min2){
    int idx_1 = 0;
    int idx_2 = 0;
    
    float8 total = 0;
    
    while(idx_1 < size1 && idx_2 < size2){
    	total += trunc_freq1[idx_1] * trunc_freq2[idx_2];
    	if(min1 < min2){
    		min1 += interval_length1;
    		idx_1++;
    	}
    	else if(min1 > min2){
    		min2 += interval_length2;
    		idx_2++;
    	}
    	else{
    		min1 += interval_length1;
    		idx_1++;
    		min2 += interval_length2;
    		idx_2++;
    	}
    } 

    return total; //TODO normalize result
}

static int roundUpDivision(int numerator, int divider){
    int div;
    div = numerator / divider;
	if (numerator % divider)
		++div;
	return div;
}

static bool IsInRange(int challenge_low, int challenge_up, int low_bound, int up_bound)
{
	// A && B <=> NOT (A << B OR A >> B)
	return ! (challenge_up < low_bound || challenge_low > up_bound);
}

static void _debug_print_frequencies(float8* frequencies_vals, int size)
{
	printf("Frequencies = [");
    for (int i = 0; i < size; i++){
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
