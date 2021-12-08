
/* \o pgsql-pizza.txt -- pour sauvegarder */


CREATE TABLE tablealacon(r int4range);
INSERT INTO tablealacon SELECT int4range(0, s+1, '[)') FROM generate_series(0, 40) AS s; -- frequencies = [40, 30, 20, 10]
-- '(' or ')' = borne non-comprise
-- '[' or ']' = born comprise
--VACUUM ANALYZE tablealacon;
--SELECT * FROM tablealacon;

CREATE TABLE temp1(r int4range);
CREATE TABLE temp2(r int4range);
INSERT INTO temp1 SELECT int4range(s*5, s*8) FROM generate_series(0, 6) AS s; -- lows: [5, 10, 15, 20, 25, 30, 35, 40, 45, 50] -- ups: [8, 16, 24, 32, 40, 48, 56, 64, 72, 80]
-- en bonus gérer si int4range(supérieur, inférieur) 
INSERT INTO temp2 SELECT int4range((-s)*3, s*4) FROM generate_series(0, 6) AS s;

SELECT * FROM temp1;
SELECT * FROM temp2;

--VACUUM ANALYZE temp1; -- per column
--VACUUM ANALYZE temp2;

CREATE TABLE reservation (room int, during tsrange);
INSERT INTO reservation VALUES
    (1108, '[2010-01-01 14:30, 2010-01-01 15:30)');
INSERT INTO reservation VALUES
    (8765, '[2005-01-01 14:30, 2016-01-01 18:30)');
INSERT INTO reservation VALUES
    (987, '[2009-01-01 14:30, 2011-01-01 15:30)');
INSERT INTO reservation VALUES
    (4567, '[2010-01-01 14:30, 2030-01-01 15:30)');

VACUUM ANALYZE reservation;

-- EXPLAIN SELECT count(*) FROM temp1 t1, temp2 t2 WHERE t1.r && int4range(60, 100);

-- EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM temp1 t1, temp2 t2 WHERE t1.r && t2.r;
-- EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM temp1 t1, temp2 t2 WHERE t1.r && t2.r;


-- SELECT * FROM pg_stats WHERE tablename = temp1;-- AND attname = r;


DROP table temp1;
DROP table temp2;

/*

2021-12-03 23:54:40.183 CET [30996] ERROR:  column "’temp1’" does not exist at character 42
2021-12-03 23:54:40.183 CET [30996] STATEMENT:  SELECT *
	FROM pg_stats
	WHERE tablename = ’temp1’ AND attname = ’r’;
psql:/media/sf_Postgres-RangeTypes/scripts/psql-script.sql:19: ERROR:  column "’temp1’" does not exist
LINE 3: WHERE tablename = ’temp1’ AND attname = ’r’;


*/
