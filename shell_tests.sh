#!/bin/bash

generate=0
run_norm=1                                 # run normal tests
run_valg=1                                 # run valgrind tests

ticktime_norm=0.1
ticktime_valg=0.5
ticktime=$ticktime_valg

function major_sep(){
    printf '%s\n' '====================================='
}
function minor_sep(){
    printf '%s\n' '-------------------------------------'
}

printf "Loading tests... "
source shell_tests_data.sh
printf "%d tests loaded\n" "$T"

NTESTS=$T
VTESTS=$T
NPASS=0
NVALG=0

all_tests=$(seq $NTESTS)

# Check whether a single test is being run
single_test=$1
if ((single_test > 0 && single_test <= NTESTS)); then
    printf "Running single TEST %d\n" "$single_test"
    all_tests=$single_test
    NTESTS=1
    VTESTS=1
else
    printf "Running %d tests\n" "$NTESTS"
fi

# printf "tests: %s\n" "$all_tests"
printf "\n"

printf "RUNNING NORMAL TESTS\n"
export valg=""
ticktime=$ticktime_norm


if [ "$run_norm" = "1" ]; then

    for i in $all_tests; do
        printf -v tid "test-%02d" "$i"         # test id for various places
        printf "TEST %2d %-18s : " "$i" "${tnames[i]}"

        # Run the test
        outfiles=()
        eval "${setup[i]}"
        while read -r act; do
            eval "$act"
            tick
        done <<< "${actions[i]}"
        eval "${teardown[i]}"

        # Check each client for failure, print side-by-side diff if problems
        failed=0
        for ((c=0; c<${#outfiles[*]}; c++)); do
            outfile=${outfiles[c]}
            printf "%s\n" "${expect_outs[i]}" | awk -F '\t' "{print \$$((c+1))}" > ${outfile}.expect
            #printf "%s\n" "${expect_outs[i]}" > xxx

            # -b ignore whitespace
            # -B ignore blank lines
            # -y do side-by-side comparison
            if ! diff -bBy ${outfile}.expect $outfile > ${outfile}.diff
            then
                printf "FAIL\n"
                minor_sep
                printf "Difference between '%s' and '%s'\n" "${outfile}.expect" "$outfile"
                printf "OUTPUT: EXPECT   vs   ACTUAL\n"
                cat ${outfile}.diff
                minor_sep
                failed=1
            else
                rm ${outfile}.diff
                # printf "'%s' OK " "${outfile}"
            fi
        done

        if [ "$failed" != "1" ]; then
            printf "ALL OK\n"
            ((NPASS++))
        elif [ "$generate" = "1" ]; then
            printf "GENERATE\n"
            echo "${outfiles[*]}"
            paste ${outfiles[*]}
            minor_sep
        fi
        #    printf "\n"

    done

    printf "%s\n" "$major_sep"
    printf "Finished:\n"
    printf "%2d / %2d Normal correct\n" "$NPASS" "$NTESTS"
    printf "\n"
fi

# ================================================================================

if [ "$run_valg" = "1" ]; then

    printf "RUNNING VALGRIND TESTS\n"
    export valg="valgrind"
    export valg_opts="--leak-check=full --show-leak-kinds=all"
    ticktime=$ticktime_valg

    for i in $all_tests; do
        if [[ "${tnames[i]}" == *"shutdown"* ]]; then
            printf "Skipping valgrind %s due to known bugs\n" "${tnames[i]}"
            ((VTESTS--))
            continue
        fi

        printf -v tid "test-%02d" "$i"         # test id for various places
        printf "TEST %2d %-18s : " "$i" "${tnames[i]}"

        # Run the test
        outfiles=()
        eval "${setup[i]}"
        while read -r act; do
            eval "$act"
            tick
        done <<< "${actions[i]}"
        eval "${teardown[i]}"

        # Check each client for failure, print side-by-side diff if problems
        failed=0

        # Check each client for failure, print side-by-side diff if problems
        failed=0
        for ((c=0; c<${#outfiles[*]}; c++)); do
            outfile=${outfiles[c]}
            ./filter-semopen-bug.awk $outfile > xxx # deal with apparent sem_open kernel bug
            mv xxx $outfile
            # Check various outputs from valgrind
            if ! grep -q 'ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)' $outfile ||
                    ! grep -q 'in use at exit: 0 bytes in 0 blocks'  $outfile ||
                    grep -q 'definitely lost: 0 bytes in 0 blocks' $outfile;
            then
                printf "FAIL\nValgrind detected problems in $outfile\n"
                major_sep
                cat $outfile
                major_sep
                failed=1
            fi
        done

        if [ "$failed" != "1" ]; then
            printf "ALL OK"
            ((NVALG++))
        fi
        printf "\n"
    done
    major_sep
    printf "Finished:\n"
    printf "%2d / %2d Valgrind correct\n" "$NVALG" "$VTESTS"
    printf "\n"

fi

major_sep
printf "OVERALL:\n"
printf "%2d / %2d Normal correct\n" "$NPASS" "$NTESTS"
printf "%2d / %2d Valgrind correct\n" "$NVALG" "$VTESTS"
