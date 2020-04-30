Description
===========

ASharp is an adaptive sharpening filter.

Only the luma plane will be processed.

This is a port of the Avisynth filter ASharp, version 0.95, written by Marc Fauconneau.


Usage
=====
::

    asharp.ASharp(clip clip[, float t=2.0, float d=4.0, float b=-1.0, bint hqbf=False])


Parameters:
    *clip*
        A clip to process. It must have constant format
        and it must be 8..16 bit with integer samples.

    *t*
        Unsharp masking threshold.

        0 will do nothing, 1 will enhance contrast 1x.
        
        Must be between 0 and 32.

        Default: 2.0.

    *d*
        Adaptive sharpening strength to avoid sharpening noise.

        If greater than 0, the threshold is adapted for each pixel (bigger for edges) and *t* acts like a maximum.
        
        Set to 0 to disable it. 

        Must be between 0 and 16.

        Default: 4.0.

    *b*
        Block adaptive sharpening.
       
        It avoids sharpening the edges of DCT blocks by lowering the threshold around them.

        Use with blocky videos. If cropping the video before ASharp the top and left edges must be cropped by multiples of 8.

        Set to a negative value to disable it.

        Must be between -∞ and 4.

        Default: -1.0.

    *hqbf*
        High quality block filtering.

        Default: False.


Compilation
===========

::

    meson build
    ninja -C build


License
=======

GPLv2.
