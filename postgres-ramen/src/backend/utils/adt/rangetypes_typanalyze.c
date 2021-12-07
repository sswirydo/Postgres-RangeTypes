/*-------------------------------------------------------------------------
 *
 * rangetypes_typanalyze.c
 *	  Functions for gathering statistics from range columns
 *
 * For a range type column, histograms of lower and upper bounds, and
 * the fraction of NULL and empty ranges are collected.
 *
 * Both histograms have the same length, and they are combined into a
 * single array of ranges. This has the same shape as the histogram that
 * std_typanalyze would collect, but the values are different. Each range
 * in the array is a valid range, even though the lower and upper bounds
 * come from different tuples. In theory, the standard scalar selectivity
 * functions could be used with the combined histogram.
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/rangetypes_typanalyze.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_operator.h"
#include "commands/vacuum.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"

static int	float8_qsort_cmp(const void *a1, const void *a2);
static int	range_bound_qsort_cmp(const void *a1, const void *a2, void *arg);
static void compute_range_stats(VacAttrStats *stats,
								AnalyzeAttrFetchFunc fetchfunc, int samplerows); //, double totalrows);

/*
 * range_typanalyze -- typanalyze function for range columns
 */
Datum
range_typanalyze(PG_FUNCTION_ARGS)
{
	FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, "1"); fclose(file);
	file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, "2"); fclose(file);

	VacAttrStats *stats = (VacAttrStats *) PG_GETARG_POINTER(0);
	TypeCacheEntry *typcache;
	Form_pg_attribute attr = stats->attr;

	/* Get information about range type; note column might be a domain */
	typcache = range_get_typcache(fcinfo, getBaseType(stats->attrtypid));

	if (attr->attstattarget < 0) // szymon: if empty
		attr->attstattarget = default_statistics_target;

	stats->compute_stats = compute_range_stats;
	stats->extra_data = typcache;
	/* same as in std_typanalyze */
	stats->minrows = 300 * attr->attstattarget; //alex: pq 300 fois ? 

	PG_RETURN_BOOL(true);
}

/*
 * Comparison function for sorting float8s, used for range lengths.
 */
static int
float8_qsort_cmp(const void *a1, const void *a2)
{

	const float8 *f1 = (const float8 *) a1;
	const float8 *f2 = (const float8 *) a2;

	if (*f1 < *f2)
		return -1;
	else if (*f1 == *f2)
		return 0;
	else
		return 1;
}

/*
 * Comparison function for sorting RangeBounds.
 */
static int
range_bound_qsort_cmp(const void *a1, const void *a2, void *arg)
{

	RangeBound *b1 = (RangeBound *) a1;
	RangeBound *b2 = (RangeBound *) a2;
	TypeCacheEntry *typcache = (TypeCacheEntry *) arg;

	return range_cmp_bounds(typcache, b1, b2);
}


static bool IsInRange(int challenge_low, int challenge_up, int low_bound, int up_bound)
{
	// A && B <=> NOT (A << B OR A >> B)
	return ! (challenge_up < low_bound || challenge_low > up_bound);
}


