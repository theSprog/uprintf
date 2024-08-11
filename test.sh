#!/bin/bash

input="$TEST_DIR/$1.c"
output_file="$1-$2-$3-$4"
baseline="$BASELINE_DIR/$1.out"
dir="$BUILD_DIR/test/$1"
bin="$dir/$output_file"
failed="$bin.failed"
log="$bin.log"
output="$bin.out"
diff="$bin.diff"

# Some tests may not match baseline exactly, so they get custom target similarity percentage.
function get_similarity {
    # These tests contain stdint.h types which have different typenames in gcc and clang
    if   [ "$1" = "struct" ]; then echo 90;
    elif [ "$1" = "packed" ]; then echo 90;
    # FILE has different implementation which is up to stdio.h and it often has pointers
    # that point out-of-bounds causing uprintf to print garbage from the memory, thus the
    # primary goal of the test is to check that there are no errors, i.e. segfaults, leaks.
    elif [ "$1" = "stdio_file" ]; then echo 1;
    else echo 100; fi
}

# Compiling
mkdir -p $dir
$2 $CFLAGS -Werror -$3 -$4 -o $bin $input > $log 2>&1
if [ $? -ne 0 ]; then
    echo "[COMPILATION FAILED] Log: $log. Rerun test: make $bin"
    exit 1
fi

# Running
./$bin > $output 2>&1
if [ $? -ne 0 ]; then
    mv $bin $failed
    echo "[TEST FAILED] Log: $log. Failed test binary: $failed. Rerun test: make $bin"
    exit 1
fi
cat $output >> $log

# Comparing
if [ ! -f $baseline ]; then
    echo "[WARNING] There is no baseline for $1!"
    echo "[TEST PASSED] $output_file: ?";
    exit 0
fi

sed -E "s/0x[0-9a-fA-F]{6,16}/POINTER/g" -i $baseline -i $output
similarity=$(wdiff -123 --statistics $baseline $output | \
    awk -F ':' '{print $NF}' | awk -F ' ' '{print $4}' | \
    sed 's/%$//' | sort | tail -n 1)
echo "Similarity is $similarity%" >> $log
if [ $similarity -lt $(get_similarity $1) ]; then
    wdiff $baseline $output > $diff
    echo "[DIFF FAILED] Similarity is $similarity%. Diff: $diff. Log: $log. Rerun test: make $bin"
    exit 1
fi

echo "[TEST PASSED] $output_file: $similarity%";
