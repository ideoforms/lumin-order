mkdir -p icon.iconset
convert -size 128x128 gradient:white-black -rotate 90 gradient.png
mv gradient.png icon.iconset/icon_128x128.png
iconutil -c icns icon.iconset
