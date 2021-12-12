
all:

szymon: 
	clear
	sudo /media/sf_Postgres-RangeTypes/scripts/compile-psql.sh /media/sf_Postgres-RangeTypes/postgres-ramen/ /media/sf_Postgres-RangeTypes/scripts/psql-script.sql
szymon-timed:
	clear
	sudo /media/sf_Postgres-RangeTypes/scripts/compile-psql.sh /media/sf_Postgres-RangeTypes/postgres-ramen/ /media/sf_Postgres-RangeTypes/benchmark/join/test_equal_tables 

alex:
	clear
	sudo /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/scripts/compile-psql.sh /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/postgres-ramen /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/scripts/psql-script.sql 
	
alex1:
	clear
	sudo /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/scripts/compile-psql.sh /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/postgres-ramen /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/benchmark/diff
	
alexp1:
	clear
	sudo /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/scripts/compile-psql.sh /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/postgres-REL_13_STABLE /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/benchmark/diff

alexx:
	clear
	sudo /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/scripts/compile-psql.sh /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/postgres-REL_13_STABLE /home/parallels/Desktop/Parallels_Shared_Folders/Postgres-RangeTypes/scripts/psql-script.sql 

aure:
	clear
	sudo /home/ubuntu/Postgres-RangeTypes/scripts/compile-psql.sh /home/ubuntu/Postgres-RangeTypes/postgres-ramen /home/ubuntu/Postgres-RangeTypes/scripts/psql-script.sql

nico:
	clear
	sudo /home/kali/Desktop/ramen/Postgres-RangeTypes/scripts/compile-psql.sh /home/kali/Desktop/ramen/Postgres-RangeTypes/postgres-ramen/ /home/kali/Desktop/ramen/Postgres-RangeTypes/scripts/psql-script.sql
