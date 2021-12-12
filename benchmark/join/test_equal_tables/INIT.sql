CREATE TABLE integers2(r int4range);
CREATE TABLE integers2BIS(r int4range);

INSERT INTO integers2 SELECT int4range((-s)*2, s*8) FROM generate_series(1, 200) AS s;
INSERT INTO integers2BIS SELECT int4range((-s)*2, s*8) FROM generate_series(1, 200) AS s;