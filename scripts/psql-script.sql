CREATE TABLE temp(r int4range);
INSERT INTO temp SELECT int4range(s, s+10) FROM generate_series(0,
1000) AS s;
VACUUM ANALYZE temp;

/*
CREATE TABLE a (
    c INT NOT NULL
);
INSERT INTO a (c) VALUES (1);

SELECT * FROM a WHERE c = 1;

DROP TABLE a;

SELECT * FROM temp WHERE r = '(71,81)'::int4range;
*/

-- SELECT * FROM information_schema.columns WHERE table_name = 'temp';

SELECT * FROM temp WHERE r && int4range(80,100); -- de [71,81] à [99,109] -> Bornes non-comprises


--  SELECT (DATE '2016-01-10', DATE '2016-02-01') OVERLAPS (DATE '2016-01-20', DATE '2016-02-10');
--  &&(anyrange,anyrange)
-- anyrange && anyrange

-- oprcom => '=(int8,int4)' (equal)

-- '[1992-03-21, 1994-06-25]'::daterange

-- SELECT &&(DATE '2020-01-20', DATE '2020-02-10');

/*
DROP TABLE temp;


CREATE TABLE
INSERT 0 1001
VACUUM
 table_catalog | table_schema | table_name | column_name | ordinal_position | column_default | is_nullable | data_type | character_maximum_length | character_octet_length | numeric_precision | numeric_precision_radix | numeric_scale | datetime_precision | interval_type | interval_precision | character_set_catalog | character_set_schema | character_set_name | collation_catalog | collation_schema | collation_name | domain_catalog | domain_schema | domain_name | udt_catalog | udt_schema | udt_name  | scope_catalog | scope_schema | scope_name | maximum_cardinality | dtd_identifier | is_self_referencing | is_identity | identity_generation | identity_start | identity_increment | identity_maximum | identity_minimum | identity_cycle | is_generated | generation_expression | is_updatable 
---------------+--------------+------------+-------------+------------------+----------------+-------------+-----------+--------------------------+------------------------+-------------------+-------------------------+---------------+--------------------+---------------+--------------------+-----------------------+----------------------+--------------------+-------------------+------------------+----------------+----------------+---------------+-------------+-------------+------------+-----------+---------------+--------------+------------+---------------------+----------------+---------------------+-------------+---------------------+----------------+--------------------+------------------+------------------+----------------+--------------+-----------------------+--------------
 sushi         | public       | temp       | r           |                1 |                | YES         | int4range |                          |                        |                   |                         |               |                    |               |                    |                       |                      |                    |                   |                  |                |                |               |             | sushi       | pg_catalog | int4range |               |              |            |                     | 1              | NO                  | NO          |                     |                |                    |                  |                  | NO             | NEVER        |                       | YES
(1 row)

DROP TABLE
*/
