#!/bin/bash 

file_num=50
file_size_max=6000
test_passed=0
folder=tests/test1
socket_name=socket_t1
file_list=

mkdir $folder
mkdir $folder/testfiles
mkdir $folder/testreadN
mkdir $folder/testread
mkdir $folder/testRemoved

tests/createFiles.sh $file_num $file_size_max $folder

for((i=1;i<=$file_num;i++)); do
    file_list+=",$folder/testfiles/file$i"
done

valgrind --leak-check=full -s ./server $1 > $folder/server_out &
server_pid=$!


./client -p -t 200 -h
./client -p -t 200 -f $socket_name -w "$folder/testfiles" -R -d $folder/testreadN

./client -p -t 200 -f $socket_name -W $file_list -r $file_list -d "$folder/testread" -c $file_list -R n=$file_num -d "$folder/testRemoved" 

kill -s SIGHUP ${server_pid}
echo "sent SIGHUP!"

diff $folder/testfiles $folder/testread > $folder/diff_file
if [ -s $folder/diff_file ];then
    test_passed=1    
fi

diff $folder/testfiles $folder/testreadN > $folder/diff_file
if [ -s $folder/diff_file ];then
    test_passed=1
fi

if find $folder/testRemoved -mindepth 1 -maxdepth 1 | read ; then
   test_passed=1 
fi

if [ $test_passed -eq 0 ];then
    echo "Test Passed!"
else
    echo "Test Failed!"
fi

