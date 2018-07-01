/* * * * * * * * *
 * Dynamic hash table using extendible hashing with multiple keys per bucket,
 * resolving collisions by incrementally growing the hash table
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 *
 * 14/05/17
 * Ben Tomlin
 * SN: 834198
 * btomlin@student.unimelb.edu.au
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "xtndbln.h"

/* Number of stats there are to print */
#define NSTATS 5
#define HEADER_MAX_STR_LENGTH 100

/* Function like macro for getting trailing nbit integer */
#define get_bit_trail(x, n) (x) & ((1<<n) - 1)

/* This will be limited by the integer representation. We assume there is no
 * overflow, and that if the underlying representation is twos complement that,
 * our argument remains in the positive domain of the representation. (Positive
 * domain is defined as the domain for which the function range is positive) */
#define pow2(x) (1<<x)

// a bucket stores an array of keys
// it also knows how many bits are shared between possible keys, and the first 
// table address that references it
typedef struct xtndbln_bucket {
	int id;			// a unique id for this bucket, equal to the first address
					// in the table which points to it
	int depth;		// how many hash value bits are being used by this bucket
	int nkeys;		// number of keys currently contained in this bucket
	int64 *keys;	// the keys stored in this bucket
} Bucket;

// a hash table is an array of slots pointing to buckets holding up to 
// bucketsize keys, along with some information about the number of hash value 
// bits to use for addressing
struct xtndbln_table {
	Bucket **buckets;	// array of pointers to buckets
	int size;			// how many entries in the table of pointers (2^depth)
	int depth;			// how many bits of the hash value to use (log2(size))
	int bucketsize;		// maximum number of keys per bucket

    int nitems;
    double accum_insert_time;
    long double accum_lookup_time;
    int (*hash)(int64 key);   // Pointer to hash function this table uses
};

static Bucket *get_new_bucket(int id, int bucketsize, int depth); 
static void free_bucket(Bucket* bucket);
static void write_to_bucket(Bucket *bucket, int64 key);
static int64 swap_bucket_key(Bucket *bucket, int64 key, int index);
static bool split_bucket(XtndblNHashTable *table, int address);
static bool double_table(XtndblNHashTable *table);
static void reinsert_key(XtndblNHashTable *table, int64 key);

static Bucket *get_new_bucket(int id, int bucketsize, int depth) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Initializes a new bucket structure
     *
     * INPT: int bucketsize
     *          How many keys the bucket can hold
     *
     * OTPT: Bucket*
     *          A pointer to the newly initialized bucket
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Allocate memory for bucket structure */
    Bucket *bucket = malloc(sizeof(*bucket));
    assert(bucket);

    /* Allocate memory for bucket keys */
    bucket->keys = malloc(sizeof(*bucket->keys)*bucketsize);
    assert(bucket->keys);
    bucket->depth = depth;
    bucket->id = id;
    bucket->nkeys = 0;
    return(bucket);
}

// Free all memory associated with a bucket structure 
static void free_bucket(Bucket *bucket) {
    free(bucket->keys);
    free(bucket);
}

static int64 swap_bucket_key(Bucket *bucket, int64 key, int index) {
     /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Swapout key in bucket at keyslot 'index' with 'key'. Return the key
     *       swapped out. 
     *
     * INPT: Bucket *bucket
     *          The address of the bucket in which the keyswap will happen.
     *
     *       int64 key
     *          The key which will be swapped into the bucket. 
     *
     *       int index
     *          Zero base index of the already existing key which the new key
     *          will be swapped for. 
     *
     * OTPT: int64
     *          The key which has been swapped out. 
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    int64 swapped_key = bucket->keys[index];
    bucket->keys[index]=key;
    return(swapped_key);
}

static void write_to_bucket(Bucket *bucket, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Write key into bucket.
     *
     * INPT: Bucket *bucket
     *          The address of the bucket inwhich the key will be  inserted. 
     *
     * OTPT: key
     *          The key which will be inserted into the magic bucket. 
     *
     * NOTE: Assumes the bucket has space for the key. If not, behaviour
     *       undefined. 
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    bucket->keys[bucket->nkeys++] = key;
}

/* Writes key into table under assumption that the table has space */
static void reinsert_key(XtndblNHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Re-insert key into table.
     *
     * INPT: XtndblNHashTable *table
     *          The address of the hash table structure in which the key will be
     *          reinserted. 
     *
     *       int64 key
     *          The key which is to be inserted
     *
     * NOTE: Assumes the table has space for the key. If not behaviour is
     *       undefined. 
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    int index = get_bit_trail(table->hash(key), table->depth);
    Bucket *bucket =table->buckets[index];
    write_to_bucket(bucket, key);
}
    
