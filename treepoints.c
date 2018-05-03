#include "treepoints.h"
#include <stdio.h>
#include <errno.h>


ssize_t _readline (char *out, size_t *len, size_t maxlen, FILE *fp)
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

  return data;
}

double
tree_pointdata_get_trunkdiam (tree_pointdata_t *)
{

}

double
tree_pointdata_get_height (tree_pointdata_t *)
{

}

double
tree_pointdata_get_maxbranchdiam (tree_pointdata_t *)
{

}
