
/* \o pgsql-pizza.txt -- pour sauvegarder */

-- '(' or ')' = borne non-comprise
-- '[' or ']' = born comprise

-- INTEGER VALUES --
CREATE TABLE integers1(r int4range);
CREATE TABLE integers2(r int4range);
INSERT INTO integers1 SELECT int4range(s*5, s*8) FROM generate_series(1, 10) AS s;
INSERT INTO integers2 SELECT int4range((-s)*3, s*4) FROM generate_series(1, 10) AS s;
--INSERT INTO integers1 SELECT int4range(s*5, s*8) FROM generate_series(1, 10) AS s;
--INSERT INTO integers2 SELECT int4range((-s)*3, s*4) FROM generate_series(1, 10) AS s;

-- SELECT * FROM integers1;

--VACUUM ANALYZE integers1;
--VACUUM ANALYZE integers2;

EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM integers1 t1, integers2 t2 WHERE t1.r && t2.r;

DROP table integers1;
DROP table integers2;

-- FLOAT VALUES -- 
/*
CREATE TABLE floats1(r numrange);
CREATE TABLE floats2(r numrange);
INSERT INTO floats1 SELECT floats1(s*5.2, s*8.1) FROM generate_series(1, 10) AS s;
INSERT INTO floats2 SELECT numrange((-s)*3.1, s*4.1) FROM generate_series(1, 10) AS s;

VACUUM ANALYZE floats1;
VACUUM ANALYZE floats2;

EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM floats1 t1, floats2 t2 WHERE t1.r && t2.r;
EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM floats1 t1, floats2 t2 WHERE t1.r && t2.r;

DROP table integers1;
DROP table integers2;
*/

-- TIMESTAMP VALUES --
/*
CREATE TABLE timestamps1(r tsrange);
CREATE TABLE timestamps2(r tsrange);
INSERT INTO timestamps1 VALUES ('[2010-01-01 14:30, 2010-01-01 15:30)'), ('[2010-01-01 14:30, 2010-01-01 18:30)'), ('[2010-01-01 11:30, 2010-01-01 17:30)'), ('[2010-01-01 11:25, 2010-01-01 19:30)'), ('[2010-01-01 10:30, 20201030-01-01 15:30)');
INSERT INTO timestamps2 VALUES ('[2010-01-01 5:30, 2010-01-01 15:30)'), ('[2010-01-01 05:30, 2010-01-01 23:30)'), ('[2010-01-01 12:30, 2010-01-01 19:30)'), ('[2010-01-01 9:25, 2010-01-01 13:30)'), ('[2010-01-01 9:30, 2010-01-01 14:30)');

VACUUM ANALYZE timestamps1;
VACUUM ANALYZE timestamps2;

EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM timestamps1 t1, timestamps2 t2 WHERE t1.r && t2.r;
EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM timestamps1 t1, timestamps2 t2 WHERE t1.r && t2.r;

DROP table integers1;
DROP table integers2;
*/

-- DATE VALUES --
/*
CREATE TABLE dates1(r daterange);
CREATE TABLE dates2(r daterange);

VACUUM ANALYZE dates2;
VACUUM ANALYZE dates2;

EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM dates1 t1, dates2 t2 WHERE t1.r && t2.r;
EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM dates1 t1, dates2 t2 WHERE t1.r && t2.r;

DROP table integers1;
DROP table integers2;
*/