static bool split_bucket(XtndblNHashTable *table, int bucket_index) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Takes a bucket that is full and splits it. Will grow the hash table
     *       if necessary. Updates the hash table pointers where appropriate. 
     *       After growing the table and splitting the bucket will re-hash the 
     *       contents of the bucket to ensure they are in the correct location.
     *
     * INPT: XtndblNHashTable *table
     *          Address of table structure which contains the bucket to be
     *          split.
     *
     *       int address
     *          Table index of the bucket to be split. 
     *
     * OTPT: bool
     *          True if bucket was split sucessfully. False otherwise.
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    
    /* Double the table if there are no further available pointers */
    if(table->buckets[bucket_index]->depth == table->depth) {
        /* Double the table. Return false if the operation fails. */
        if(!double_table(table)) {return(false);} } 

    Bucket *old_bucket = table->buckets[bucket_index];

    /* New bucket id is the old bucket binary id (of depth bits) prefixed with
     * an additional true bit. */
    int id = (old_bucket->id | pow2(old_bucket->depth));
    assert(id == old_bucket->id + pow2(old_bucket->depth));
    
    /* Increase old bucket depth by one, set new bucket depth to result. */
    int depth = ++(old_bucket->depth);
    assert(depth == old_bucket->depth);

    /* Create the new bucket */
    Bucket *new_bucket = get_new_bucket(id, table->bucketsize, depth);
    if (!new_bucket) { return(false); }


    /* Calculate the number of unused bits in the table for newly split bucket*/
    int ix, prefix, unused_bits = (table->depth - depth);

    /* Reroute hash table pointers where appropriate to the new bucket. */
    /* Generate all possible extraneous bit prefixes for the new bucket & set
     * the hash table bucket addresses that correspond to the concatenation of
     * these prefixes and the new bucket id (base address)*/
    for (ix=0;ix<pow2(unused_bits); ix++) {
        
        /* Align extraneous bit prefix to new bucket id */
        prefix = ix<<(depth);

        /* BIT-OR extraneous prefix and id, and set the bucket pointer */
        table->buckets[(id|prefix)] = new_bucket;
    }

    /* Relocate the keys in the full bucket as appropriate. */
    /* Overwrite keys as we go. Keys retrieved before overwriting */
    old_bucket->nkeys = 0;
    for(ix=0; ix<table->bucketsize; ix++) {
        reinsert_key(table, old_bucket->keys[ix]);
    }

    return(true);
}

static bool double_table(XtndblNHashTable *table) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Doubles the size of an extendible hash table. The extra slots
     *       created will duplicate the pointers in the first half. 
     *
     * INPT: XtndblNHashTable *table
     *          The table to double in size
     *
     * OTPT: bool
     *          Indicates whether or not the doubling operation was successful.
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Operation fails if expanding table over max table size.*/
    if ((table->size*2) > MAX_TABLE_SIZE) {
        return(false);}

    /* realloc buckets. double size. Increase depth. copy first half to second*/
    table->buckets = realloc(table->buckets, 
                             (sizeof(*table->buckets))*2*(table->size));

    /* Doubling operation fails if cannot get memory */
    if (table->buckets == NULL) {
        return(false);}

    /* Copy over the content of the first half of the table */
    /* Assumption that memory is allocated in a contiguous manner.*/
    memcpy(table->buckets + table->size, 
            table->buckets,
            sizeof(*table->buckets)*(table->size));

    table->size*=2;
    table->depth++;
    return(true);
}

/* Return the size of a table.*/ 
int get_xtndbln_table_size(XtndblNHashTable *table) {return(table->size);}

XtndblNHashTable 
*new_xtndbln_specified_hash_table(int bucketsize, int (*hash)(int64 key)) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: More general extendable hash table initialization wrapper that
     *       initializes an extendable hash table with a specified hash function
     *
     * INPT: int bucketsize
     *          How many keys each bucket of the table can store 
     *
     *       int *hash(int64 key)
     *          Function pointer to the hash function this table is to
     *          use. 
     *
     * OTPT: XtndblNHashTable*
     *          A pointer to the newly initialized hash table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    XtndblNHashTable *table = new_xtndbln_hash_table(bucketsize);

    /* Set the hash function as per argument */
    table->hash = hash;
    return(table);
}

