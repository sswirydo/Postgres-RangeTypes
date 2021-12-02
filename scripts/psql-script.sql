CREATE TABLE temp1(r int4range);
CREATE TABLE temp2(r int4range);
INSERT INTO temp1 SELECT int4range(s, s+20) FROM generate_series(0,
5) AS s;
INSERT INTO temp2 SELECT int4range(s, s+100) FROM generate_series(0,
5) AS s;
VACUUM ANALYZE temp1;



-- SELECT * FROM temp WHERE r && int4range(80,100); -- de [71,81] à [99,109] -> Bornes non-comprises

EXPLAIN SELECT count(*) FROM temp1 t1, temp2 t2 WHERE t1.r && int4range(60, 100);

DROP table temp1;
DROP table temp2;

