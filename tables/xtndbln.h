/* * * * * * * * *
 * Dynamic hash table using extendible hashing with multiple keys per bucket,
 * resolving collisions by incrementally growing the hash table
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by ...
 * 14/05/17
 * Ben Tomlin
 * SN: 834198
 * btomlin@student.unimelb.edu.au
 */

#ifndef XTNDBLN_H
#define XTNDBLN_H

#include <stdbool.h>
#include "../inthash.h"

typedef struct xtndbln_table XtndblNHashTable;

// initialise an extendible hash table with 'bucketsize' keys per bucket
XtndblNHashTable *new_xtndbln_hash_table(int bucketsize);

// Init a new hash table that uses a specified hash function 
XtndblNHashTable*
new_xtndbln_specified_hash_table(int bucketsize, int (*hash)(int64 key));

// Get size of XtndblNHashTable
int get_xtndbln_table_size(XtndblNHashTable *table); 

// free all memory associated with 'table'
void free_xtndbln_hash_table(XtndblNHashTable *table);

// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool xtndbln_hash_table_insert(XtndblNHashTable *table, int64 key);

// Swap key with a random one from the bucket to which it hashes.*/ 
int64 xtndbln_hash_table_rand_swap(XtndblNHashTable *table, int64 key);

// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool xtndbln_hash_table_lookup(XtndblNHashTable *table, int64 key);

/* Checks whether table has space for a key */
bool xtndbln_hash_table_has_space(XtndblNHashTable *table, int64 key);

// print the contents of 'table' to stdout
void xtndbln_hash_table_print(XtndblNHashTable *table);

// print the contents of 'table' to stdout without header or footer
void xtndbln_hash_table_print_sparse(XtndblNHashTable *table);

// print some statistics about 'table' to stdout
void xtndbln_hash_table_stats(XtndblNHashTable *table);

// print a csv row of stats
void xtndbln_hash_table_csv_stats(XtndblNHashTable *table);

// print a csv header for stats
void xtndbln_hash_table_csv_stats_header (XtndblNHashTable *table);

#endif
