#!/bin/sh

input="$TEST_DIR/$1.c"
output_file="$1-$2-$3-$4-$5"
baseline="$BASELINE_DIR/$1.out"
dir="$BUILD_DIR/all/$1"
bin="$dir/$output_file"
log="$bin.log"
output="$bin.out"
diff="$bin.diff"

# Compiling
mkdir -p $dir
$2 $CFLAGS -Werror -$3 -$4 -std=$5 -o $bin $input > $log 2>&1
if [ $? -ne 0 ]; then
    echo "[COMPILATION FAILED] Log: $log. Rerun test: make $bin"
    exit 1
fi

# Running
./$bin > $output 2>&1
if [ $? -ne 0 ]; then
    rm $bin
    echo "[TEST FAILED] Log: $log. Rerun test: make $bin"
    exit 1
fi
cat $output >> $log

# Comparing
if [ ! -f $baseline ]; then
    echo "[WARNING] There is no baseline for $1!"
    echo "[TEST PASSED] $output_file: ?";
    exit 0
fi

sed -E "s/0x[0-9a-fA-F]{6,16}/POINTER/" -i $baseline -i $output
similarity=$(wdiff -123 --statistics $baseline $output | tail -n 1 | \
    awk -F ':' '{print $NF}' | awk -F ' ' '{print $4}' | sed 's/%$//')
echo "Similiarity is $similarity%" >> $log
if [ $similarity -lt 95 ]; then
    wdiff $baseline $output > $diff
    echo "[DIFF FAILED] Similarity is $similarity%. Diff: $diff. Log: $log. Rerun test: make $bin"
    exit 1
fi

echo "[TEST PASSED] $output_file: $similarity%";
