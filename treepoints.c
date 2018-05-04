#include "treepoints.h"
#include <stdio.h>
#include <errno.h>
#include <math.h>


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

int *
get_convhull_indices (double *xs, double *ys, int count)
{
  double avg_x, avg_y;
  double max_dist, farthest_x, farthest_y;

  for (int i = 0; i < count; i++)
  {
    /* Update average this way to avoid overflow, even if much slower. */
    double c = 1 / ((double) i + 1);
    double d = i / ((double) i + 1);
    avg_x = (avg_x * d) + (xs[i] * c);
    avg_y = (avg_y * d) + (ys[i] * c);
  }

  for (int i = 0; i < count; i++)
  {
    double sqdist = _square_dist (xs[i], avg_x, ys[i], avg_y);
    if (dist >= max_dist)
    {
      max_dist = dist;
      farthest_x = xs[i];
      farthest_y = ys[i];
    }
  }

  int *convhull_indices, convhull_size;
  _safe_malloc (convhull_indices, sizeof (int) * count)

  /* Iterate through each point on hull. */
  double curr_x = farthest_x;
  double curr_y = farthest_y;

  /* TODO */
}

/*
 * process_tree_pointdata:
 * Process the data to find the trunk, max branch
 * and vertical lengths of the tree. This is all
 * done in one function as they are all
 * intertwined operations.
 */
void
process_tree_pointdata (tree_pointdata_t *data)
{
  int curr_maxbucket = data->z_num_buckets - 1;
  int max_trunkbucket = -1;

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

  /* find trunk diameter */
  double trunk_avg_x, trunk_farthest_x;
  double trunk_avg_y, trunk_farthest_y;
  double trunk_max_dist;

  for (int i = 0; i < data->z_bucket_lengths[max_trunkbucket]; i++)
  {
    trunk_avg_x += data->x_z_bucket[max_trunkbucket][i];
    trunk_avg_y += data->y_z_bucket[max_trunkbucket][i];
  }

  for (int i = 0; i < data->z_bucket_lengths[max_trunkbucket]; i++)
  {
    double sqdist = _square_dist (
        data->x_z_bucket[max_trunkbucket][i],
        trunk_avg_x,
        data->y_z_bucket[max_trunkbucket][i],
        trunk_avg_y
        );
    if (dist >= trunk_max_dist)
    {
      trunk_max_dist = dist;
      trunk_farthest_x = data->x_z_bucket[max_trunkbucket][i];
      trunk_farthest_y = data->y_z_bucket[max_trunkbucket][i];
    }
  }

  /* Trunk diameter = diameter of smallest encompassing circle */
  data->trunkdiam = sqrt (2 * _square_dist (
      trunk_farthest_x, trunk_avg_x,
      trunk_farthest_y, trunk_avg_y
      ));

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

  int *convhull_indices = get_convhull_indices (bush_xs, bush_ys, i);

  /* TODO: get line through furthest and middle points,
   * see where it hits other side of hull*/

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
