#!/bin/bash 
file_num=300
file_size_max=4000
folder="tests/test2"
socket=socket_t2

mkdir $folder
mkdir $folder/testfiles
mkdir $folder/testreadN

./tests/createFiles.sh $file_num $file_size_max $folder

./server tests/configs/config_t2.txt  > $folder/server_out &
server_pid=$!

./client -p -t 200 -h
./client -p -t 200 -f $socket  -w "$folder/testfiles" > $folder/fileswritten
./client -p -t 200 -f $socket -R -d "$folder/testreadN" > $folder/filesread

grep "Deleting file"* $folder/server_out  | cut -d "-" -f 2  > $folder/filesRemoved 
grep "received file" $folder/filesread | cut -d "-" -f 2  > $folder/filesreceived
grep -Ff $folder/filesreceived $folder/filesRemoved > $folder/filesNotRemoved
if [ -s "$folder/filesNotRemoved" ];then
    echo "Test2 Failed!"
else
    echo "Test2 Passed!"
fi

kill -s SIGQUIT ${server_pid}

