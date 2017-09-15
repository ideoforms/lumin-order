# lumin-order

Reorder a video file by frame luminosity.

Luminosity is processed frame by frame by taking the mean pixel
intensity from `[0..1]`. Luminosity is then rounded to `N` decimal places
to reduce fragmentation, so that in-order sequences from the original
material are represented intact. Alter the rounding factor with the
`-r` argument to achieve different levels of fragmentation.

Commissioned for the opening of [Black Tower Projects](http://www.blacktowerprojects.com/), September 2017.

Created by [Daniel Jones](http://www.erase.net/).

# Installation

To install and run the script:

```
pip install -r requirements.txt
python reorder.py
```

# History

This script replaces an earlier libcinder incarnation of LuminOrder.
