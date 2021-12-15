# Postgres-RangeTypes
## Description
This project was realised in the context of the H415 Databases Architectutre university course at Université Libre de Bruxelles.

The purpose of this project is to improve the overall scheme of statistics collection of PostgreSQL for range types along with the cardinality estimation for range types (join and restriction selectivites) and also to understand in more detail how a database works.

## Structure
### postgres-REL_13_STABLE
The official relase of postgreSQL. Should not be modified. Should be used to compare to compare our solution to the official.

#### Version
28 Nov 2021 1AM CEST
Commit : https://github.com/postgres/postgres/commit/f76fd05bae047103cb36ef5fb82137c8995142c1


### postgres-ramen
Our modified version of postgreSQL. Why ramen? Cause why not, every one loves ramen.

#### Modified files
[rangetypes_typeanalyze.c](./postgres-ramen/src/backend/utils/adt/rangetypes_typanalyze.c)
#####
[geo_selfuncs.c](./postgres-ramen/src/backend/utils/adt/geo_selfuncs.c).
#####
[rangetypes_selfuncs.c](./postgres-ramen/src/backend/utils/adt/rangetypes_selfuncs.c)
#####
[pg_operator.dat](./postgres-ramen/src/include/catalog/pg_operator.dat)
#####
[pg_statistic.h](./postgres-ramen/src/include/catalog/pg_statistic.h)
#####
[pg_proc.dat](postgres-ramen/src/include/catalog/pg_proc.dat)


### Testing
In order to test the implementation, you can use the compiling script.
```bash
scripts/compile-psql.sh
```
You can take inspiration from the Makefile situated in the root folder, in order to make sure you are passing correct arguments to the script.

### Debug printing
Instead of printing with printf(...) etc., simply put this line in your code
```C
FILE* file = fopen("sushiOUT.txt","a"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);
```
which while add append everything into an output file in /postgres-ramen/ramendata/sushiOUT.txt