// initialise an extendible hash table with 'bucketsize' keys per bucket
XtndblNHashTable *new_xtndbln_hash_table(int bucketsize) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Initializes a new extendable hash table structure
     *
     * INPT: int bucketsize
     *          How many elements each bucket can store
     *
     * OTPT: XtndblNHashTable*
     *          A pointer to the newly initialized table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    
    /* Allocate memory for table */
    XtndblNHashTable *table = malloc(sizeof(*table));
    assert(table);

    table->bucketsize = bucketsize;
    table->depth=0;
    table->size=1;
    table->nitems=0;
    table->accum_lookup_time=0;
    table->accum_insert_time=0;

    /* Default to h1 as per assignment spec */
    table->hash=h1;

    /* Allocate memory for bucket pointers */
    table->buckets = malloc(sizeof(*table->buckets)*table->size);
    assert(table->buckets);

    /* Init bucket */
    table->buckets[0] = get_new_bucket(0, bucketsize, 0);

	return(table);
}
void free_xtndbln_hash_table(XtndblNHashTable *table) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Frees all memory associated with an extendableN table structure.
     *
     * INPT: XtndblNHashTable *table
     *          How many elements the inner table can store
     *
     * OTPT: void
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    int jx, ix, extraneous_bits, prefix;
    Bucket *bucket;

    /* Free all buckets */
    for(ix=0;ix<table->size;ix++) {
        bucket=table->buckets[ix];

        /* Free bucket if not null (ie already freed)*/
        if (bucket) {
            extraneous_bits = (table->depth - bucket->depth); 

            /* Null all table pointers to this bucket */
            for (jx=0; jx<pow2(extraneous_bits);jx++) {
                prefix = (jx << bucket->depth);
                table->buckets[(ix | prefix)]=NULL;
            }
            free_bucket(bucket);
        }
    }
    free(table->buckets);
    free(table);
}

bool xtndbln_hash_table_has_space(XtndblNHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Checks whether the bucket in which this key will be placed has
     *       space for the key.
     *
     * INPT: int size
     *          How many elements the inner table can store
     *
     * OTPT: InnerTable*
     *          A pointer to the newly initialized inner table
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    int index = get_bit_trail(table->hash(key), table->depth);
    return(table->buckets[index]->nkeys < table->bucketsize);
}

bool xtndbln_hash_table_insert(XtndblNHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Inserts a key into a given extendable hash table 
     *
     * INPT: XtndblNHashTable *table
     *          A pointer to the table structure in which the key is to be
     *          inserted.
     *
     *       int64 key
     *          Key which is to be inserted.
     *
     * OTPT: bool
     *          True if the insertion was sucessful, false if the given key was
     *          already in the table.
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Use this to reset the lookup time */
    double original_accum_lookup_time = table->accum_lookup_time;

    /* Start cpu timer for insert */
    int start_time = clock();

    /* Check to see if the key is already contained in the table */
    if (xtndbln_hash_table_lookup(table, key)) {return false;}

    /* Store the hash to reduce overhead */
    int key_hash = table->hash(key);
    Bucket *bucket;

    /* Helper. Checks if a bucket is full or not. return true if full */
    bool bucket_full(Bucket *bucket) {
        return(bucket->nkeys == table->bucketsize);}

    /* Helper. Return the address of the bucket in which a key should go*/
    Bucket *update_bucket() {
        return(table->buckets[get_bit_trail(key_hash, table->depth)]);}

    /* Split bucket (& possibly double table) untill it has room for the key 
     *
     * Update the bucket after every split as pointer for a given table address
     * may have been changed by the split operation 
     *
     * Note that even after a split everything might rehash to the same bucket,
     * and thus a while loop is used to split however many times required to 
     * make space. */
    while (bucket_full(bucket = update_bucket())) {

        /* Split bucket at address where the key belongs for the current table*/
        split_bucket(table, get_bit_trail(key_hash, table->depth));
    }

    /* Put the key in the bucket (At present it is invariant the bucket has
     * space, meeting write_to_bucket()'s condition of invocation)*/
    write_to_bucket(bucket, key);

    /* Lookup time counted for insertion, so dont change it */
    table->accum_lookup_time = original_accum_lookup_time;

    /* Increment keycount for table */
    table->nitems++;

    table->accum_insert_time += (clock()-start_time);

    return(true);
}

int64 xtndbln_hash_table_rand_swap(XtndblNHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Swap a key with a another at random from the bucket to which it
     *       hashes. Return the key swapped out. Assumes the bucket to which the
     *       key hashes is not empty.
     *
     * INPT: XtndblNHashTable *table
     *          Pointer to the table swap insert. 
     *
     *       int64 key
     *          Key which to insert
     *
     * OTPT: int64
     *          The key which was swapped out. 
     *
     * NOTE: Asserts that there is a key in the bucket.
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    int swapkey_bucket_index, key_hash = table->hash(key);

    /* Get bucket into which the key will be swapped*/
    int bucket_index = get_bit_trail(key_hash, table->depth);
    Bucket *bucket = table->buckets[bucket_index];

    /* There must be at least one key in the bucket to swap*/
    assert(bucket->nkeys>0);

    /* Swap-out key with one in the bucket at random and return swapped key */
    swapkey_bucket_index = rand()%(bucket->nkeys);
    int64 swapped_key = swap_bucket_key(bucket, key, swapkey_bucket_index);
    return(swapped_key);
}

