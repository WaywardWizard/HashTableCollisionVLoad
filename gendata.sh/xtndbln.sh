#1/bin/bash

# this script runs the assignment2 program with a variety of parameters.
# the results go to a CSV file convenient for visualization

initial_sizes=($(seq 1 1 30))
command_set_sizes=($(seq 1000 1000 100000))
output_file="xtndbln.csv"

# Holds commands. Pipe only has 4k.
command_buffer_file="xtndbln.tmp"
table_type="2"

rm ./$output_file

# get the csv header
echo -ne "Inserts [Items],Lookups [Items],Initial Table Size [Slots]," >> ./$output_file
echo -e "n\nq\n" | ./a2 -t $table_type | sed -n '1p' >> ./$output_file

# Test against variety of command sizes
for ncommands in ${command_set_sizes[@]}
do
    # Twice as many inserts
    ninsert=$(echo $ncommands'*2' | bc)

    # Generate a list of commands afresh
    ./cmdgen $ncommands $ninsert 1 > ./$command_buffer_file

    # Put a csv statistic row print command in the command buffer
    sed -in '$d' ./$command_buffer_file
    echo -e "d\nq" >> ./$command_buffer_file

    # Test against a variety of inital table sizes
    for size in ${initial_sizes[@]}
    do
        # Run the program and write stats to output file
        echo -n $ncommands","$ninsert","$size"," >> ./$output_file
        ./a2 -t $table_type -s $size < ./$command_buffer_file >> ./$output_file

    done
done

rm ./$command_buffer_file
exit 0
