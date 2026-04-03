#!/bin/bash
# run_grade.sh  –  One-shot: generate + build + run for a trace file.
#
# Usage:  ./run_grade.sh traces/short1.rep [weight]
#         ./run_grade.sh traces/short1.rep 0.5
 
set -e
 
TRACE=${1:?Usage: $0 trace.rep [weight]}
WEIGHT=${2:-0.6}
 
echo "==> Generating trace_ops.c from $TRACE"
./gen_trace "$TRACE" > trace_ops.c
 
echo "==> Building grader"
gcc -Wall -O2 -o grader grader.c trace_ops.c mm.c memlib.c -lm
 
echo "==> Running"
./grader "$WEIGHT"
 