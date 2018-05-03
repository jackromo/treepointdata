#include "treepoints.h"
#include <string.h>
#include <stdio.h>


int
main (void)
{
  char *files[4] = {
     "data/Tree1.txt",
     "data/tree2b.txt",
     "data/tree3.txt",
     "data/tree4.txt"
  };

  printf ("=== Tree Point Cloud Project ===\n");

  for (int i = 0; i < 4; i++)
  {
    printf ("\nData for file %s\n", files[i]);

    tree_pointdata_t *data = tree_pointdata_init (files[i]);
    printf ("Trunk diamater: %f\n",
        tree_pointdata_get_trunkdiam (data));
    printf ("Tree height: %f\n",
        tree_pointdata_get_height (data));
    printf ("Max branch diameter: %f\n",
        tree_pointdata_get_maxbranchdiam (data));
    printf ("===========================\n");
  }

  return 0;
}
