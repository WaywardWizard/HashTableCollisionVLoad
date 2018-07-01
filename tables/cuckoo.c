/* * * * * * * * *
 * Dynamic hash table using cuckoo hashing, resolving collisions by switching
 * keys between two tables with two separate hash functions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by ...
 * 14/05/17
 * Ben Tomlin
 * SN: 834198
 * btomlin@student.unimelb.edu.au
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <math.h>

/* Includes inthash.h */
#include "cuckoo.h"

/* A multiple by which to increase a tables size */
#define EXPANSION_FACTOR 2
#define NSTATS 4
#define HEADER_MAX_STR_LENGTH 100

// an inner table represents one of the two internal tables for a cuckoo
// hash table. it stores two parallel arrays: 'slots' for storing keys and
// 'inuse' for marking which entries are occupied
typedef struct inner_table {
	int64 *slots;	// array of slots holding keys
	bool  *inuse;	// is this slot in use or not?
} InnerTable;

// a cuckoo hash table stores its keys in two inner tables
struct cuckoo_table {
	InnerTable *table1; /* array of inner table pointers */
	InnerTable *table2; /* array of inner table pointers */
	int size;	  		// size of each table
    
    int nkeys;
    double accum_lookup_time;
    double accum_insert_time;
};

/* PROTOTYPES */
static InnerTable *new_inner_table(int size);
static bool upsize_inner_table(InnerTable *table, 
                                bool first_table,
                                int size,
                                int factor);

static void free_inner_table(InnerTable* table);
static bool upsize_hash_table(CuckooHashTable *table, int factor);
static int get_cuckoo_index(int size, bool first_table,int64 key);

static InnerTable 
*new_inner_table(int size) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Allocate and initialize an inner-table of 'size'
     *
     * INPT: int size
     *          How many elements the inner table can store
     *
     * OTPT: InnerTable*
     *          A pointer to the newly initialized inner table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    InnerTable *table = malloc(sizeof(*table));
    table->slots=calloc((size_t)size, sizeof(int64));
    table->inuse=calloc((size_t)size, sizeof(bool));
    return(table);
}

static bool upsize_inner_table(InnerTable *table, 
                                bool first_table, 
                                int size, 
                                int factor) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Increase the size of a given inner table by an integer factor.
     *       Rehash all elements of the table where necessary.
     *
     * INPT: InnerTable *table
     *          Pointer to the inner table on which to operate.
     *
     *       bool first_table
     *          Indicate which inner table this is. True for the first inner
     *          table. False for the second. 
     *
     *       int size
     *          How many elements the inner table currently stores
     *
     *       int factor
     *          The factor by which the table will have it's size increased by.
     *          Reccommended to be two. Note this function relys on some
     *          mathematical laws like all things on the universe. The law
     *          requires this be an integer. (For rehashing items)
     *
     * OTPT: bool
     *          Indicates the status of the resize operation. True if resize and
     *          rehash sucessful. False if not.
     *
     * NOTE: 
     *       No need to use the usual insertion procedure as we can guarantee no
     *       collisions provided the new size is an integer multiple of the old
     *       size. This condition is enforced via an (int) type factor argument.
     *       Under such a condition result of the hash value mod the new size 
     *       (hi(key)%new_size) can only be one of the following possibilities:
     *
     *       >) given new_size = k * old_size and k member of {N} where N is 
     *         the set of all natural numbers. (k is the argument factor)
     *
     *       1) The same value as hi(key)%old_size
     *       2) f*old_size + hi(key)%old_size (where f in {1 .. k-1})
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    
    /* Factor needs to be greater than zero. Validate */
    if (factor<0) {return(false);}
    int ix, new_index, new_size=size * factor;
    
    /* Fail the new size will be greater than what is allowed. */
    if (new_size > MAX_TABLE_SIZE) {return(false);}

    /* Realloc the table members and return false if failed */
    table->slots = realloc(table->slots, sizeof(int64)*new_size);
    table->inuse = realloc(table->inuse, sizeof(bool)*new_size);
    if (table->slots == NULL || table->inuse == NULL ) {return(false);}

    /* Initialize usage indicator of new part of inner table */
    for(ix=size; ix<new_size; ix++) {table->inuse[ix]=false;}

    /* Relocate and vacate all keys that have their hash position changed */
    for (ix=0;ix<size;ix++) {

        /* Transfer over key if there is one */
        if (table->inuse[ix]) {

            /* Get new hashmod (index) given the new size */
            new_index=get_cuckoo_index(new_size,first_table,table->slots[ix]);

            /* No change if hashmod with new size gives same result */
            if (ix != new_index) {

                /* Move key to its new location, vacate old slot. */
                table->slots[new_index]=table->slots[ix]; 
                table->inuse[ix]=false;
                table->inuse[new_index]=true;
            }
        }
    }

    return(true);
}

