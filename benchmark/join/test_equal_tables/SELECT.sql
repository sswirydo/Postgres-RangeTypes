EXPLAIN (ANALYZE) SELECT count(*) FROM integers2 t1, integers2BIS t2 WHERE t1.r && t2.r; -- JOIN CARDINALITY ESTIMATION