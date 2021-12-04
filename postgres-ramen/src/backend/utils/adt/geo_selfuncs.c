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

    double      selec = 0.005; // nb_result / nb_result_max

    VariableStatData vardata1;
    VariableStatData vardata2;
    Oid         opfuncoid;
    AttStatsSlot sslot1;
    AttStatsSlot sslot2;
    int         nhist1;
    int         nhist2;
    RangeBound *hist_lower1;
    RangeBound *hist_upper1;
    RangeBound *hist_lower2;
    RangeBound *hist_upper2;
    int         i;
    Form_pg_statistic stats1 = NULL;
    Form_pg_statistic stats2 = NULL;
    TypeCacheEntry *typcache = NULL;
    bool        join_is_reversed; // Pas ENCORE utilisé, ici, mais potentiellement important. 
    bool        empty;


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


    //RECUPERER LE FREQUENCY ICI
    /* ... */


	//SZYMON: EN GROS TODO ALGO POUR JOIN SELECTIVITY ICI :3
    rangeoverlapsjoinsel_inner();

    pfree(hist_lower1);
    pfree(hist_upper1);
    pfree(hist_lower2);
    pfree(hist_upper2);

    free_attstatsslot(&sslot1);
    free_attstatsslot(&sslot2);

    ReleaseVariableStats(vardata1);
    ReleaseVariableStats(vardata2);

    // selec=0.5;
    CLAMP_PROBABILITY(selec);
    PG_RETURN_FLOAT8((float8) selec); //compris entre 0 et 1
}



int rangeoverlapsjoinsel_inner() {
    /* TODO */
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