/* Free all memory associated with an inner table */
static void free_inner_table(InnerTable* table) {
    free(table->slots);
    free(table->inuse);
    free(table);
}

/* Get index for a given key and inner table */
static int get_cuckoo_index(int size, bool first_table,int64 key){
    if (first_table) {
        return(h1(key)%size);
    } else {
        return(h2(key)%size);
    }
}

static bool upsize_hash_table(CuckooHashTable *table, int factor) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Increases the size of a CuckooHashTable's inner tables
     *
     * INPT: CuckooHashTable *table
     *          pointer to structure for which inner tables will be doubled.
     *
     * OTPT: bool
     *          true whenever doubling of size was sucessfull, false if not.
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    bool status=false;
    /* Upsize the the inner tables and return status */
    
    /* Lazily upside the tables. Track the sucess of upsize operations.*/
    if (upsize_inner_table(table->table1, true, table->size, factor)) {
        status = (upsize_inner_table(table->table2,false, table->size, factor));
    }

    /* Update the table size */
    table->size*=factor;
    return(status);
}

// initialise a cuckoo hash table with 'size' slots in each table
CuckooHashTable *new_cuckoo_hash_table(int size) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Allocate and initialize cuckoo hash table.
     *
     * INPT: int size
     *          number of elements each table of the cuckoo strucure can
     *          store.
     *
     * OTPT: CuckooHashTable*
     *          initialized, empty cuckoo table pointer
     * 
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Initialize the cuckoo table structure */
    CuckooHashTable *cuckoo_table = malloc(sizeof(*cuckoo_table));
    cuckoo_table->size=size;

    /* Initialize the inner tables */
    cuckoo_table->table1=new_inner_table(size);
    cuckoo_table->table2=new_inner_table(size);

    cuckoo_table->nkeys=0;
    cuckoo_table->accum_lookup_time=0;
    cuckoo_table->accum_insert_time=0;

    return(cuckoo_table);
}

// free all memory associated with 'table'
void free_cuckoo_hash_table(CuckooHashTable *table) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Frees all memory used by a cuckoo table structure
     * 
     * OTPT: NULL
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Free members */
    free_inner_table(table->table1);
    free_inner_table(table->table2);
        
    /* Free structure */
    free(table);
}


bool cuckoo_hash_table_insert(CuckooHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Insert 'key' into 'table', if it's not in there already 
     *
     * INPT: CuckooHashTable *table
     *          table in which key will be inserted
     *
     *       int64 key
     *          the integer key to insert
     *
     * OTPT: bool
     *          indicates insertion sucess if true, otherwise false
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Time insertion */
    int start_time = clock();

    /* Lookup time saved for reset at end of insertion. Count lookup time as
     * part of insertion */
    double original_accum_lookup_time = table->accum_lookup_time;

    /* See if the key is already in the table */
    if (cuckoo_hash_table_lookup(table, key)) {
        return(false);
    }

    /* Helper fn, calculates a kick threshold before increasing cuckoo size */
    int get_kick_threshold(int size) {
        return((int)log2(size)+1);
    }

    /* Helper fn, get usage for a given inner table location */
    bool get_usage(bool first_table, int index) {
        if (first_table) {
            return(table->table1->inuse[index]);
        } else {
            return(table->table2->inuse[index]); }
    }

    /* Helper fn, get a key for a given inner table */
    int64 get_key(bool first_table, int index) {
        if (first_table) {
            return(table->table1->slots[index]);
        } else {
            return(table->table2->slots[index]);
        }
    }

    /* Helper fn, Write key to cuckoo table to a given location */
    void write_key(bool first_table, int index, int64 key) {
        if (first_table) {
            table->table1->slots[index] = key;
            table->table1->inuse[index] = true;
        } else {
            table->table2->slots[index] = key;
            table->table2->inuse[index] = true;
        }
    }

    /* kick_count threshold for expansion of cuckoo hash table. */
    int expansion_threshold = get_kick_threshold(table->size);

    /* Init loop variables */
    bool use_first_table = true;
    int current_index;   // Index for the current key in the current inner table
    int kick_count=0;    // A counter for the number of kicks required on insert
    bool loop_exit = false;
    int64 next_key, current_key=key; 

    /* Insert and cuckoo keys as necessary */
    while (loop_exit == false) {

        /* Loop detection and threshold detection for resizing table */
        /* Loop has occured once loop is back at it's initial state */
        if ( (kick_count > expansion_threshold) || 
             ((kick_count != 0) && (current_key == key) && use_first_table) ) {
            
            /* Upsize hash table, return false if fails */
            if (!(upsize_hash_table(table, (int)EXPANSION_FACTOR))){
                return(false);}

            /* Reset kick count as we are restarting the insertion process with
             * a larger table size*/
            kick_count =0;

            /* Update expansion threshold */
            expansion_threshold = get_kick_threshold(table->size);
        }

        /* Get key index*/
        current_index = get_cuckoo_index(table->size, 
                                         use_first_table,
                                         current_key);
        
        /* Grab kicked key if there is one*/
        if (get_usage(use_first_table, current_index)){

            /* Grab key and increment kick count */
            next_key = get_key(use_first_table, current_index);
            kick_count++;

        } else {

            /* No key left to cuckoo, insertion complete at the end of loop*/
            loop_exit=true;
        }

        /* Write the key to its hashed slot (overwrites whatever was there)*/
        write_key(use_first_table, current_index, current_key);

        /* Update key and the inner table to be used */
        current_key=next_key;
        use_first_table = !(use_first_table);
    }

    table->nkeys++;

    /* Reset lookup time */
    table->accum_lookup_time = original_accum_lookup_time;

    /* Accumulate insertion time */
    table->accum_insert_time += clock()-start_time;

    return(true);
}


