#!/bin/bash

#cd /Users/Arun/Documents/Computer\ Science/Year\ 3/Networks
counter=0
echo "${res}"
for i in `seq 1 100`;
do
	res0=$(./client localhost 2997 /Users/Arun/Desktop/test4.txt)
	echo "$res0"
	if [[ "$res0" == *"Match"* ]]; then
		counter=$((counter+1))
	fi
done    

echo "Counter is $counter"



