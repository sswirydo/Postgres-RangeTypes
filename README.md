# Postgres-RangeTypes

## Structure
### postgres-REL_13_STABLE
The official relase of postgreSQL. Should not be modified. Should be used to compare to compare our solution to the official.

#### Version
28 Nov 2021 1AM CEST
Commit : https://github.com/postgres/postgres/commit/f76fd05bae047103cb36ef5fb82137c8995142c1


### postgres-ramen
Our modified version of postgreSQL. Why ramen? Cause why not, every one loves ramen.


### Debug printing
Instead of printing with printf(...) etc., simply put this line in your code
```C
FILE* file = fopen("sushiOUT.txt","w+"); fprintf(file, "\nFile: %s Line: %d Fct: %s Info: %s",__FILE__, __LINE__, __func__, ""); fclose(file);
```
which while add append everything into an output file in /postgres-ramen/ramendata/sushiOUT.txt
