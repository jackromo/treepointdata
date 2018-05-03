#ifndef TREEPOINT_DATA_H
#define TREEPOINT_DATA_H

#include <stdio.h>
#include <stdlib.h>

/* Define this as a standard way to fail. */
#define _EXIT_FAIL(msg_prefix) { \
    perror (msg_prefix); \
    exit(EXIT_FAILURE); \
  }

/* Helper macros to safely allocate/reallocate memory. */
#define _safe_malloc(ptr, sz, errmsg_prefix) { \
  if ((ptr = malloc (sz)) == NULL) \
    _EXIT_FAIL ("Error in _safe_malloc on allocating memory") \
  }

#define _safe_realloc(ptr, sz, errmsg_prefix) { \
  if ((ptr = realloc (sz)) == NULL) \
    _EXIT_FAIL ("Error in _safe_realloc on reallocating memory") \
  }

/*
 * tree_pointdata_t: Container datatype for all
 * information on point cloud for a tree.
 */

typedef struct tree_pointdata {
  unsigned int num_coords;
  /*
   * Dynamic arrays of x-coordinates, y-coordinates and
   * z-coordinates respectively.
   */
  double *xs;
  double *ys;
  double *zs;
  /* Initial size for coordinate list. */
#define COORD_LIST_SIZE 1000
  /*
   * Cheaper to have these from the start, as they are
   * used repeatedly.
   */
  double max_z;
  double min_z;
  /*
   * Buckets for X-coordinates and Y-coordinates of all
   * points in certain ranges of Z-values, respectively.
   */
  double **x_z_bucket;
  double **y_z_bucket;
  unsigned int *z_bucket_lengths;
  /* Range of Z-values per bucket. */
#define ZBUCKET_RANGE 0.1;
#define ZBUCKET_SIZE 200;
} tree_pointdata_t;

tree_pointdata_t *tree_pointdata_init (char *);


double tree_pointdata_get_trunkdiam (tree_pointdata_t *);
double tree_pointdata_get_height (tree_pointdata_t *);
double tree_pointdata_get_maxbranchdiam (tree_pointdata_t *);

#endif
