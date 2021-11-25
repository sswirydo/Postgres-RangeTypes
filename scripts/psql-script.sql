CREATE TABLE temp(r int4range);
INSERT INTO temp SELECT int4range(s, s+10) FROM generate_series(0,
1000) AS s;
VACUUM ANALYZE temp;
DROP TABLE temp;