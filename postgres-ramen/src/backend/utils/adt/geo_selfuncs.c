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


double rangeoverlapsjoinsel_inner(float* freq_values1, float* freq_values2, int freq_nb_intervals1, int freq_nb_intervals2);


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

    double      selec = 0.005;

    VariableStatData vardata1;
    VariableStatData vardata2;
    Oid         opfuncoid;
    AttStatsSlot sslot1;
    AttStatsSlot sslot2;
    int         nhist1;
    int         nhist2;
    RangeBound* hist_lower1;
    RangeBound* hist_upper1;
    RangeBound* hist_lower2;
    RangeBound* hist_upper2;
    int         i;
    Form_pg_statistic stats1 = NULL;
    Form_pg_statistic stats2 = NULL;
    TypeCacheEntry* typcache = NULL;
    bool        join_is_reversed; // TODO Pas ENCORE utilisé, ici, mais potentiellement important. 
    bool        empty;

    AttStatsSlot freq_sslot1;
    AttStatsSlot freq_sslot2;
    int         freq_nb_intervals1;
    int         freq_nb_intervals2;
    float*        freq_values1;
    float*        freq_values2;


    get_join_variables(root, args, sjinfo,
                       &vardata1, &vardata2, &join_is_reversed);

    typcache = range_get_typcache(fcinfo, vardata1.vartype);
    opfuncoid = get_opcode(operator);

    memset(&sslot1, 0, sizeof(sslot1));
    memset(&sslot2, 0, sizeof(sslot2));

    /* Can't use the histogram with insecure range support functions */
    if (!statistic_proc_security_check(&vardata1, opfuncoid))
        PG_RETURN_FLOAT8((float8) selec);
    if (!statistic_proc_security_check(&vardata2, opfuncoid))
        PG_RETURN_FLOAT8((float8) selec);

    if (HeapTupleIsValid(vardata1.statsTuple))
    {
        stats1 = (Form_pg_statistic) GETSTRUCT(vardata1.statsTuple);
        /* Try to get fraction of empty ranges */
        if (!get_attstatsslot(&sslot1, vardata1.statsTuple,
                             STATISTIC_KIND_BOUNDS_HISTOGRAM,
                             InvalidOid, ATTSTATSSLOT_VALUES))
        {
            ReleaseVariableStats(vardata1);
            ReleaseVariableStats(vardata2);
            PG_RETURN_FLOAT8((float8) selec);
        }
    }

    if (HeapTupleIsValid(vardata2.statsTuple))
    {
        stats2 = (Form_pg_statistic) GETSTRUCT(vardata2.statsTuple);
        /* Try to get fraction of empty ranges */
        if (!get_attstatsslot(&sslot2, vardata2.statsTuple,
                             STATISTIC_KIND_BOUNDS_HISTOGRAM,
                             InvalidOid, ATTSTATSSLOT_VALUES))
        {
            ReleaseVariableStats(vardata1);
            ReleaseVariableStats(vardata2);
            PG_RETURN_FLOAT8((float8) selec);
        }
    }

    nhist1 = sslot1.nvalues;
    nhist2 = sslot2.nvalues;

    hist_lower1 = (RangeBound *) palloc(sizeof(RangeBound) * nhist1);
    hist_upper1 = (RangeBound *) palloc(sizeof(RangeBound) * nhist1);

    hist_lower2 = (RangeBound *) palloc(sizeof(RangeBound) * nhist2);
    hist_upper2 = (RangeBound *) palloc(sizeof(RangeBound) * nhist2);

    for (i = 0; i < nhist1; i++){
        range_deserialize(typcache, DatumGetRangeTypeP(sslot1.values[i]),
                          &hist_lower1[i], &hist_upper1[i], &empty);
        /* The histogram should not contain any empty ranges */
        if (empty)
            elog(ERROR, "bounds histogram contains an empty range");
    }

    for (i = 0; i < nhist2; i++){
        range_deserialize(typcache, DatumGetRangeTypeP(sslot2.values[i]),
                          &hist_lower2[i], &hist_upper2[i], &empty);
        /* The histogram should not contain any empty ranges */
        if (empty)
            elog(ERROR, "bounds histogram contains an empty range");
    }

    printf("is_reversed %d\n", join_is_reversed);
    printf("hist_lower1 = [");
    for (i = 0; i < nhist1; i++){
        printf("%d", DatumGetInt16((hist_lower1+i)->val));
        if (i < nhist1 - 1)
            printf(", ");
    }
    printf("]\n");
    printf("hist_upper1 = [");
    for (i = 0; i < nhist1; i++){
        printf("%d", DatumGetInt16((hist_upper1+i)->val));
        if (i < nhist1 - 1)
            printf(", ");
    }
    printf("]\n");
    fflush(stdout);

    printf("hist_lower2 = [");
    for (i = 0; i < nhist2; i++){
        printf("%d", DatumGetInt16((hist_lower2+i)->val));
        if (i < nhist2 - 1)
            printf(", ");
    }
    printf("]\n");
    printf("hist_upper2 = [");
    for (i = 0; i < nhist2; i++){
        printf("%d", DatumGetInt16((hist_upper2+i)->val));
        if (i < nhist2 - 1)
            printf(", ");
    }
    printf("]\n");
    fflush(stdout);


    // ----- FREQUENCY HISTOGRAM ----- //
    memset(&freq_sslot1, 0, sizeof(freq_sslot1));
    memset(&freq_sslot2, 0, sizeof(freq_sslot2));

    if (HeapTupleIsValid(vardata1.statsTuple))
    {
        // stats1 = (Form_pg_statistic) GETSTRUCT(vardata1.statsTuple);
        if (!get_attstatsslot(&freq_sslot1, vardata1.statsTuple,
                             STATISTIC_KIND_FREQUENCY_HISTOGRAM,
                             InvalidOid, ATTSTATSSLOT_VALUES))
        {
            ReleaseVariableStats(vardata1);
            ReleaseVariableStats(vardata2);
            PG_RETURN_FLOAT8((float8) selec);
        }
    }
    if (HeapTupleIsValid(vardata2.statsTuple))
    {
        // stats2 = (Form_pg_statistic) GETSTRUCT(vardata2.statsTuple);
        if (!get_attstatsslot(&freq_sslot2, vardata2.statsTuple,
                             STATISTIC_KIND_FREQUENCY_HISTOGRAM,
                             InvalidOid, ATTSTATSSLOT_VALUES))
        {
            ReleaseVariableStats(vardata1);
            ReleaseVariableStats(vardata2);
            PG_RETURN_FLOAT8((float8) selec);
        }
    }

    freq_nb_intervals1 = freq_sslot1.nvalues;
    freq_nb_intervals2 = freq_sslot2.nvalues;

    freq_values1 = (float *) palloc(sizeof(float) * freq_nb_intervals1);
    freq_values2 = (float *) palloc(sizeof(float) * freq_nb_intervals2);

   printf("\n*****************\n");

    for (i = 0; i < freq_nb_intervals1; ++i){
        freq_values1[i] = DatumGetFloat8(freq_sslot1.values[i]);
    }
    for (i = 0; i < freq_nb_intervals2; ++i){
        freq_values2[i] = DatumGetFloat8(freq_sslot2.values[i]);
    }


    // Debug print:
    printf("SUUUUUUUUUUUUSHHHI:\n");
    printf("Intervals 1:\n");
	printf("frequencies = [");
    for (i = 0; i < freq_nb_intervals1; i++){
        printf("%f", (freq_values1[i]));
        if (i < freq_nb_intervals1 - 1)
        	printf(", ");
    } 
    printf("]\n");

    // Debug print2:
    printf("Intervals 2:\n");
	printf("frequencies = [");
    for (i = 0; i < freq_nb_intervals2; i++){
        printf("%f", (freq_values2[i]));
        if (i < freq_nb_intervals2 - 1)
        	printf(", ");
    } 
    printf("]\n");
    fflush(stdout);
    

    // ----- FREQUENCY HISTOGRAM ----- //


	//SZYMON: TODO ALGO POUR JOIN SELECTIVITY ICI :3
    selec = rangeoverlapsjoinsel_inner(freq_values1, freq_values2, freq_nb_intervals1, freq_nb_intervals2);



    // -- FREE -- //
    pfree(hist_lower1);
    pfree(hist_upper1);
    pfree(hist_lower2);
    pfree(hist_upper2);

    free_attstatsslot(&sslot1);
    free_attstatsslot(&sslot2);

    ReleaseVariableStats(vardata1);
    ReleaseVariableStats(vardata2);

    // -- FREQUENCY FREE -- //
    pfree(freq_values1);
    pfree(freq_values2);
    free_attstatsslot(&freq_sslot1);
    free_attstatsslot(&freq_sslot2);
    
    CLAMP_PROBABILITY(selec);
    PG_RETURN_FLOAT8((float8) selec); // VALUE BETWEEN 0 AND 1.
}


