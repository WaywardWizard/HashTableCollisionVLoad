/* * * * * * * * *
* Dynamic hash table using a combination of extendible hashing and cuckoo
* hashing with a single keys per bucket, resolving collisions by switching keys 
* between two tables with two separate hash functions and growing the tables 
* incrementally in response to cycles
*
* created for COMP20007 Design of Algorithms - Assignment 2, 2017
* 14/05/17
* Ben Tomlin
* SN: 834198
* btomlin@student.unimelb.edu.au
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <math.h>

/* The code for xtndbln.h can be reused */
#include "xtndbln.h"
#include "xuckoo.h"

#define NSTATS 5
#define HEADER_MAX_STR_LENGTH 100

// an inner table is an extendible hash table with an array of slots pointing 
// to buckets holding up to 1 key, along with some information about the number 
// of hash value bits to use for addressing
typedef struct inner_table {
	XtndblNHashTable *xtable;// xtndbln hash table
	int keycount;			 // how many keys are being stored in the table
} InnerTable;

// a xuckoo hash table is just two inner tables for storing inserted keys
struct xuckoo_table {
	InnerTable *table1;
	InnerTable *table2;

    int bucketsize;
    double accum_insert_time; 
    double accum_lookup_time;
};

static InnerTable *new_inner_table(int bucketsize, int (*hash)(int64 key)) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Initializes a new xuckoo inner table
     *
     * INPT: int bucketsize
     *          How many keys buckets of this inner table should hold.
     *
     *       int (*hash)(int64 key)
     *          A function pointer to the hash function to be used for this
     *          inner table
     *
     * OTPT: InnerTable*
     *          A pointer to the newly initialized inner table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    InnerTable *table = malloc(sizeof(*table));
    table->xtable = new_xtndbln_specified_hash_table(bucketsize, hash);
    table->keycount = 0;
    return(table);
}

XuckooHashTable *new_xuckoo_hash_table() {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Initializes a new xuckoo table
     *
     * OTPT: XuckooHashTable*
     *          A pointer to the newly initialized table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    /* Default bucket size for extendable xuckoo table */
    int bucketsize = 1;
    XuckooHashTable *table = malloc(sizeof(*table));
    
    /* Create inner tables, with hash functions as per assignment spec */
    table->table1 = new_inner_table(bucketsize, h1); 
    table->table2 = new_inner_table(bucketsize, h2);

    table->bucketsize = bucketsize;
    table->accum_insert_time = 0;
    table->accum_lookup_time = 0;

	return table;
}

XuckooHashTable *new_xuckoo_hash_table_with_bucketsize(int bucketsize) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Initializes a new xuckoo inner table with a specified bucketsize.
     *
     * INPT: int bucketsize
     *          How many keys buckets of this table should hold.
     *
     * OTPT: InnerTable*
     *          A pointer to the new table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    XuckooHashTable *table = malloc(sizeof(*table));
    table->table1 = new_inner_table(bucketsize, h1); 
    table->table2 = new_inner_table(bucketsize, h2);

    table->bucketsize = bucketsize;
    table->accum_insert_time = 0;
    table->accum_lookup_time = 0;

    return table;
}

/* Free all memory associated with an inner table */
void free_inner_table(InnerTable *table) {
    free_xtndbln_hash_table(table->xtable);
    free(table);
}

// free all memory associated with 'table'
void free_xuckoo_hash_table(XuckooHashTable *table) {
    free_inner_table(table->table1);
    free_inner_table(table->table2);
    free(table);
}

bool xuckoo_hash_table_insert(XuckooHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Insert key to an extendible cuckoo hash table
     *
     * INPT: XuckooHashTable*
     *          Table in which to insert the key
     *
     *       int64 key
     *          Key to be inserted into the table
     *
     * OTPT: none
     *          Mutates table structure
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Time insert operation */
    int start_time = clock();

    /* Save original lookup time accumulator value and reset it at end of
     * function. (Count time of lookup calls made as part of an insert as
     * insertion time)*/
    double original_accum_lookup_time = table->accum_lookup_time;

    /* Return false if key already in the table */
    if (xuckoo_hash_table_lookup(table, key)) { return(false); }

    /* Helper function selects first inner table if 'first_table' else 2nd*/
    InnerTable *get_inner_table(bool first_table) {
        if (first_table) {
            return(table->table1);
        } else {
            return(table->table2);
        }
    }

    /* Helper function finds a kick threshold for the table state at time of
     * call. Kick threshold is log2(mean inner table size) + 1 */
    int get_kick_threshold() {

        /* Find the total size of both extendable N tables */
        int total_size = get_xtndbln_table_size(table->table1->xtable);
        total_size += get_xtndbln_table_size(table->table2->xtable);

        /* Calculate & return the kick threshold. */
        return(log2(total_size));
    }

    /* Find which table has the least keys in it, Attempt to insert here first*/
    bool first_slim = (table->table1->keycount <= table->table2->keycount);
    InnerTable *inner_table = get_inner_table(first_slim);

    int kick_count = 0;
    int kick_threshold = get_kick_threshold();

    /* Cuckoo keys between the inner tables untill there is space to insert or
     * the kick threshold is exceeded */
    while ( (kick_count != kick_threshold) &&
            !xtndbln_hash_table_has_space(inner_table->xtable, key) ) {

        /* Swap(cuckoo) key a random key from the bucket to which key belongs*/
        key = xtndbln_hash_table_rand_swap(inner_table->xtable, key);

        /* Increase the kick count. */
        kick_count++;

        /* Swap insert target table */
        first_slim = !first_slim;
        inner_table = get_inner_table(first_slim);
    }
    
    /* If there is space, perform standard insert operation */
    /* If there is not space, split the bucket as necessary */
    xtndbln_hash_table_insert(inner_table->xtable, key);
    inner_table->keycount++;

    /* Dont double count lookup time (reset it)*/
    table->accum_lookup_time = original_accum_lookup_time;

    /* Add insert time to accumulator */
    table->accum_insert_time += clock() - start_time;

	return true;
}

