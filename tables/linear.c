/* * * * * * * * *
 * Dynamic hash table using linear probing to resolve collisions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Matt Farrugia <matt.farrugia@unimelb.edu.au>
 *
 * Modified by
 * 14/05/17
 * Ben Tomlin
 * SN: 834198
 * btomlin@student.unimelb.edu.au
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "linear.h"

// how many cells to advance at a time while looking for a free slot
#define STEP_SIZE 1
// Number of columns in the stat output csv
#define NSTATS 9
#define HEADER_MAX_STR_LENGTH 100

// a hash table is an array of slots holding keys, along with a parallel array
// of boolean markers recording which slots are in use (true) or free (false)
// important because not-in-use slots might hold garbage data, as they may
// not have been initialised
struct linear_table {
	int64 *slots;	// array of slots holding keys
	bool  *inuse;	// is this slot in use or not?
	int size;		// the size of both of these arrays right now
	int load;		// number of keys in the table right now
    int preexist;

    /* Count of collisions on insert of keys */
    int collision_count;

    /* Average probe length for collision inserts*/
    double average_probe_length;
    double average_load_factor;

    double accum_lookup_time;
    double accum_insert_time;
};


/* * * *
 * helper functions
 */

// set up the internals of a linear hash table struct with new
// arrays of size 'size'
static void initialise_table(LinearHashTable *table, int size) {
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->slots = malloc((sizeof *table->slots) * size);
	assert(table->slots);
	table->inuse = malloc((sizeof *table->inuse) * size);
	assert(table->inuse);
	int i;
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
	}

	table->size = size;
	table->load = 0;
    table->collision_count = 0;
    table->average_probe_length=0;
    table->average_load_factor=0;
    table->accum_lookup_time=0;
    table->accum_insert_time=0;
    table->preexist=0;
}


// double the size of the internal table arrays and re-hash all
// keys in the old tables
static void double_table(LinearHashTable *table) {
	int64 *oldslots = table->slots;
	bool  *oldinuse = table->inuse;
	int oldsize = table->size;

	initialise_table(table, table->size * 2);

	int i;
	for (i = 0; i < oldsize; i++) {
		if (oldinuse[i] == true) {
			linear_hash_table_insert(table, oldslots[i]);
		}
	}

	free(oldslots);
	free(oldinuse);
}


/* * * *
 * all functions
 */

// initialise a linear probing hash table with initial size 'size'
LinearHashTable *new_linear_hash_table(int size) {
	LinearHashTable *table = malloc(sizeof *table);
	assert(table);

	// set up the internals of the table struct with arrays of size 'size'
	initialise_table(table, size);

	return table;
}


