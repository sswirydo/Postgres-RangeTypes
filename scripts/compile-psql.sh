#!/bin/bash

:'  COMPILE-PSQL SCRIPT:
    Expected format:
    sudo ./compile-psql.sh ramen_sql_root_path (full_path_sql_script | full_path_sql_script)
'

START=$(date +%s.%N)
TIME=$(date +%s.%N)

SQL_SCRIPT=''
SQL_INIT_SCRIPT=''
SQL_VACUUM_SCRIPT=''
SQL_SELECT_SCRIPT=''

# How to measure time with bash. (might not work on some OSs)

function elapsed_time {
    echo $TIME;
}
function start_timer {
    START=$(date +%s%N)
}
function stop_timer {
    TIME=$((($(date +%s%N) - $START)/1000000))
}


function check_arguments {
    if [ $# -eq 0 ] || [ $# -ge 4 ] 
    then
        echo "Incorrect program format."
        echo "Expected: sudo ./compile-psql.sh ramen_sql_root_path (full_path_sql_script | full_path_sql_script)"
        exit
    fi
}
function check_root {
    if [[ $EUID -ne 0 ]]
    then
        echo "Root required!"
        echo "\t Run with: sudo ./compile-psql.sh ramen_sql_root_path (full_path_sql_script | full_path_sql_script)"
        exit
    fi
}
function error_quit {
    echo "Error during $1"
    exit
}

function make_pgsql {
    # ./configure # 'uncomment if you haver never compiled it before.'
    make || error_quit "make"
    sudo make install || error_quit "make install"
}
function run_pgsql {
    rm -rR /usr/local/pgsql/data
    mkdir /usr/local/pgsql/data
    chown postgres /usr/local/pgsql/data
    setfacl -m u:postgres:rwx "${SQL_SCRIPT}"
    su postgres << EOSU
    echo "--- init"
    /usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data || exit
    echo "--- pg_crl start"
    /usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data start  || exit
    echo "--- createdb"
    /usr/local/pgsql/bin/createdb sushi
    echo "--- psql w/ script"
    echo ""
    echo ""
    /usr/local/pgsql/bin/psql -d sushi -f "${SQL_SCRIPT}" --echo-hidden
    echo ""
    echo ""
    echo "--- drop"
    /usr/local/pgsql/bin/dropdb sushi
    echo "--- stop"
    /usr/local/pgsql/bin/pg_ctl stop  -D /usr/local/pgsql/data -m fast || exit
    sleep 2
EOSU
}

function run_pgsql_timed {
    rm -rR /usr/local/pgsql/data
    mkdir /usr/local/pgsql/data
    chown postgres /usr/local/pgsql/data
    setfacl -m u:postgres:rwx "${SQL_SCRIPT}"
    su postgres << EOSU
    echo "--- init"
    /usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data || exit
    echo "--- pg_crl start"
    /usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data start  || exit
    echo "--- createdb"
    /usr/local/pgsql/bin/createdb sushi
    echo "--- psql w/ script"
    echo ""
    echo ""
    echo "" 
    /usr/local/pgsql/bin/psql -d sushi -f '${SQL_INIT_SCRIPT}'
EOSU

    start_timer
    su postgres << EOSU
    /usr/local/pgsql/bin/psql -d sushi -f '${SQL_VACUUM_SCRIPT}'
EOSU
    stop_timer
    echo ''
    echo "--- BENCHMARK: VACUUM TIME:"
    elapsed_time
    echo ''

    start_timer
    su postgres << EOSU
    /usr/local/pgsql/bin/psql -d sushi -f '${SQL_SELECT_SCRIPT}'
EOSU
    stop_timer
    echo ''
    echo "--- BENCHMARK: SELECT TIME:"
    elapsed_time
    echo ''

    start_timer
    su postgres << EOSU
EOSU
    stop_timer
    echo ''
    echo "--- BENCHMARK: APPROX COMMAND TIME:"
    elapsed_time
    echo ''

    su postgres << EOSU
    echo ""
    echo ""
    echo "--- drop"
    /usr/local/pgsql/bin/dropdb sushi
    echo "--- stop"
    /usr/local/pgsql/bin/pg_ctl stop  -D /usr/local/pgsql/data -m fast || exit
    sleep 2
EOSU
}

function main {
    current_path=$(pwd)
    if [ $# -eq 2 ] 
    then
        cd "${RAMEN_PATH}"
        make_pgsql || error_quit "make"
        
        if [[ -d ${SQL} ]]; then
            SQL_INIT_SCRIPT=${SQL}'/INIT.sql'
            SQL_VACUUM_SCRIPT=${SQL}'/VACUUM.sql'
            SQL_SELECT_SCRIPT=${SQL}'/SELECT.sql'
            run_pgsql_timed || error_quit "run-timed"
        elif [[ -f ${SQL} ]]; then
            SQL_SCRIPT=${SQL}
            run_pgsql || error_quit "run"
        else
            echo "--- INCORRECT FILE ---"
            echo ${SQL}
            echo "--- INCORRECT FILE ---"
        fi
        cd "${current_path}"
    fi
}

check_root "${@}"
check_arguments "${@}"

RAMEN_PATH="${1}"
SQL="${2}"
echo ${RAMEN_PATH}
echo ${SQL}
OFFICIAL_PATH="${3}"

main "${@}"