// lookup whether 'key' is inside 'table' returns true if found, false if not
bool xuckoo_hash_table_lookup(XuckooHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Looks for a given key in a given xuckoo table. Return true if key 
     *       is inside the table
     *
     * INPT: int64 key
     *          The key for search for
     *
     *       XuckooHashTable *table
     *          Table in which to search for the key
     *
     * OTPT: bool
     *          Boolean indicating whether or not the key was found. True if
     *          found. False if not. 
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    /* Time lookup operations */
    int start_time = clock();

    /* Search for key */
    bool status = xtndbln_hash_table_lookup(table->table1->xtable, key);
    status = (status || xtndbln_hash_table_lookup(table->table2->xtable, key));

    /* Save lookup time and return search status */
    table->accum_lookup_time += clock()-start_time;
    return(status);
}

// print the contents of 'table' to stdout
void xuckoo_hash_table_print(XuckooHashTable *table) {
	assert(table != NULL);

	printf("--- table ---\n");

	// loop through the two tables, printing them
	InnerTable *innertables[2] = {table->table1, table->table2};
	int t;
	for (t = 0; t < 2; t++) {
		// print header
		printf("table %d\n", t+1);

        // print table
        xtndbln_hash_table_print_sparse(innertables[t]->xtable);
	}
	printf("--- end table ---\n");
}

// print some statistics about 'table' to stdout
void xuckoo_hash_table_stats(XuckooHashTable *table) {
	fprintf(stderr, "Please see the print csv data functions.\n");
    fprintf(stderr, "Press h for additional csv stat print commands\n");
	return;
}

// Returns csv stat header 
static char** get_stat_header() {
    int ix, n_columns = NSTATS;
    char** header= malloc(sizeof(char*)*n_columns);

    /* Allocate memory for header strings */
    for (ix=0; ix<n_columns; ix++) {
        header[ix]=malloc(sizeof(char)*HEADER_MAX_STR_LENGTH);
    }

    strcpy(header[0],"bucket count [buckets]");
    strcpy(header[1],"bucket size [slots]");
    strcpy(header[2],"key count [keys]");
    strcpy(header[3],"lookup time [ms]");
    strcpy(header[4],"insert time [ms]");

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
static double* get_stat_row(XuckooHashTable* table) {
    int n_columns = NSTATS;
    double* statrow = malloc(sizeof(double) * n_columns);

    double insert_time = 1000*table->accum_insert_time/CLOCKS_PER_SEC;
    long double lookup_time = 1000*table->accum_lookup_time/CLOCKS_PER_SEC;
    int size = get_xtndbln_table_size(table->table1->xtable);
    size+=get_xtndbln_table_size(table->table2->xtable);

    int nkeys = table->table1->keycount;
    nkeys+= table->table2->keycount;

    statrow[0] = size;
    statrow[1] = table->bucketsize;
    statrow[2] = nkeys;
    statrow[3] = lookup_time;
    statrow[4] = insert_time;

    return(statrow);
}

// Print statistics to stdout
static void print_stat_row(double *statrow) {
    char seperator = ',';
    int ix, n_columns=NSTATS;
    for (ix=0; ix<n_columns;ix++) {
        printf("%3.9f", statrow[ix]);
        if (ix!=(n_columns-1)) {
            printf("%c", seperator);
        }
    }
    printf("\n");
}

// Get stats and print to stdout
void xuckoo_hash_table_csv_stats(XuckooHashTable *table) {
    double* stats = get_stat_row(table);
    print_stat_row(stats);
    free(stats);
}

// Get stats header and print to stdout
void xuckoo_hash_table_csv_stats_header (XuckooHashTable *table) {
    char** header =get_stat_header();
    print_stat_header(header);
    free(header);
}
