#include "treepoints.h"
#include <stdio.h>
#include <errno.h>


ssize_t
_readline (char *out, size_t *len, size_t maxlen, FILE *fp)
{
  char linebuf[maxlen];
  int i;
  char c = fgetc (fp);
  linebuf[0] = c;  /* If line is 1 character long at end of file */

  for (; (!feof (fp)) && (c != '\n'); c = fgetc (fp))
  {
    /*
     * We choose to fail if line exceeds a given length,
     * since we can reasonably guess line length in this case.
     */
    if (i >= maxlen)
      return -1;
    linebuf[i] = c;
    i++;
  }

  if ((out = malloc (sizeof(char)*(i+1))) == NULL)
    return -1;

  for (int j = 0; j < i; j++)
    out[j] = linebuf[j];

  return 0;
}


/*
 * tree_pointdata_init:
 * Initialize a new tree_pointdata_t element
 * based on a file path to read from.
 */

tree_pointdata_t *
tree_pointdata_init (const char *path)
{
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  tree_pointdata_t *data;
  int coordlen;
  int curr_coordlen = COORD_LIST_SIZE;
  int max_z, min_z;

  /* Allocate data memory. */

  _safe_malloc (data, sizeof(tree_pointdata_t))
  _safe_malloc (data->xs, sizeof(tree_pointdata_t)*curr_coordlen)
  _safe_malloc (data->ys, sizeof(tree_pointdata_t)*curr_coordlen)
  _safe_malloc (data->zs, sizeof(tree_pointdata_t)*curr_coordlen)

  FILE *inp_file = fopen (path, "r");
  if (inp_file == NULL)
    _EXIT_FAIL ("Error in tree_pointdata_init on reading file")

  /* Read file line-by-line for coordinates. */
  while ((read = _readline (&line, &len, 255, inp_file)) != -1)
  {
    int x, y, z;
    sscanf (line, "%d, %d, %d", &x, &y, &z);

    if (coordlen >= curr_coordlen)
    {
      /* Reallocate all coord lists to double length. */
      curr_coordlen << 1;

      _safe_realloc (data->xs, sizeof(tree_pointdata_t)*curr_coordlen)
      _safe_realloc (data->ys, sizeof(tree_pointdata_t)*curr_coordlen)
      _safe_realloc (data->zs, sizeof(tree_pointdata_t)*curr_coordlen)
    }

    data->xs[coordlen] = x;
    data->ys[coordlen] = y;
    data->zs[coordlen] = z;

    /* Compute max and min z values as we go. */
    if ((coordlen == 0) || (max_z < z))
      max_z = z;
    if ((coordlen == 0) || (min_z > z))
      min_z = z;

    coordlen++;
    free (line);
  }

  data->num_coords = coordlen;
  data->max_z = max_z;
  data->min_z = min_z;

  if (line)
    free (line);

  if (fclose (inp_file) == EOF)
    _EXIT_FAIL ("Error in tree_pointdata_init on closing file")

  /* Compute buckets for x and y. */

  size_t num_buckets = (size_t) ((max_z - min_z) / ZBUCKET_RANGE);

  _safe_malloc (data->x_z_bucket, sizeof(double *) * num_buckets)
  _safe_malloc (data->y_z_bucket, sizeof(double *) * num_buckets)
  _safe_malloc (data->z_bucket_lengths, sizeof(unsigned int) * num_buckets)

  unsigned int curr_bucket_lengths[num_buckets];

  for (int j = 0; j < (int) ((max_z - min_z) / ZBUCKET_RANGE); j++)
  {
    _safe_malloc (data->x_z_bucket[j], sizeof(double) * ZBUCKET_SIZE)
    _safe_malloc (data->y_z_bucket[j], sizeof(double) * ZBUCKET_SIZE)
    data->z_bucket_lengths[j] = 0;
  }

  for (int j = 0; j < coordlen; j++)
  {
    double x = data->xs[j];
    double y = data->ys[j];
    double z = data->zs[j];

    unsigned int bucket = (int) (z / ZBUCKET_RANGE);
    unsigned int in_bucket_pos = data->z_bucket_lengths[bucket];

    if (in_bucket_pos > curr_bucket_lengths[bucket])
    {
      curr_bucket_lengths[bucket] << 1;
      _safe_realloc (data->x_z_bucket[bucket],
          sizeof(double) * curr_bucket_lengths[bucket])
      _safe_realloc (data->y_z_bucket[bucket],
          sizeof(double) * curr_bucket_lengths[bucket])
    }

    data->x_z_bucket[bucket][in_bucket_pos] = x;
    data->y_z_bucket[bucket][in_bucket_pos] = y;

    data->z_bucket_lengths[bucket]++;
  }

  data->z_num_buckets = num_buckets;

  return data;
}