bool cuckoo_hash_table_lookup(CuckooHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: lookup whether 'key' is inside 'table'
     * 
     * INPT: CuckooHashTable *table
     *          table in which to look
     *
     *       int64 key
     *          key to be searched for
     *
     * OTPT: bool
     *          true if the key was in 'table', false otherwise
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Time operation */
    int start_time = clock();
    bool found = false;

    /* Lazy check if in either inner table */
    int hash1 = h1(key) % table->size;
    found = (table->table1->inuse[hash1] && table->table1->slots[hash1]==key);

    if (!found) {
        int hash2 = h2(key) % table->size;
        found = (table->table2->inuse[hash2] && table->table2->slots[hash2]==key);
    }

    /* Accumulate lookup time */
    table->accum_lookup_time += clock()-start_time;
    return(found);
}

// print the contents of 'table' to stdout
void cuckoo_hash_table_print(CuckooHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("                    table one         table two\n");
	printf("                  key | address     address | key\n");
	
	// print rows of each table
	int i;
	for (i = 0; i < table->size; i++) {

		// table 1 key
		if (table->table1->inuse[i]) {
			printf(" %20llu ", table->table1->slots[i]);
		} else {
			printf(" %20s ", "-");
		}

		// addresses
		printf("| %-9d %9d |", i, i);

		// table 2 key
		if (table->table2->inuse[i]) {
			printf(" %llu\n", table->table2->slots[i]);
		} else {
			printf(" %s\n",  "-");
		}
	}

	// done!
	printf("--- end table ---\n");
}

// print some statistics about 'table' to stdout
void cuckoo_hash_table_stats(CuckooHashTable *table) {
	fprintf(stderr, "Please see the print csv data functions.\n");
    fprintf(stderr, "Press h for additional csv stat print commands\n");
}

// Returns csv stat header 
static char** get_stat_header() {
    int ix, n_columns = NSTATS;
    char** header= malloc(sizeof(char*)*n_columns);

    /* Allocate memory for header strings */
    for (ix=0; ix<n_columns; ix++) {
        header[ix]=malloc(sizeof(char)*HEADER_MAX_STR_LENGTH);
    }

    strcpy(header[0],"table size [slots]");
    strcpy(header[1],"lookup time [ms]");
    strcpy(header[2],"insert time [ms]");
    strcpy(header[3],"keycount [keys]");

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
static double* get_stat_row(CuckooHashTable* table) {
    int n_columns = NSTATS;
    double* statrow = malloc(sizeof(double) * n_columns);

    double insert_time = 1000*table->accum_insert_time/CLOCKS_PER_SEC;
    long double lookup_time = 1000*table->accum_lookup_time/CLOCKS_PER_SEC;

    statrow[0] = table->size;
    statrow[1] = lookup_time;
    statrow[2] = insert_time;
    statrow[3] = table->nkeys;

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
void cuckoo_hash_table_csv_stats(CuckooHashTable *table) {
    double* stats = get_stat_row(table);
    print_stat_row(stats);
    free(stats);
}

// Get stats header and print to stdout
void cuckoo_hash_table_csv_stats_header (CuckooHashTable *table) {
    char** header =get_stat_header();
    print_stat_header(header);
    free(header);
}

