# Tree Point Cloud

This program finds the trunk diameter, tree height and max
branch diameter of a tree.

It is MinGW-64 compliant on Windows, rather than natively
compliant.

# Strategy

Our strategy will have several stages. First, we gather
points into buckets of Z-coordinate ranges, which we
will term z-buckets for now. When we observe the size
of each z-bucket, we expect to see, from high to low:
1. A continuous increase to a local maximum (the widest
point on the tree),
2. A continuous decrease after this towards the trunk,
3. A series of somewhat consistent sizes at the trunk,
4. A sudden increase at the ground.

Our underlying assumption in the above is that,
for the point cloud, more points means a greater size,
as adjacent points are of consistent distance apart and
only cover an object's surface. Thus, we can avoid
finding the actual diameter of each z-bucket.

Overall, we do as follows:
1. Find the highest bucket that is part of the trunk.
2. Identify the trunk diameter.
3. Take all points above the trunk as the tree bush.
4. Find the diameter for the bush as the maximum diameter.
5. Identify the lowest bucket containing the trunk.
6. Take the lowest trunk point and the treetop to give
the height.

We first identify the first bucket that we can
reasonably call the 'trunk', and then identify its
diameter by drawing the smallest possible enclosing
circle around it. This strategy is somewhat inaccurate
if the trunk is not circular, but is fast.

To find the first trunk bucket, we note the rate of
change of size between buckets proportional to the
current bucket size. If, starting from the top bucket
down, this change between the two most recently seen
buckets is small and the current bucket is far smaller
than the largest bucket seen so far, we have found
a trunk bucket. (These qualitative concepts are
quantified with reasonable guesses made beforehand.)

After this, we take all buckets above this one as the
tree bush. We can then flatten the bush vertically and
only consider x-y coordinates. Next, we draw the convex
hull and find the largest line between points on it,
calling this the maximum diameter. We don't use the
circle method like the trunk, as the bush could be less
circular overall.

Finally, we find the lowest bucket that has an upper-
bounded number of points relative to the highest trunk,
and start a further search here. We do a search down each
bucket, starting at a given point and searching for
the nearest point in the next bucket down. If the nearest
point is not closely below the current one, the trunk
has eneded.
