#!/bin/bash
#creates $1 files of size between 1 and $2 
mkdir "$3/testfiles"
mkdir "$3/testread"
mkdir "$3/testreadN"

for((i=1;i<=$1;i++)); do
    size=$((($RANDOM%$2)+1))
    touch "$3/testfiles/file$i"
    head -c $size </dev/urandom>"$3/testfiles/file$i"
done


