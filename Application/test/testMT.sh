#!/bin/bash

NCPU=$(cat /proc/cpuinfo | grep processor | wc -l)

TESTNUM=$(./setupTest.sh)

for ((th=1;th<=$NCPU;th++)); do
	# use other CPUs
	declare -A PIDS
	for ((busy=$((th+1));busy<=$NCPU;busy++)); do
		yes >& /dev/null &
		PIDS[$busy]=$!
	done

	# run test
	./runTest.sh -t $TESTNUM -a "particle=electron mult=2 energy=50 maxEvents=100 sim=GeantV year=2018 threads=$th"
	TESTEXIT=$?
	if [[ $TESTEXIT -ne 0 ]]; then
		exit $TESTEXIT
	fi

	# kill busy processes
	for PID in ${PIDS[@]}; do
		kill $PID >& /dev/null
		wait $PID >& /dev/null
	done
done

./cleanupTest.sh -t $TESTNUM
