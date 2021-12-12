CREATE TABLE integers1(r int4range);
CREATE TABLE integers2(r int4range);

INSERT INTO integers1 SELECT int4range(s+3, s+4) FROM generate_series(1, 1000) AS s;
INSERT INTO integers2 SELECT int4range((-s)*2, s*8) FROM generate_series(1, 200) AS s;