static void
ComputeFrequencyHistogram(VacAttrStats* stats, int slot_idx, RangeBound* lowers, RangeBound* uppers, int rows) {
	// idée principale :
	// 		on crée genre une liste abstraite qui va de la plus petite valeur à la plus grande valeur de ma colonne de ranges
	// 		on divise cette liste en intervalles réguliers selon par ex la taille de cette liste (ou autre)
	// 		on calcule le nombre de fois qu'une range pop dans un intervalle de la liste


	//for aurelien & alex : x++ and ++X in forloop is the same. :)
	//for (int x = 0; x < 5; x++) {printf("x: %d\n", x);}
	//for (int y = 0; y < 5; ++y) {printf("y: %d\n", y);}
	
	
	// TODO à féfléchir : Fixe le nbr d'intevral ou la longueur des intervals ?
	int PERCENT_INTERVAL_LENGTH = 5; //FIXME arbitraire value : longueur interval = 5% de longueur tot
		
	int i;
	int j;
	int min;
	int max;

	int length;
	int interval_length;
	int nb_of_intervals;

	int* frequencies_vals;
	int* frequencies_intervals;
	TypeCacheEntry *typcache = (TypeCacheEntry *) stats->extra_data;
	
	// VERSION AURE (lower et upper sont triés)
	min = (lowers)->val;
	max = (uppers+rows-1)->val;


	/// --- TODO TODO TODO --- //
	length = max - min;
	interval_length = PERCENT_INTERVAL_LENGTH * length;
	if (interval_length%100 == 0) { interval_length = interval_length/100; }
	else {interval_length = 1 + interval_length/100; } // (+1 pour l'arrondi haut)
	nb_of_intervals = length/interval_length;
	if (length % interval_length != 0) { 
		nb_of_intervals++; // Arrondir au supérieur
	}
	// vérifier des trucs comme que la longueur d'intervale soit naturelle
	//  + pas trop petite car trop de stockage et trop de temps
	//  + ni trop grande sinon estimation sera bien trop grande
	/// --- TODO TODO TODO --- //

	frequencies_vals = (int*) palloc(sizeof(int) * nb_of_intervals);
	frequencies_intervals = (int*) palloc(sizeof(int) * nb_of_intervals);
	memset(frequencies_vals, 0, sizeof(int) * nb_of_intervals);
	memset(frequencies_intervals, 0, sizeof(int) * nb_of_intervals);

	//// TODO Equidepth ou pas ?
	
	///////////////////////
	// CLASSIC HISTOGRAM //
	///////////////////////
	
	// Version Aure AKA Compiling 
	
	int sum_hist = 0; // used after for normalization
	int l = 0;
	int u = 0;
	int count = 0;
	int sup = min-1;

	for (i = 0; i < nb_of_intervals; ++i){
		sup += interval_length;
		frequencies_intervals[i] = sup;
		while((int)(lowers+l)->val <= sup && l < rows){
			count++;
			l++;
		}
		frequencies_vals[i] = count;
		sum_hist += count;
		while((int)(uppers+u)->val <= sup+1 && u < rows){
			count--;
			u++;
		}
	}

	// Version Szymon AKA First // PETIT TRUCS A CORRIGER
	/*
	for (i = 0; i < nb_of_intervals; ++i){
		frequencies_intervals[i] = (i+1)*interval_length; // ERROR -> FAUX, supposer que le premier interval commence à 0 (hors c'est un cas particulier)
		for (j = 0; j < rows; ++j){
			if (IsInRange(i*interval_length+1, ((i+1)*interval_length), (lowers+j)->val, (uppers+j)->val)) // range size 0 possible ? genre range(3,3) //btw [Ø,Ø] is empty
				++(frequencies_vals[i]); 
				sum_hist++;
		}
	}
	*/

	// Debug print.
	/*
	printf("Intervals:\n");
	for (i = 0; i < nb_of_intervals; ++i){
		printf("[%d : %d]", frequencies_intervals[i]-interval_length+1, frequencies_intervals[i]);
	}
	printf("\nfrequencies = [");
    	for (i = 0; i < nb_of_intervals; i++){
        	printf("%d", (frequencies_vals[i]));
        	if (i < nb_of_intervals - 1)
        		printf(", ");
    	} 
    	printf("]\n");
    	// printf("TOTAL COUNT : %d\n", sum_hist);
	fflush(stdout);
	*/
	
	/////////////////////////////
	// HISTOGRAM NORMALIZATION //
	/////////////////////////////
	
	float ratio;
	ratio = ((float) rows) / ((float) sum_hist); // divided by "sum_hist" (= percentage) -> multiply by "rows" (weighted)
	
	// Debug print. (aure)
	/*
	printf("DEBUG:\n");
	for (i = 0; i < nb_of_intervals; ++i){
		printf("[%d : %d] %d elem \n", frequencies_intervals[i]-interval_length+1, 			frequencies_intervals[i], frequencies_vals[i]);
	} 
	printf("SIZE : %d / %d = %f\n\n", rows, sum_hist, ratio);
	fflush(stdout);
	*/
	
	float8* normalized_frequencies_vals;
	normalized_frequencies_vals = (float8*) palloc(sizeof(float8) * nb_of_intervals);
	
	// float debug = 0;
	for (i = 0; i < nb_of_intervals; ++i){
		
		//printf("---BEFORE: %d\n", frequencies_vals[i]);
		//debug += ((float) frequencies_vals[i]) * ratio;
		normalized_frequencies_vals[i] = (float8) ((float) frequencies_vals[i]) * ratio;
		//printf("---AFTER: %f\n", normalized_frequencies_vals[i]);

	}
	//printf("DGB : %f\n\n", debug); -> doit étre égale à 'rows' (à une erreur d'approx près, float)
	//fflush(stdout);

	Datum* hist_frequencies_vals = (Datum *) palloc(sizeof(Datum) * nb_of_intervals);
	for (i = 0; i < nb_of_intervals; ++i) {
		hist_frequencies_vals[i] = Float8GetDatum(normalized_frequencies_vals[i]);
	}

	stats->staop[slot_idx] = Float8LessOperator;
	stats->stacoll[slot_idx] = InvalidOid;
	stats->stavalues[slot_idx] = hist_frequencies_vals;
	stats->numvalues[slot_idx] = nb_of_intervals;
	stats->statypid[slot_idx] = FLOAT8OID;
	stats->statyplen[slot_idx] = sizeof(float8);
	stats->statypbyval[slot_idx] = FLOAT8PASSBYVAL;
	stats->statypalign[slot_idx] = 'd';
	//Store the fraction of empty ranges
	//emptyfrac = (float4 *) palloc(sizeof(float4));
	//*emptyfrac = ((double) empty_cnt) / ((double) non_null_cnt);
	//stats->stanumbers[slot_idx] = emptyfrac;
	//stats->numnumbers[slot_idx] = 1;
	stats->stakind[slot_idx] = STATISTIC_KIND_FREQUENCY_HISTOGRAM;

	// --- (THIS IS FOR THE LENGTH HISTOGRAM) --- //
	/*
	stats->staop[slot_idx] = Float8LessOperator;
	stats->stacoll[slot_idx] = InvalidOid;
	stats->stavalues[slot_idx] = frequencies_intervals;
	stats->numvalues[slot_idx] = nb_of_intervals;
	stats->statypid[slot_idx] = FLOAT8OID;
	stats->statyplen[slot_idx] = sizeof(float8);
	stats->statypbyval[slot_idx] = FLOAT8PASSBYVAL;
	stats->statypalign[slot_idx] = 'd';
	//Store the fraction of empty ranges
	//emptyfrac = (float4 *) palloc(sizeof(float4));
	//*emptyfrac = ((double) empty_cnt) / ((double) non_null_cnt);
	//stats->stanumbers[slot_idx] = emptyfrac;
	//stats->numnumbers[slot_idx] = 1;
	stats->stakind[slot_idx] = STATISTIC_KIND_FREQUENCY_HISTOGRAM_LENGTH;
	*/
	

	pfree(frequencies_vals);
	pfree(normalized_frequencies_vals);
}