bool xtndbln_hash_table_lookup(XtndblNHashTable *table, int64 key) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Checks for key inside hash table. 
     *
     * INPT: XtndblNHashTable *table
     *          Pointer to table structure in which to search
     *
     *       int64 key
     *          Key for  which to search
     *
     *       int time
     *          If = TIME, add time taken to lookup to tables cumulative lookup
     *          time. if != TIME dont.
     *
     * OTPT: bool
     *          True to indicate the key is present in the table, false
     *          otherwise.
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    /* Start timer */
    int start_time = clock();

    /* Find the address of the singular bucket which can contain this key. */
    int address = get_bit_trail(table->hash(key), table->depth);
    int ix=0;

    /* Get the bucket address which this key hashed to */
    Bucket *bucket = table->buckets[address];
    
    /* Search for the key inside the bucket */
    while (ix<bucket->nkeys) {

        /* Return true if key has been found */
        if (bucket->keys[ix++] == key) {
            return(true);
        }
    }

    /* Add lookup time */
    table->accum_lookup_time += (clock()-start_time);

    /* Key was not found in search through the bucket to which it hashed. */
	return false;
}

// print the contents of 'table' to stdout
void xtndbln_hash_table_print(XtndblNHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("  table:               buckets:\n");
	printf("  address | bucketid   bucketid [key]\n");
	
	// print table and buckets
	int i;
	for (i = 0; i < table->size; i++) {
		// table entry
		printf("%9d | %-9d ", i, table->buckets[i]->id);

		// if this is the first address at which a bucket occurs, print it now
		if (table->buckets[i]->id == i) {
			printf("%9d ", table->buckets[i]->id);

			// print the bucket's contents
			printf("[");
			for(int j = 0; j < table->bucketsize; j++) {
				if (j < table->buckets[i]->nkeys) {
					printf(" %llu", table->buckets[i]->keys[j]);
				} else {
					printf(" -");
				}
			}
			printf(" ]");
		}
		// end the line
		printf("\n");
	}

	printf("--- end table ---\n");

}

void xtndbln_hash_table_print_sparse(XtndblNHashTable *table) {
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * DESC: Prints and xtndbln table sparsely. That is, it prints no dash to
     *       indicate an empty bucket for consistency with the xuckoo print 
     *       table function which it replaces. For buckets that hold more than
     *       one key (ie xuckoon), a dash will be printed per empty slot for 
     *       consistency with part 2 print format and because logically there 
     *       needs to be something explicit to show how many empty slots a 
     *       bucket has for readability. The spec does not specify fine detail
     *       of the print output function for the bonus question.
     *
     * INPT: XtndblNHashTable *table
     *          XtndblNHashTable for which to print sparsely.
     *
     * OTPT: Prints to stdout the table given
     *
     * CRDT: Adapted from xuckoo print function written by Matthew Farrugia
     *
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    
    // print header
    printf("  table:               buckets:\n");
    printf("  address | bucketid   bucketid [key]\n");
    
    // print table and buckets
    int i;
    for (i = 0; i < table->size; i++) {
        // table entry
        printf("%9d | %-9d ", i, table->buckets[i]->id);

        // if this is the first address at which a bucket occurs, print it
        if (table->buckets[i]->id == i) {
            printf("%9d ", table->buckets[i]->id);

            // Print bucket contents
            printf("[");
            for(int j = 0; j < table->bucketsize; j++) {
                if (table->bucketsize>1) {
                    printf(" ");
                }
                if (j < table->buckets[i]->nkeys) {
                    printf("%llu", table->buckets[i]->keys[j]);

                /* Only print a dash for buckets with more than one thing in
                 * them to match the assignment print spec for xuckoo*/
                } else if (table->bucketsize>1) {
                    printf("-");
                } else {
                    printf(" ");
                }
            }
            if (table->bucketsize>1) {
                printf(" ");
            }
            printf("]");
        }

        // end the line
        printf("\n");
    }
}

// print some statistics about 'table' to stdout
void xtndbln_hash_table_stats(XtndblNHashTable *table) {
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
static double* get_stat_row(XtndblNHashTable *table) {
    int n_columns = NSTATS;
    double* statrow = malloc(sizeof(double) * n_columns);

    double insert_time = 1000*table->accum_insert_time/CLOCKS_PER_SEC;
    long double lookup_time = 1000*table->accum_lookup_time/CLOCKS_PER_SEC;


    statrow[0] = table->size;
    statrow[1] = table->bucketsize;
    statrow[2] = table->nitems;
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
void xtndbln_hash_table_csv_stats(XtndblNHashTable *table) {
    double* stats = get_stat_row(table);
    print_stat_row(stats);
    free(stats);
}

// Get stats header and print to stdout
void xtndbln_hash_table_csv_stats_header (XtndblNHashTable *table) {
    char** header =get_stat_header();
    print_stat_header(header);
    free(header);
}
