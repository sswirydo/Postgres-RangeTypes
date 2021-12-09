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

// -- H-417 OUR FUNCTIONS -- //
static void ComputeFrequencyHistogram(VacAttrStats* stats, int slot_idx, RangeBound* lowers, RangeBound* uppers, int rows);
static void count_frequencies(RangeBound* lowers, RangeBound* uppers, int* frequencies_vals, int* frequencies_intervals, int* sum_hist, int nb_of_intervals, int rows, int interval_length, int min);
static void normalize_frequencies(float8* normalized_frequencies_vals, int* frequencies_vals, int* sum_hist, int nb_of_intervals, int rows);
// ------------------------ //


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


static void
ComputeFrequencyHistogram(VacAttrStats* stats, int slot_idx, RangeBound* lowers, RangeBound* uppers, int rows)
{
	/*
	typedef struct
	{
		Datum		val;			// the bound value, if any
		bool		infinite;		// bound is +/- infinity
		bool		inclusive;		// bound is inclusive (vs exclusive)
		bool		lower;			// this is the lower (vs upper) bound
	} RangeBound;
	*/

	int PERCENT_INTERVAL_LENGTH = 5; //FIXME ARBITRARY VALUE : INTERVAL LENGTH = 5% OF TOTAL LENGTH
	int i;
	int j;
	int min;
	int max;

	int length;
	int interval_length;
	int nb_of_intervals;
	int sum_hist;

	int* frequencies_vals;
	int* frequencies_intervals;
	float8* normalized_frequencies_vals;

	// No data for floats. Rip. :/
	//printf("lower: %f\n", (float) (lower)->val);
	//printf("upper: %f\n", DatumGetFloat8((upper+rows+1)->val));
	// fflush(stdout);

	min = (lowers)->val;
	max = (uppers+rows-1)->val;

	/*
	printf("lower: %f\n", (float) min);
	printf("upper: %d\n", max);
	fflush(stdout);
	*/

	// -- CHOOSING THE INTERVAL LENGTH AND THE NUMBER OF INTERVALS -- //
	length = max - min; 
	interval_length = PERCENT_INTERVAL_LENGTH * length;
	if (interval_length % 100 == 0)
		interval_length = interval_length/100;
	else
		interval_length = 1 + interval_length/100; // +1 for rounding up.
	nb_of_intervals = length/interval_length;
	if (length % interval_length != 0) 
		nb_of_intervals++; // Rounding

	// -- ALLOCATING MEMORY -- //
	frequencies_vals = (int*) palloc(sizeof(int) * nb_of_intervals);
	frequencies_intervals = (int*) palloc(sizeof(int) * nb_of_intervals);
	normalized_frequencies_vals = (float8*) palloc(sizeof(float8) * nb_of_intervals);
	memset(frequencies_vals, 0, sizeof(int) * nb_of_intervals);
	memset(frequencies_intervals, 0, sizeof(int) * nb_of_intervals);

	// -- BUILDING THE FREQUENCY HISTOGRAM -- //
	count_frequencies(lowers, uppers, frequencies_vals, frequencies_intervals, &sum_hist, nb_of_intervals, rows, interval_length, min);
	normalize_frequencies(normalized_frequencies_vals, frequencies_vals, &sum_hist, nb_of_intervals, rows);

	// -- STORING THE HISTOGRAM FOR LATER USAGE -- //
	Datum* hist_frequencies_vals = (Datum *) palloc(sizeof(Datum) * nb_of_intervals);
	for (i = 0; i < nb_of_intervals; ++i)
		hist_frequencies_vals[i] = Float8GetDatum(normalized_frequencies_vals[i]);
	stats->staop[slot_idx] = Float8LessOperator;
	stats->stacoll[slot_idx] = InvalidOid;
	stats->stavalues[slot_idx] = hist_frequencies_vals;
	stats->numvalues[slot_idx] = nb_of_intervals;
	stats->statypid[slot_idx] = FLOAT8OID;
	stats->statyplen[slot_idx] = sizeof(float8);
	stats->statypbyval[slot_idx] = FLOAT8PASSBYVAL;
	stats->statypalign[slot_idx] = 'd';
	stats->stakind[slot_idx] = STATISTIC_KIND_FREQUENCY_HISTOGRAM;
	
	// -- FREEING ALLOCATED MEMORY -- //
	pfree(frequencies_vals);
	pfree(frequencies_intervals);
	pfree(normalized_frequencies_vals);
}

static void
count_frequencies(RangeBound* lowers, RangeBound* uppers, int* frequencies_vals, int* frequencies_intervals, int* sum_hist, int nb_of_intervals, int rows, int interval_length, int min)
{
	///////////////////////
	// CLASSIC HISTOGRAM //
	///////////////////////
	
	*(sum_hist) = 0;
	int i;
	int l = 0;
	int u = 0;
	int count = 0;
	int sup = min - 1;

	for (i = 0; i < nb_of_intervals; ++i){
		sup += interval_length;
		frequencies_intervals[i] = sup; // fixme delete (optimizer)
		while((int)(lowers+l)->val <= sup && l < rows){
			count++;
			l++;
		}
		frequencies_vals[i] = count;
		*(sum_hist) += count;
		while((int)(uppers+u)->val <= sup + 1 && u < rows){ // TO VERIFY '&& u < rows' (normaly not useful)
			count--;
			u++;
		}
	}
}

static void
normalize_frequencies(float8* normalized_frequencies_vals, int* frequencies_vals, int* sum_hist, int nb_of_intervals, int rows)
{
	/////////////////////////////
	// HISTOGRAM NORMALIZATION //
	/////////////////////////////
	
	int i;
	float ratio;
	ratio = ((float) rows) / (float) *(sum_hist); // divided by "sum_hist" (= percentage) -> multiply by "rows" (weighted)
	
	for (i = 0; i < nb_of_intervals; ++i)
		normalized_frequencies_vals[i] = (float8) ((float) frequencies_vals[i] * ratio);
}



/*
 * compute_range_stats() -- compute statistics (if not empty) for (a) range(s) column(s)
 */
static void
compute_range_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc,
					int samplerows)
{
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

			// -------------------------- //
			// No data for floats. Rip. :/
<<<<<<< HEAD
			// printf("-VALS:\n");
			// printf("val: %lf\n", (float) lower.val);
			// printf("val: %lf\n", (float8) lower.val);
			// printf("val: %lf\n", DatumGetFloat8(lower.val));
			// printf("val: %lf\n", (float8) DatumGetFloat8(lower.val));
			// printf("val: %lf\n", (float) DatumGetFloat8(lower.val));
			// -------------------------- //
=======
			//printf("lower: %d\n", lower.val);

		 	printf("lower: %f\n", (float) lower.val);
		 	printf("upper: %f\n", DatumGetFloat8((upper.val)));
			// fflush(stdout);
>>>>>>> 07680cae858bb313207c1c62a46d5fbecbb671ab

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

		++slot_idx;
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
