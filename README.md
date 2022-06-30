# movie_maker
C++ slideshow maker

### Features Planned:
Turn all images in a folder into a slideshow\
Turn all images in all subfolders into a slideshow\
Turn all images in a folder into a slideshow for each subfolder\
Crop all images in a folder\
Crop all images in all subfolders\
Resize all images in a folder to a desired size\
Resize all images in all subfolders to a desired size\
Remove all duplicate images in a folder\
Remove all duplicate images in all subfolders\
Decompile a slideshow into individual frames

### Flags:
**-m, --mode:** Mode (default: compile)\
**-i, --input:** Input folder (required)\
**-r, --recurse:** Apply recursively to subfolders (`UB` with creation of slideshows)\
**-o, --output:** Output folder (Recreates exactly what it would do but somewhere else to prevent overwriting) (required with decompilation of slideshows)\
**-R, --res:** Resolution from a defined list or as custom (default: 1920x1080)\
**-il, --image-length:** How many seconds each image should remain on screen (default: 3)\
**-vl, --video-length:** The minimum length of time a video should be on screen (default: 5)