/*
 * process_tree_pointdata: Process the data to find
 * the trunk, max branch and vertical lengths of the tree.
 * This is all done in one function as they are all
 * intertwined operations.
 *
 * Our strategy to do this has several stages. When we observe
 * the size of each z-bucket, we expect to see, from high
 * to low:
 * - A continuous increase to a local maximum (the widest
 *   point on the tree),
 * - A continuous decrease after this towards the trunk,
 * - A series of somewhat consistent sizes at the trunk,
 * - A sudden increase at the ground.
 *
 * Our underlying assumption in the above is that,
 * for the point cloud, more points means a greater size,
 * as adjacent points are of consistent distance apart and
 * only cover an object's surface. Thus, we can avoid
 * finding the actual diameter of each z-bucket.
 *
 * Overall, we do as follows:
 * 1. Find the highest bucket that is part of the trunk.
 * 2. Identify the trunk diameter.
 * 3. Take all points above the trunk as the tree bush.
 * 4. Find the diameter for the bush as the maximum diameter.
 * 5. Identify the lowest bucket containing the trunk.
 * 6. Take the lowest trunk point and the treetop to give
 * the height.
 *
 * We first identify the first bucket that we can
 * reasonably call the 'trunk', and then identify its
 * diameter by drawing the smallest possible enclosing
 * circle around it. This strategy is somewhat inaccurate
 * if the trunk is not circular, but is fast.
 *
 * To find the first trunk bucket, we note the rate of
 * change of size between buckets proportional to the
 * current bucket size. If, starting from the top bucket
 * down, this change between the two most recently seen
 * buckets is small and the current bucket is far smaller
 * than the largest bucket seen so far, we have found
 * a trunk bucket. (These qualitative concepts are
 * quantified with reasonable guesses made beforehand.)
 *
 * After this, we take all buckets above this one as the
 * tree bush. We can then flatten the bush vertically and
 * only consider x-y coordinates. Next, we draw the convex
 * hull and find the largest line through the center of the
 * bush (ie. from the furthest point to the other side),
 * calling this the maximum diameter.
 *
 * Finally, we find the lowest bucket that has an upper-
 * bounded number of points relative to the highest trunk,
 * and start a further search here. We do a search down each
 * bucket, starting at a given point and searching for
 * the nearest point in the next bucket down. If the nearest
 * point is not closely below the current one, the trunk
 * has eneded.
 */
void
process_tree_pointdata (tree_pointdata_t *data)
{
  int curr_maxbucket = data->z_num_buckets - 1;
  int max_trunkbucket = -1;

#define TRUNK_BUCKET_DIFF_THRESH 0.2
#define TRUNK_BUCKET_MAXDIFF_THRESH 5

  int curr = data->z_num_buckets - 2;
  int prev = data->z_num_buckets - 1

  int bush_size;

  /* Find the highest trunk bucket. */
  for (; curr >= 0; curr--)
  {
    if (data->z_bucket_lengths[curr] >= data->z_bucket_lengths[curr_maxbucket])
      curr_maxbucket = curr;
    else
    {
      int diff = data->z_bucket_lengths[curr] - data->z_bucket_lengths[prev];
      double ratio_diff = ((double) diff) / ((double) data->z_bucket_lengths[curr]);
      if (-TRUNK_BUCKET_DIFF_THRESH <= ratio_diff
          && ratio_diff <= TRUNK_BUCKET_DIFF_THRESH)
      {
        int max_diff = data->z_bucket_lengths[curr_maxbucket]
                       - data->z_bucket_lengths[curr];
        double ratio_maxdiff = ((double) max_diff)
                               / ((double) data->z_bucket_lengths[curr]);
        if (-TRUNK_BUCKET_MAXDIFF_THRESH <= ratio_diff
            && ratio_diff <= TRUNK_BUCKET_MAXDIFF_THRESH)
        {
          /* We have reached the highest trunk bucket */
          max_trunkbucket = curr;
          break;
        }
      }
    }

    prev = curr;
    bush_size += data->z_bucket_lengths[curr];
  }

  if (max_trunkbucket == -1)
    _EXIT_FAIL ("Error in process_tree_pointdata from failing to find trunk top")

  /* TODO: find trunk diameter */

  /* TODO: find tree height */

  double *bush_xs, *bush_ys;

  _safe_malloc (bush_xs, sizeof (double) * bush_size)
  _safe_malloc (bush_ys, sizeof (double) * bush_size)

  int i;

  /* Collect bush x/y coords. */
  for (curr = data->z_num_buckets - 1; curr > max_trunkbucket; curr--)
  {
    for (int j = 0; j < data->z_bucket_lengths[curr]; j++)
    {
      bush_xs[i] = data->x_z_bucket[curr][j];
      bush_ys[i] = data->y_z_bucket[curr][j];
      i++;
    }
  }

  /* TODO: find bush diameter */

  data->processed = 1;
}


double
tree_pointdata_get_trunkdiam (tree_pointdata_t *data)
{
  if (!data->processed)
    process_tree_pointdata (data);

  return data->trunkdiam;
}

double
tree_pointdata_get_height (tree_pointdata_t *data)
{
  if (!data->processed)
    process_tree_pointdata (data);

  return data->treeheight;
}

double
tree_pointdata_get_maxbranchdiam (tree_pointdata_t *data)
{
  if (!data->processed)
    process_tree_pointdata (data);

  return data->maxbranchdiam;
}
