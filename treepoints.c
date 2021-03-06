#include "treepoints.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>


#define _MAX_LINELEN 1024

/*
 * _readline:
 * Read a single line of upper-bounded length.
 */
ssize_t
_readline (char **out, size_t *len, FILE *fp)
{
  char linebuf[_MAX_LINELEN];
  int i = 0;
  char c = fgetc (fp);
  linebuf[0] = c;  /* If line is 1 character long at end of file */

  if (feof (fp))
    return -1;

  for (; (!feof (fp)) && (c != '\n'); c = fgetc (fp))
  {
    /*
     * We choose to fail if line exceeds a given length,
     * since we can reasonably guess line length in this case.
     */
    if (i >= _MAX_LINELEN)
    {
      linebuf[_MAX_LINELEN - 1] = '\0';
      return -1;
    }
    linebuf[i] = c;
    i++;
  }
  linebuf[i] = '\0';

  _safe_malloc (*out, sizeof(char) * (i + 1));

  for (int j = 0; j <= i; j++)
    (*out)[j] = linebuf[j];

  *len = i + 1;

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
  char *line;
  size_t len = 0;
  tree_pointdata_t *data;
  int coordlen = 0;
  int curr_coordlen = COORD_LIST_SIZE;
  double max_z, min_z;

  /* Allocate data memory. */

  _safe_malloc (data, sizeof(tree_pointdata_t));
  _safe_malloc (data->xs, sizeof(tree_pointdata_t)*curr_coordlen);
  _safe_malloc (data->ys, sizeof(tree_pointdata_t)*curr_coordlen);
  _safe_malloc (data->zs, sizeof(tree_pointdata_t)*curr_coordlen);

  FILE *inp_file = fopen (path, "r");
  if (inp_file == NULL)
    _EXIT_FAIL ("Error in tree_pointdata_init on reading file")

  /* Read file line-by-line or coordinates. */
  while (_readline (&line, &len, inp_file) != -1)
  {
    double x, y, z;

    sscanf (line, "%lf, %lf, %lf", &x, &y, &z);

    if (coordlen >= curr_coordlen)
    {
      /* Reallocate all coord lists to double length. */
      curr_coordlen += COORD_LIST_SIZE;

      _safe_realloc (data->xs, sizeof (double) * curr_coordlen);
      _safe_realloc (data->ys, sizeof (double) * curr_coordlen);
      _safe_realloc (data->zs, sizeof (double) * curr_coordlen);
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

  if (fclose (inp_file) == EOF)
    _EXIT_FAIL ("Error in tree_pointdata_init on closing file")

  /* Compute buckets for x and y. */

  size_t num_buckets = (size_t) ((max_z - min_z) / ZBUCKET_RANGE);

  _safe_malloc (data->x_z_bucket, sizeof(double *) * num_buckets);
  _safe_malloc (data->y_z_bucket, sizeof(double *) * num_buckets);
  _safe_malloc (data->z_z_bucket, sizeof(double *) * num_buckets);
  _safe_malloc (data->z_bucket_lengths, sizeof(unsigned int) * num_buckets);

  unsigned int *curr_bucket_lengths;
  _safe_malloc (curr_bucket_lengths, sizeof(unsigned int) * num_buckets);
  for (int j = 0; j < num_buckets; j++)
    curr_bucket_lengths[j] = ZBUCKET_SIZE;

  for (int j = 0; j < (int) ((max_z - min_z) / ZBUCKET_RANGE); j++)
  {
    _safe_malloc (data->x_z_bucket[j], sizeof(double) * ZBUCKET_SIZE);
    _safe_malloc (data->y_z_bucket[j], sizeof(double) * ZBUCKET_SIZE);
    _safe_malloc (data->z_z_bucket[j], sizeof(double) * ZBUCKET_SIZE);
    data->z_bucket_lengths[j] = 0;
  }

  for (int j = 0; j < coordlen; j++)
  {
    double x = data->xs[j];
    double y = data->ys[j];
    double z = data->zs[j];

    unsigned int bucket = (unsigned int) ((z - min_z) / ZBUCKET_RANGE);
    /* Special case if z == max_z */
    if (bucket == num_buckets)
      bucket--;
    unsigned int in_bucket_pos = data->z_bucket_lengths[bucket];

    if (in_bucket_pos >= curr_bucket_lengths[bucket])
    {
      curr_bucket_lengths[bucket] += ZBUCKET_SIZE;
      _safe_realloc (data->x_z_bucket[bucket],
          sizeof(double) * curr_bucket_lengths[bucket]);
      _safe_realloc (data->y_z_bucket[bucket],
          sizeof(double) * curr_bucket_lengths[bucket]);
      _safe_realloc (data->z_z_bucket[bucket],
          sizeof(double) * curr_bucket_lengths[bucket]);
    }

    data->x_z_bucket[bucket][in_bucket_pos] = x;
    data->y_z_bucket[bucket][in_bucket_pos] = y;
    data->z_z_bucket[bucket][in_bucket_pos] = z;

    data->z_bucket_lengths[bucket]++;
  }

  data->z_num_buckets = num_buckets;
  data->processed = 0;

  return data;
}

/*
 * cos_from_xaxis:
 * Find the cosine between two vectors,
 * [0,0]->P and O->[1, 0].
 */
double
cos_from_xaxis (double px, double py)
{
  return px / sqrt ((px * px) + (py * py));
}

/*
 * cmp_cos_indices:
 * Compare two points, reformed as cmp_val_t
 * values, by the cosine of their angles
 * to the origin.
 */
int
cmp_cos_indices (const void *v1, const void *v2)
{
  cmp_val_t *ind1 = (cmp_val_t *) v1;
  cmp_val_t *ind2 = (cmp_val_t *) v2;

  double cos1 = cos_from_xaxis (ind1->x, ind1->y);
  double cos2 = cos_from_xaxis (ind2->x, ind2->y);

  if (cos1 < cos2)
    return -1;
  else if (cos1 == cos2)
    return 0;
  else
    return 1;
}

/*
 * get_convhull_indices:
 * Get all indices of points in the convex hull
 * of a set of points.
 */
int *
get_convhull_indices (double *xs, double *ys, int count)
{
  /*
   * Our algorithm is O(nlogn) complexity. We
   * sort the points by their angle to the lowest
   * point Y-axis-wise, then accumulate points into
   * the hull in this order, removing previous points
   * if they make a concave angle to the current one.
   */
  double lowest_x, lowest_y;
  double max_dist, farthest_x, farthest_y;

  for (int i = 0; i < count; i++)
  {
    if (ys[i] <= lowest_y)
    {
      lowest_x = xs[i];
      lowest_y = ys[i];
    }
  }

  /*
   * Quicksort xs and ys in-place by angle to
   * (lowest_x, lowest_y).
   * We need to produce comparable values rather
   * than two arrays, and need to zero-adjust them
   * so the lowest point is at the origin. (This will
   * have no impact on later computations, as we are
   * merely producing a list of indices.)
   */
  cmp_val_t *sort_vals;
  _safe_malloc (sort_vals, sizeof (cmp_val_t) * count);

  for (int i = 0; i < count; i++)
  {
    sort_vals[i].ind = i;
    sort_vals[i].x = xs[i] - lowest_x;
    sort_vals[i].y = ys[i] - lowest_y;
  }

  qsort ((void *) sort_vals, count, sizeof (cmp_val_t), cmp_cos_indices);

  bool *in_convhull;
  int convhull_sz = count;
  _safe_malloc (in_convhull, sizeof (bool) * count);

  /*
   * Now iterate through all points, checking if they
   * are in the hull or not.
   */
  in_convhull[0] = true;
  in_convhull[1] = true;

  for (int i = 2; i < count; i++)
  {
    in_convhull[i] = true;
    /*
     * At each new point, we expect the next angle to give a
     * 'left turn' of angle as we go. We can find if this is
     * true by, for current point p1, prev point p2 and earlier
     * point p3, taking the z-coordinate of the cross product
     * of p1->p3 and p1->p2. If negative, it is a right turn
     * and thus p2 is not in the convex hull. (This might take
     * drawing out to better visualize!)
     */
    int prev1 = i - 1;
    while (!in_convhull[prev1] && prev1 > 0)
      prev1--;
    int prev2 = prev1 - 1;
    while (!in_convhull[prev2] && prev2 > 0)
      prev2--;
    /* while cross prod < 0 */
    while (((sort_vals[prev2].x - sort_vals[i].x)
            * (sort_vals[prev1].y - sort_vals[i].y))
           < ((sort_vals[prev2].y - sort_vals[i].y)
              * (sort_vals[prev1].x - sort_vals[i].x)))
    {
      in_convhull[prev1] = false;
      convhull_sz--;
      while (!in_convhull[prev1] && prev1 > 0)
        prev1--;
      prev2 = (prev1 == 0) ? 0 : (prev1 - 1);
      while (!in_convhull[prev2] && prev2 > 0)
        prev2--;
      if (prev1 == 0)
        break;
    }
  }

  /*
   * Finally, accumulate points in convex hull in a
   * -1-terminated list of indices.
   */

  int *conv_indices;
  int convindices_top = 0;
  _safe_malloc (conv_indices, sizeof (int) * (convhull_sz + 1));

  for (int i = 0; i < convhull_sz; i++)
  {
    if (in_convhull[i])
    {
      conv_indices[convindices_top] = sort_vals[i].ind;
      convindices_top++;
    }
  }
  conv_indices[convindices_top] = -1;

  return conv_indices;
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
  int prev = data->z_num_buckets - 1;

  int bush_size = data->z_num_buckets - 1;

  /* Find the highest trunk bucket. */
  for (; curr >= 0; curr--)
  {
    if (data->z_bucket_lengths[curr] >= data->z_bucket_lengths[curr_maxbucket])
      curr_maxbucket = curr;
    else
    {
      int diff = data->z_bucket_lengths[curr] - data->z_bucket_lengths[prev];
      double ratio_diff = fabs(((double) diff) / ((double) data->z_bucket_lengths[curr]));
      if (ratio_diff <= TRUNK_BUCKET_DIFF_THRESH)
      {
        int max_diff = data->z_bucket_lengths[curr_maxbucket]
                       - data->z_bucket_lengths[curr];
        double ratio_maxdiff = fabs(((double) max_diff)
                                    / ((double) data->z_bucket_lengths[curr]));
        if (ratio_maxdiff >= TRUNK_BUCKET_MAXDIFF_THRESH)
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

  /* Find trunk diameter */

  double trunk_avg_x, trunk_avg_y;
  double trunk_farthest_x, trunk_farthest_y;
  double trunk_max_dist;

  for (int i = 0; i < data->z_bucket_lengths[max_trunkbucket]; i++)
  {
    trunk_avg_x += data->x_z_bucket[max_trunkbucket][i];
    trunk_avg_y += data->y_z_bucket[max_trunkbucket][i];
  }

  trunk_avg_x /= data->z_bucket_lengths[max_trunkbucket];
  trunk_avg_y /= data->z_bucket_lengths[max_trunkbucket];

  for (int i = 0; i < data->z_bucket_lengths[max_trunkbucket]; i++)
  {
    double sqdist = _square_dist (
        data->x_z_bucket[max_trunkbucket][i],
        trunk_avg_x,
        data->y_z_bucket[max_trunkbucket][i],
        trunk_avg_y
        );
    if (sqdist >= trunk_max_dist)
    {
      trunk_max_dist = sqdist;
      trunk_farthest_x = data->x_z_bucket[max_trunkbucket][i];
      trunk_farthest_y = data->y_z_bucket[max_trunkbucket][i];
    }
  }

  /* Trunk diameter = diameter of smallest encompassing circle */
  data->trunkdiam = 2 * sqrt (_square_dist (
      trunk_farthest_x, trunk_avg_x,
      trunk_farthest_y, trunk_avg_y
      ));

  /*
   * Find tree height
   * Do this in several stages:
   * 1. Generate a circle around trunk based on highest trunk
   * bucket, with a margin of error. We will expect a similar
   * number of points within this circle in lower buckets
   * if they contain the trunk. (This assumes the trunk is
   * straight.)
   * 2. Skip buckets below until reaching one of significant
   * difference in size to the highest trunk bucket.
   * 3. Keep going, checking points within the circle rather
   * than just all points. Find the last one with a similar
   * count in this region, bounded only below now.
   * 4. Within this bucket, find the nearest point outside.
   * This will be the considered height for the ground.
   * 5. Take difference between max_z and this value.
   *
   * If the trunk isn't of consistent width, this method
   * will underestimate the height. If not straight, it
   * will state the height is the first major point
   * where the ground starts appearing, even if far away.
   */

  circ_t trunk_err_circ;

  trunk_err_circ.x = trunk_avg_x;
  trunk_err_circ.y = trunk_avg_y;
  trunk_err_circ.rad = (data->trunkdiam / 2) * (1 + TRUNK_BUCKET_DIFF_THRESH);

  int curr_bucket = 0;

  for (curr_bucket = max_trunkbucket;
       abs (data->z_bucket_lengths[curr_bucket] - data->z_bucket_lengths[max_trunkbucket])
       < (TRUNK_BUCKET_DIFF_THRESH * data->z_bucket_lengths[max_trunkbucket]);
       curr_bucket--)
    ;

  /* Search for bucket below start of ground where trunk ends  */
  int in_trunkerr_count = data->z_bucket_lengths[max_trunkbucket];
  for (/**/
      ; abs (in_trunkerr_count - data->z_bucket_lengths[max_trunkbucket])
        < (TRUNK_BUCKET_DIFF_THRESH * data->z_bucket_lengths[max_trunkbucket])
      ; curr_bucket--)
  {
    in_trunkerr_count = 0;
    for (int i = 0; i < data->z_bucket_lengths[curr_bucket]; i++)
    {
      double sqdist = _square_dist (
          data->x_z_bucket[curr_bucket][i], trunk_err_circ.x,
          data->y_z_bucket[curr_bucket][i], trunk_err_circ.y
          );
      if (sqdist < (trunk_err_circ.rad * trunk_err_circ.rad))
        in_trunkerr_count++;
    }
  }
  /* Go back to bucket that failed (decremented after failing iteration) */
  curr_bucket += 2;

  /* Find closest point outside circle in bucket. Get its z-coordinate */
  double closest_outside_z = data->z_z_bucket[curr_bucket][0];
  double closest_outside_dist = trunk_err_circ.rad * trunk_err_circ.rad * 16;
  for (int i = 0; i < data->z_bucket_lengths[curr_bucket]; i++)
  {
    double sqdist = _square_dist (
        data->x_z_bucket[curr_bucket][i], trunk_err_circ.x,
        data->y_z_bucket[curr_bucket][i], trunk_err_circ.y
        );
    if (sqdist > (trunk_err_circ.rad * trunk_err_circ.rad)
        && sqdist < closest_outside_dist)
    {
      closest_outside_dist = sqdist;
      closest_outside_z = data->z_z_bucket[curr_bucket][i];
    }
  }

  data->treeheight = data->max_z - closest_outside_z;

  /* Find max branch diameter */

  double *bush_xs, *bush_ys;

  _safe_malloc (bush_xs, sizeof (double) * bush_size);
  _safe_malloc (bush_ys, sizeof (double) * bush_size);

  int i = 0;

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

  int *conv_indices = get_convhull_indices (bush_xs, bush_ys, i);
  int convind_sz;
  for (convind_sz = 0; conv_indices[convind_sz] != -1; convind_sz++)
    ;

  /*
   * Converge on furthest points away from one
   * another in hull. This is doable by the nature
   * of a convex hull in linear time.
   */
  int pt1 = conv_indices[0];
  int pt2 = conv_indices[1];

  while (1)
  {
    /* Test adjacent points. */
    int adjpts1[3] = {
      (pt1 == 0 ? convind_sz-1 : pt1-1),
      pt1,
      (pt1 == convind_sz-1 ? 0 : pt1+1)};
    int adjpts2[3] = {
      (pt2 == 0 ? convind_sz-1 : pt2-1),
      pt2,
      (pt2 == convind_sz-1 ? 0 : pt2+1)};

    int pt1_max = pt1;
    int pt2_max = pt2;
    double sqdist_max = 0;

    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
      {
        int p1 = adjpts1[i];
        int p2 = adjpts2[j];
        double sqdist = _square_dist (
            bush_xs[p1], bush_xs[p2], bush_ys[p1], bush_ys[p2]
            );
        if (sqdist > sqdist_max)
        {
          sqdist_max = sqdist;
          pt1_max = p1;
          pt2_max = p2;
        }
      }
    if (pt1_max == pt1 && pt2_max == pt2)
      break;
    else
    {
      pt1 = pt1_max;
      pt2 = pt2_max;
    }
  }

  data->maxbranchdiam = sqrt (_square_dist (
      bush_xs[pt1], bush_xs[pt2], bush_ys[pt1], bush_ys[pt2]
      ));

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

void
tree_pointdata_free (tree_pointdata_t *data)
{
  free (data->xs);
  free (data->ys);
  free (data->zs);
  for (int i = 0; i < data->z_num_buckets; i++)
  {
    free (data->x_z_bucket[i]);
    free (data->y_z_bucket[i]);
    free (data->z_z_bucket[i]);
  }
  free (data->x_z_bucket);
  free (data->y_z_bucket);
  free (data->z_z_bucket);
  free (data->z_bucket_lengths);
  free (data);
}
