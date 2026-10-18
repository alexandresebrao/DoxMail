#!/bin/bash
# sample-stacks.sh SECONDS INTERVAL CMD...: runs CMD and samples the
# main thread stack with eu-stack every INTERVAL seconds ("poor man's
# profiler"); prints each sample, separated by "--", for counting
secs=$1; interval=$2; shift 2
"$@" >/dev/null 2>&1 &
pid=$!
end=$((SECONDS + secs))
while [ $SECONDS -lt $end ] && kill -0 $pid 2>/dev/null; do
	eu-stack -1 -n 60 -p $pid 2>/dev/null | awk 'NR>1 && /^#/ {print $3}'
	echo "--"
	sleep "$interval"
done
kill $pid 2>/dev/null
