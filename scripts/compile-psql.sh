#!/bin/bash

# Expected format:
# sudo ./compile-pgsql full_path_sql_script ramen_sql_root_path [official_sql_root_path]


function main {
    current_path=$(pwd)

    if [ $# -eq 2 ] 
    then
        cd "$2"
        make_pgsql || error_quit "make"
        run_pgsql || error_quit "run"
        cd "${current_path}"
    fi

    if [ $# -eq 3 ] 
    then
        cd "$3"
        make_pgsql || error_quit "make"
        run_pgsql || error_quit "run"
        cd "${current_path}"
    fi
}

function check_arguments {
    echo $#
    if [ $# -eq 0 ] || [ $# -ge 4 ] 
    then
        echo "Incorrect program format."
        echo "Expected: sudo ./compile-pgsql full_path_sql_script ramen_sql_root_path [official_sql_root_path]"
    fi
}

function check_root {
    if [[ $EUID -ne 0 ]]
    then
        echo "Root required!"
        echo "\t Run with: sudo ./compile-pgsql full_path_sql_script ramen_sql_root_path [official_sql_root_path]"
        exit
    fi
}

function error_quit {
    echo "Error during $1"
}

function make_pgsql {
    make || error_quit
    sudo make install || error_quit
}

function run_pgsql {
    rm -rR /usr/local/pgsql/data
    mkdir /usr/local/pgsql/data
    chown postgres /usr/local/pgsql/data
    setfacl -m u:postgres:rwx "${SQL_SCRIPT}"
    su postgres << EOSU
    echo "--- init"
    /usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data
    echo "--- pg_crl start"
    /usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data start
    echo "--- createdb"
    /usr/local/pgsql/bin/createdb sushi
    echo "--- psql w/ script"
    /usr/local/pgsql/bin/psql -d sushi -f "${SQL_SCRIPT}"
    echo "--- drop"
    /usr/local/pgsql/bin/dropdb sushi
    echo "--- stop"
    /usr/local/pgsql/bin/pg_ctl stop  
    sleep 10
EOSU
}

check_root "${@}"
check_arguments "${@}"

SQL_SCRIPT="${1}"
RAMEN_PATH="${2}"
OFFICIAL_PATH="${3}"

main "${@}"
