#!/usr/bin/env python

#------------------------------------------------------------------------
# Reorder a video file by frame luminosity.
#
# Luminosity is processed frame by frame by taking the mean pixel
# intensity from [0..1]. Luminosity is then rounded to N decimal places
# to reduce fragmentation, so that in-order sequences from the original
# material are represented intact. Alter the rounding factor with the
# -r argument to achieve different levels of fragmentation.
#
# Commissioned for the opening of Black Tower Projects
# <http://www.blacktowerprojects.com/>
# September 2017
#
# by Daniel John Jones
# <http://www.erase.net/> 
#------------------------------------------------------------------------

from moviepy.editor import VideoFileClip
import numpy as np

import argparse
import math
import sys

#------------------------------------------------------------------------
# Parse command-line arguments.
#------------------------------------------------------------------------
parser = argparse.ArgumentParser(description='Reorder a video file by luminosity')
parser.add_argument('input', metavar='input_file', type=str,
    help='Input filename')
parser.add_argument('-o', dest='output', metavar='output_file',
    type=str, help='Output filename', default='out.mp4')
parser.add_argument('-R', dest='reverse', help='Reverse order',
    action='store_true', default=False)
parser.add_argument('-r', dest='round', metavar='decimal_places',
    type=int, help='Luminosity precision, in decimal places, or 0 to disable rounding', default=2)
parser.add_argument('-l', dest='length', metavar='duration_seconds',
    type=float, help='Duration to crop to')

args = parser.parse_args()
if args.round == 0:
    args.round = 12

clip = VideoFileClip(args.input)

if args.length is not None:
    clip = clip.set_duration(args.length)

print "Read clip, duration = %.1fs, FPS = %.1f" % (clip.duration, clip.fps)

#------------------------------------------------------------------------
# Non-integer frame rates (e.g. 29.97) cause problems when retrieving
# offsets. Round to an integer.
#------------------------------------------------------------------------
clip.fps = round(clip.fps)

#------------------------------------------------------------------------
# Transform brightnesses into a list of (offset_seconds, brightness) 
#------------------------------------------------------------------------
print "Analysing brightness ...."
brightnesses = [ (index / clip.fps, round(np.mean(frame) / 255.0, args.round)) for index, frame in enumerate(clip.iter_frames()) ]

#------------------------------------------------------------------------
# Sort ascending
#------------------------------------------------------------------------
brightnesses = sorted(brightnesses, key = lambda x: ((-1 if args.reverse else 1) * x[1], x[0]))

#------------------------------------------------------------------------
# Transform into pairs of (origin_offset, destination_offset)
#------------------------------------------------------------------------
brightnesses = dict([ ("%.2f" % (index / clip.fps), value[0]) for index, value in enumerate(brightnesses) ])
print "Found %f frames" % (clip.fps * clip.duration)

#------------------------------------------------------------------------
# Filter function.
# Need two cases, for video (scalar offsets) and audio (arrays).
#------------------------------------------------------------------------
def remap_time(t):
    try:
        t0 = t[0]
        t1 = brightnesses["%.2f" % t0]
        t += (t1 - t0)
    except:
        try:
            t0 = t
            t1 = brightnesses["%.2f" % t0]
            t += (t1 - t0)
        except Exception, e:
            print "Exception when reordering: %s" % e
            sys.exit(1)
    return t

#------------------------------------------------------------------------
# Process the clip.
#------------------------------------------------------------------------
clip_sorted = clip.fl_time(remap_time, apply_to=['mask', 'audio'], keep_duration=True)
clip_sorted.write_videofile(args.output, fps = clip.fps, write_logfile = True, audio_bufsize = int(44100.0 / clip.fps))
