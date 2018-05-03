#ifndef TREEPOINT_DATA_H
#define TREEPOINT_DATA_H


/*
 * tree_pointdata_t: Container datatype for all
 * information on point cloud for a tree.
 */

typedef struct tree_pointdata {
  // Dynamic arrays of x-coordinates, y-coordinates and
  // z-coordinates respectively.
  double *xs;
  double *ys;
  double *zs;
  // Cheaper to have these from the start, as they are
  // used repeatedly.
  double max_z;
  double min_z;
  // Buckets for X-coordinates and Y-coordinates of all
  // points in certain ranges of Z-values, respectively.
  double *x_z_bucket;
  double *y_z_bucket;
  // Range of Z-values per bucket.
#define ZBUCKET_SIZE 5;
} tree_pointdata_t;


tree_pointdata_t *tree_pointdata_init (char *);

double tree_pointdata_get_trunkdiam (tree_pointdata_t *);
double tree_pointdata_get_height (tree_pointdata_t *);
double tree_pointdata_get_maxbranchdiam (tree_pointdata_t *);

#endif