/*
 * compute_range_stats() -- compute statistics (if not empty) for (a) range(s) column(s)
 */
static void
compute_range_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc,
					int samplerows)
{
	FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);

	TypeCacheEntry *typcache = (TypeCacheEntry *) stats->extra_data;
	bool		has_subdiff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);
	int			null_cnt = 0;
	int			non_null_cnt = 0;
	int			non_empty_cnt = 0;
	int			empty_cnt = 0;
	int			range_no;
	int			slot_idx;
	int			num_bins = stats->attr->attstattarget;
	int			num_hist;
	float8	   *lengths;
	RangeBound *lowers,
			   *uppers;
	double		total_width = 0;

	/* Allocate memory to hold range bounds and lengths of the sample ranges. */
	lowers = (RangeBound *) palloc(sizeof(RangeBound) * samplerows);
	uppers = (RangeBound *) palloc(sizeof(RangeBound) * samplerows);
	lengths = (float8 *) palloc(sizeof(float8) * samplerows);


	/* Loop over the sample ranges. */
	for (range_no = 0; range_no < samplerows; range_no++)
	{
		Datum		value;
		bool		isnull,
					empty;
		RangeType  *range;
		RangeBound	lower,
					upper;
		float8		length;

		vacuum_delay_point();

		// check ce qu'on a est correct ou si y a des trucs
		value = fetchfunc(stats, range_no, &isnull); // dedans ça met isnull=true soir isnull=false
		if (isnull)
		{
			/* range is null, just count that */
			null_cnt++;
			continue;
		}

		/*
		 * XXX: should we ignore wide values, like std_typanalyze does, to
		 * avoid bloating the statistics table?
		 */
		total_width += VARSIZE_ANY(DatumGetPointer(value));

		/* Get range and deserialize it for further analysis. */
		range = DatumGetRangeTypeP(value);
		range_deserialize(typcache, range, &lower, &upper, &empty);
		

		//check si le deserialize est ok.
		if (!empty)
		{
			/* Remember bounds and length for further usage in histograms */
			lowers[non_empty_cnt] = lower;
			uppers[non_empty_cnt] = upper;


			// -- szymon: cette partie on calcule la longueur par rapport aux lower and upper bounds -- //
			if (lower.infinite || upper.infinite) // szymon: si range infinie, length infinie
			{
				/* Length of any kind of an infinite range is infinite */
				length = get_float8_infinity();
			}
			else if (has_subdiff)
			{
				/*
				 * For an ordinary range, use subdiff function between upper
				 * and lower bound values.
				 */
				length = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo,
														  typcache->rng_collation,
														  upper.val, lower.val));
			}
			else
			{
				/* Use default value of 1.0 if no subdiff is available. */
				length = 1.0;
			}
			lengths[non_empty_cnt] = length;

			non_empty_cnt++;
		}
		else
			empty_cnt++; // rip

		non_null_cnt++; // nombre de range qu'on a deserialise correctement
	}

	slot_idx = 0;

	// histo crée àpd de la sur base de range désarialisé avant

	/* We can only compute real stats if we found some non-null values. */
	if (non_null_cnt > 0)
	{
		fflush(stdout);
		Datum	   *bound_hist_values;
		Datum	   *length_hist_values;
		int			pos,
					posfrac,
					delta,
					deltafrac,
					i;
		MemoryContext old_cxt;
		float4	   *emptyfrac;

		stats->stats_valid = true;
		/* Do the simple null-frac and width stats */
		stats->stanullfrac = (double) null_cnt / (double) samplerows;
		stats->stawidth = total_width / (double) non_null_cnt;

		/* Estimate that non-null values are unique */
		stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);

		/* Must copy the target values into anl_context */
		old_cxt = MemoryContextSwitchTo(stats->anl_context);

		/*
		 * Generate a bounds histogram slot entry if there are at least two
		 * values.
		 */
		if (non_empty_cnt >= 2)
		{
			/* Sort bound values */
			qsort_arg(lowers, non_empty_cnt, sizeof(RangeBound),
					  range_bound_qsort_cmp, typcache);
			qsort_arg(uppers, non_empty_cnt, sizeof(RangeBound),
					  range_bound_qsort_cmp, typcache);

			num_hist = non_empty_cnt;
			if (num_hist > num_bins)
				num_hist = num_bins + 1;

			bound_hist_values = (Datum *) palloc(num_hist * sizeof(Datum));

			/*
			 * The object of this loop is to construct ranges from first and
			 * last entries in lowers[] and uppers[] along with evenly-spaced
			 * values in between. So the i'th value is a range of lowers[(i *
			 * (nvals - 1)) / (num_hist - 1)] and uppers[(i * (nvals - 1)) /
			 * (num_hist - 1)]. But computing that subscript directly risks
			 * integer overflow when the stats target is more than a couple
			 * thousand.  Instead we add (nvals - 1) / (num_hist - 1) to pos
			 * at each step, tracking the integral and fractional parts of the
			 * sum separately.
			 */
			delta = (non_empty_cnt - 1) / (num_hist - 1);
			deltafrac = (non_empty_cnt - 1) % (num_hist - 1);
			pos = posfrac = 0;

			for (i = 0; i < num_hist; i++) // num_hist: number of row
			{
				// szymon: c'est ici qu'on combine les lower & upper histogrammes en 1
				bound_hist_values[i] = PointerGetDatum(range_serialize(typcache,
																	   &lowers[pos],
																	   &uppers[pos],
																	   false));
				pos += delta;
				posfrac += deltafrac;
				if (posfrac >= (num_hist - 1))
				{
					/* fractional part exceeds 1, carry to integer part */
					pos++;
					posfrac -= (num_hist - 1);
				}
			}

			// alex: c'est qui nous intéresse
			stats->stakind[slot_idx] = STATISTIC_KIND_BOUNDS_HISTOGRAM; // (id)
			stats->stavalues[slot_idx] = bound_hist_values; // (from 0 to num_hist-1)
			stats->numvalues[slot_idx] = num_hist; // nb of hists)

			slot_idx++; // szymon: en gros slot_idx=0 -> bound_hist et slot_inx=1 -> lengths_hist
		}

		/* // szymon: le histogram de longueurs
		 * Generate a length histogram slot entry if there are at least two
		 * values.
		 */
		 // alex: on verifie que c'est pas vide - null
		if (non_empty_cnt >= 2)
		{
			/*
			 * Ascending sort of range lengths for further filling of
			 * histogram
			 */
			qsort(lengths, non_empty_cnt, sizeof(float8), float8_qsort_cmp);

			num_hist = non_empty_cnt;
			if (num_hist > num_bins)
				num_hist = num_bins + 1;

			length_hist_values = (Datum *) palloc(num_hist * sizeof(Datum));

			/*
			 * The object of this loop is to copy the first and last lengths[]
			 * entries along with evenly-spaced values in between. So the i'th
			 * value is lengths[(i * (nvals - 1)) / (num_hist - 1)]. But
			 * computing that subscript directly risks integer overflow when
			 * the stats target is more than a couple thousand.  Instead we
			 * add (nvals - 1) / (num_hist - 1) to pos at each step, tracking
			 * the integral and fractional parts of the sum separately.
			 */
			delta = (non_empty_cnt - 1) / (num_hist - 1);
			deltafrac = (non_empty_cnt - 1) % (num_hist - 1);
			pos = posfrac = 0;

			for (i = 0; i < num_hist; i++)
			{
				length_hist_values[i] = Float8GetDatum(lengths[pos]);
				pos += delta;
				posfrac += deltafrac;
				if (posfrac >= (num_hist - 1))
				{
					/* fractional part exceeds 1, carry to integer part */
					pos++;
					posfrac -= (num_hist - 1);
				}
			}
		}
		else
		{
			/*
			 * Even when we don't create the histogram, store an empty array
			 * to mean "no histogram". We can't just leave stavalues NULL,
			 * because get_attstatsslot() errors if you ask for stavalues, and
			 * it's NULL. We'll still store the empty fraction in stanumbers.
			 */
			length_hist_values = palloc(0);
			num_hist = 0;
		}
		stats->staop[slot_idx] = Float8LessOperator;
		stats->stacoll[slot_idx] = InvalidOid;
		stats->stavalues[slot_idx] = length_hist_values;
		stats->numvalues[slot_idx] = num_hist;
		stats->statypid[slot_idx] = FLOAT8OID;
		stats->statyplen[slot_idx] = sizeof(float8);
		stats->statypbyval[slot_idx] = FLOAT8PASSBYVAL;
		stats->statypalign[slot_idx] = 'd';

		/* Store the fraction of empty ranges */
		emptyfrac = (float4 *) palloc(sizeof(float4));
		*emptyfrac = ((double) empty_cnt) / ((double) non_null_cnt);
		stats->stanumbers[slot_idx] = emptyfrac;
		stats->numnumbers[slot_idx] = 1;

		stats->stakind[slot_idx] = STATISTIC_KIND_RANGE_LENGTH_HISTOGRAM;

		ComputeFrequencyHistogram(stats, slot_idx, lowers, uppers, num_hist); // alex: compute the frequency histogram at next slot_idx, STATISTIC_KIND_FREQUENCY_HISTOGRAM 42
		++slot_idx;

		MemoryContextSwitchTo(old_cxt);
	}
	else if (null_cnt > 0)
	{
		/* We found only nulls; assume the column is entirely null */
		stats->stats_valid = true;
		stats->stanullfrac = 1.0;
		stats->stawidth = 0;	/* "unknown" */
		stats->stadistinct = 0.0;	/* "unknown" */
	}

	/*
	 * We don't need to bother cleaning up any of our temporary palloc's. The
	 * hashtable should also go away, as it used a child memory context.
	 */
}