// free all memory associated with 'table'
void free_linear_hash_table(LinearHashTable *table) {
	assert(table != NULL);

	// free the table's arrays
	free(table->slots);
	free(table->inuse);

	// free the table struct itself
	free(table);
}


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool linear_hash_table_insert(LinearHashTable *table, int64 key) {
    /* CPU time for insert operation */
    int start_time = clock();
	assert(table != NULL);

	// need to count our steps to make sure we recognise when the table is full
	int steps = 0;

	// calculate the initial address for this key
	int h = h1(key) % table->size;

	// step along the array until we find a free space (inuse[]==false),
	// or until we visit every cell
	while (table->inuse[h] && steps < table->size) {
		if (table->slots[h] == key) {
			// this key already exists in the table! no need to insert
            table->preexist++;
			return false;
		}
		
		// else, keep stepping through the table looking for a free slot
		h = (h + STEP_SIZE) % table->size;
		steps++;
	}

    /* If we had a collision increment the collision count */
    if (steps) {
        table->collision_count++;
    }

    /* Update average probe length */
    table->average_probe_length = 
        (((table->average_probe_length*(double)(table->load))+((double)steps))/
            (double)(1+table->load));
    
   
	// if we used up all of our steps, then we're back where we started and the
	// table is full
    /* Could be >= to support STEP_SIZE > 1 */
	if (steps == table->size) {
		// let's make some more space and then try to insert this key again!
		double_table(table);
        
        //* Add insert time so far */
        table->accum_insert_time += clock() - start_time;
		return linear_hash_table_insert(table, key);

	} else {
		// otherwise, we have found a free slot! insert this key right here
		table->slots[h] = key;
		table->inuse[h] = true;
		table->load++;

        /* Get average load factor */
        table->average_load_factor = ((table->average_load_factor*
                ((double)(table->load -1)/(table->load))) +
                ((double)(1)/(table->size))); 

        /* Add insert time */
        table->accum_insert_time += clock() - start_time;
		return true;
	}
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool linear_hash_table_lookup(LinearHashTable *table, int64 key) {
    int start_time = clock();
	assert(table != NULL);

	// need to count our steps to make sure we recognise when the table is full
	int steps = 0;

	// calculate the initial address for this key
	int h = h1(key) % table->size;

	// step along until we find a free space (inuse[]==false), or until we
	// visit every cell
	while (table->inuse[h] && steps < table->size) {

		if (table->slots[h] == key) {

			// found the key!. Add time to accumulator 
            table->accum_lookup_time+=clock()-start_time;
			return true;
		}

		// keep stepping
		h = (h + STEP_SIZE) % table->size;
		steps++;
	}

	// we have either searched the whole table or come back to where we started
	// either way, the key is not in the hash table
	return false;
}


// print the contents of 'table' to stdout
void linear_hash_table_print(LinearHashTable *table) {
	assert(table != NULL);

	printf("--- table size: %d\n", table->size);

	// print header
	printf("   address | key\n");

	// print the rows of the hash table
	int i;
	for (i = 0; i < table->size; i++) {
		
		// print the address
		printf(" %9d | ", i);

		// print the contents of the slot
		if (table->inuse[i]) {
			printf("%llu\n", table->slots[i]);
		} else {
			printf("-\n");
		}
	}

	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void linear_hash_table_stats(LinearHashTable *table) {
    assert(table != NULL);

    double load_factor = 100*table->load/table->size;
    double overall_average_probe_length = 
            (table->average_probe_length *
            ((double)table->collision_count)/table->load);

    double insert_time = table->accum_insert_time/CLOCKS_PER_SEC;
    double lookup_time = table->accum_lookup_time/CLOCKS_PER_SEC;

	printf("--- table stats ---\n");
	// print some information about the table
	printf("%20s: %d %s\n", "current size", table->size, "slots");
	printf("%20s: %d %s\n", "current load", table->load, "items");
	printf("%20s: %.3f%%\n", "load factor", load_factor);
	printf("%20s: %d\n", "step size", STEP_SIZE);
	printf("%20s: %d\n", "collision count", table->collision_count);

	printf("%20s: %3.1f\n", 
            "collision probe length",
            table->average_probe_length);
    printf("%20s: %3.1f\n",
            "average probe length",
            overall_average_probe_length);
	
	printf("%20s: %.3f [%s]\n", "lookup time", 1000*lookup_time, "ms");
	printf("%20s: %.3f [%s]\n", "insert time", 1000*insert_time, "ms");
	printf("%20s: %d\n", "preexisted", table->preexist);
	printf("--- end stats ---\n");
}

// Returns csv stat header 
static char** get_stat_header() {
    int ix, n_columns = NSTATS;
    char** header= malloc(sizeof(char*)*n_columns);

    /* Allocate memory for header strings */
    for (ix=0; ix<n_columns; ix++) {
        header[ix]=malloc(sizeof(char)*HEADER_MAX_STR_LENGTH);
    }

    strcpy(header[0],"current size [slots]");
    strcpy(header[1],"current load [items]");
    strcpy(header[2],"load factor [-]");
    strcpy(header[3],"step size [-]");
    strcpy(header[4],"collision count [inserts]");
    strcpy(header[5],"average probe length [items]");
    strcpy(header[6],"lookup time [ms]");
    strcpy(header[7],"insert time [ms]");
    strcpy(header[8],"average load factor [-]");

    return(header);
}

// Print csv stat header to stdout
static void print_stat_header(char** header) {
    int ix, n_columns=NSTATS;
    char seperator=',';
    for (ix=0; ix<n_columns; ix++) {
        printf("%s", header[ix]);
        if (ix!=(n_columns-1)) {
            printf("%c", seperator);
        }
    }
    printf("\n");
}

// Get a csv row of statistics for the table 
static double* get_stat_row(LinearHashTable *table) {
    int n_columns = NSTATS;
    double* statrow = malloc(sizeof(double) * n_columns);

    double load_factor = 100*table->load/table->size;
    double average_load = table->average_load_factor;
    double insert_time = 1000*table->accum_insert_time/CLOCKS_PER_SEC;
    double lookup_time = 1000*table->accum_lookup_time/CLOCKS_PER_SEC;


    statrow[0] = table->size;
    statrow[1] = table->load;
    statrow[2] = load_factor;
    statrow[3] = STEP_SIZE;
    statrow[4] = table->collision_count;
    statrow[5] = table->average_probe_length;
    statrow[6] = lookup_time;
    statrow[7] = insert_time;
    statrow[8] = average_load;

    return(statrow);
}

// Print statistics to stdout
static void print_stat_row(double *statrow) {
    char seperator = ',';
    int ix, n_columns=NSTATS;
    for (ix=0; ix<n_columns;ix++) {
        printf("%3.3f", statrow[ix]);
        if (ix!=(n_columns-1)) {
            printf("%c", seperator);
        }
    }
    printf("\n");
}

// Get stats and print to stdout
void linear_hash_table_csv_stats(LinearHashTable *table) {
    double* stats = get_stat_row(table);
    print_stat_row(stats);
    free(stats);
}

// Get stats header and print to stdout
void linear_hash_table_csv_stats_header (LinearHashTable *table) {
    char** header =get_stat_header();
    print_stat_header(header);
    free(header);
}