double rangeoverlapsjoinsel_inner(float* freq_values1, float* freq_values2, int freq_nb_intervals1, int freq_nb_intervals2) {
    
    /* TODO */
    
    return 0.005; //FIXME TEMP (return selectivity)
}


//szymon : osef de ce qu'il y a en dessous


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


// Szymon: copy-pasted here, may be useful to how stats are defined.
// (from src/include/commands/vacuum.h)
typedef struct SushiStats//VacAttrStats
{
	/*
	 * These fields are set up by the main ANALYZE code before invoking the
	 * type-specific typanalyze function.
	 *
	 * Note: do not assume that the data being analyzed has the same datatype
	 * shown in attr, ie do not trust attr->atttypid, attlen, etc.  This is
	 * because some index opclasses store a different type than the underlying
	 * column/expression.  Instead use attrtypid, attrtypmod, and attrtype for
	 * information about the datatype being fed to the typanalyze function.
	 * Likewise, use attrcollid not attr->attcollation.
	 */
	Form_pg_attribute attr;		/* copy of pg_attribute row for column */
	Oid			attrtypid;		/* type of data being analyzed */
	int32		attrtypmod;		/* typmod of data being analyzed */
	Form_pg_type attrtype;		/* copy of pg_type row for attrtypid */
	Oid			attrcollid;		/* collation of data being analyzed */
	MemoryContext anl_context;	/* where to save long-lived data */

	/*
	 * These fields must be filled in by the typanalyze routine, unless it
	 * returns false.
	 */
	AnalyzeAttrComputeStatsFunc compute_stats;	/* function pointer */
	int			minrows;		/* Minimum # of rows wanted for stats */
	void	   *extra_data;		/* for extra type-specific data */

	/*
	 * These fields are to be filled in by the compute_stats routine. (They
	 * are initialized to zero when the struct is created.)
	 */
	bool		stats_valid;
	float4		stanullfrac;	/* fraction of entries that are NULL */
	int32		stawidth;		/* average width of column values */
	float4		stadistinct;	/* # distinct values */
	int16		stakind[STATISTIC_NUM_SLOTS];
	Oid			staop[STATISTIC_NUM_SLOTS];
	Oid			stacoll[STATISTIC_NUM_SLOTS];
	int			numnumbers[STATISTIC_NUM_SLOTS];
	float4	   *stanumbers[STATISTIC_NUM_SLOTS];
	int			numvalues[STATISTIC_NUM_SLOTS];
	Datum	   *stavalues[STATISTIC_NUM_SLOTS];

	/*
	 * These fields describe the stavalues[n] element types. They will be
	 * initialized to match attrtypid, but a custom typanalyze function might
	 * want to store an array of something other than the analyzed column's
	 * elements. It should then overwrite these fields.
	 */
	Oid			statypid[STATISTIC_NUM_SLOTS];
	int16		statyplen[STATISTIC_NUM_SLOTS];
	bool		statypbyval[STATISTIC_NUM_SLOTS];
	char		statypalign[STATISTIC_NUM_SLOTS];

	/*
	 * These fields are private to the main ANALYZE code and should not be
	 * looked at by type-specific functions.
	 */
	int			tupattnum;		/* attribute number within tuples */
	HeapTuple  *rows;			/* access info for std fetch function */
	TupleDesc	tupDesc;
	Datum	   *exprvals;		/* access info for index fetch function */
	bool	   *exprnulls;
	int			rowstride;
} SushiStats;
