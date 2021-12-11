
/* \o pgsql-pizza.txt -- pour sauvegarder */

-- '(' or ')' = borne non-comprise
-- '[' or ']' = born comprise

-- INTEGER VALUES -

/*
CREATE TABLE integers1(r int4range);
CREATE TABLE integers2(r int4range);
CREATE TABLE integers2BIS(r int4range);
INSERT INTO integers1 SELECT int4range(s+3, s+4) FROM generate_series(1, 1000) AS s;
INSERT INTO integers2 SELECT int4range((-s)*2, s*8) FROM generate_series(1, 200) AS s;
INSERT INTO integers2BIS SELECT int4range((-s)*2, s*8) FROM generate_series(1, 200) AS s;
VACUUM ANALYZE integers1;
VACUUM ANALYZE integers2;
VACUUM ANALYZE integers2BIS;
EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM integers1 t1, integers2 t2 WHERE t1.r && t2.r; -- JOIN CARDINALITY ESTIMATION
EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM integers2 t1, integers2BIS t2 WHERE t1.r && t2.r; -- JOIN CARDINALITY ESTIMATION
DROP table integers1;
DROP table integers2;
*/


--CREATE TABLE restoverlapint(r int4range);
CREATE TABLE reststrleftint(r int4range);
--INSERT INTO restoverlapint SELECT int4range(s, s+10) FROM generate_series(1, 10000) AS s;
INSERT INTO reststrleftint SELECT int4range(s, s+10) FROM generate_series(1, 1000) AS s;
-- VACUUM ANALYZE restoverlapint;
VACUUM ANALYZE reststrleftint;
-- EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM restoverlapint t1 WHERE t1.r && int4range(25, 75); -- RESTRICTION SELECTIVITY OVERLAP ESTIMATION
EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM reststrleftint t1 WHERE t1.r << int4range(25, 75); -- RESTRICTION SELECTIVITY OVERLAP ESTIMATION
-- DROP table restoverlapint;
DROP table reststrleftint;


-- FLOAT VALUES -- 
/*
CREATE TABLE floats1(r numrange);
CREATE TABLE floats2(r numrange);
INSERT INTO floats1 SELECT numrange(s*5.2, s*8.1) FROM generate_series(1, 10) AS s;
INSERT INTO floats2 SELECT numrange((-s)*3.1, s*4.1) FROM generate_series(1, 10) AS s;

SELECT * FROM floats1;
SELECT * FROM floats2;

--VACUUM ANALYZE floats1;
VACUUM ANALYZE floats2;

EXPLAIN (ANALYZE, BUFFERS) SELECT count(*) FROM floats1 t1, floats2 t2 WHERE t1.r && t2.r;

DROP table floats1;
DROP table floats2;
*/




