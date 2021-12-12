import sys
from random import randint

table_1 = "table_1"
table_2 = "table_2"

print("Input format : <rows> <min> <max>")
rows_1, min_1, max_1 = (input("Table 1 : ")).split()
rows_2, min_2, max_2 = (input("Table 2 : ")).split()

com1 = "-- " + table_1 + " : " + rows_1 + " [" + min_1 + " ; " + max_1 + "]"
com2 = "-- " + table_2 + " : " + rows_2 + " [" + min_2 + " ; " + max_2 + "]"

rows_1 = int(rows_1)
min_1 = int(min_1)
max_1 = int(max_1)
rows_2 = int(rows_2)
min_2 = int(min_2)
max_2 = int(max_2)

sys.stdout = open("INIT.sql", 'w')
print(com1)
print(com2)

print("CREATE TABLE " + table_1 +"(r int4range);")
print("CREATE TABLE " + table_2 +"(r int4range);")

for i in range(rows_1):
	r1 = randint(min_1, max_1)
	r2 = randint(min_1, max_1)
	if r1 > r2:
		r1, r2 = r2, r1
	print("INSERT INTO", table_1, "SELECT int4range("+str(r1)+", "+str(r2)+");")
for i in range(rows_2):
	r1 = randint(min_2, max_2)
	r2 = randint(min_2, max_2)
	if r1 > r2:
		r1, r2 = r2, r1
	print("INSERT INTO", table_2, "SELECT int4range("+str(r1)+", "+str(r2)+");")

sys.stdout = open("VACUUM.sql", 'w')
print(com1)
print(com2)

print("VACUUM ANALYZE " + table_1 +";")
print("VACUUM ANALYZE " + table_2 +";")

sys.stdout = open("SELECT.sql", 'w')

print(com1)
print(com2)
r1 = randint(min_1, max_1)
r2 = randint(min_1, max_1)
if r1 > r2:
	r1, r2 = r2, r1
t1 = randint(min_2, max_2)
t2 = randint(min_2, max_2)
if t1 > t2:
	t1, t2 = t2, t1
print("EXPLAIN (ANALYZE) SELECT count(*) FROM", table_1, "t1,", table_2, "t2 WHERE t1.r && t2.r; -- JOIN CARDINALITY ESTIMATION")
print("EXPLAIN (ANALYZE) SELECT count(*) FROM", table_1, "t1 WHERE t1.r && int4range("+str(r1)+", "+str(r2)+"); -- RESTRICTION SELECTIVITY OVERLAP ESTIMATION")
print("EXPLAIN (ANALYZE) SELECT count(*) FROM", table_1, "t1 WHERE t1.r << int4range("+str(r1)+", "+str(r2)+"); -- RESTRICTION SELECTIVITY STRICTLY LEFT ESTIMATION")

