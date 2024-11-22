#!/usr/bin/env python3

from PIL import Image
import sys

def rgb_to_rgb555(rgb):
	value = rgb[0] >> 3
	value |= (rgb[1] >> 3) << 5
	value |= (rgb[2] >> 3) << 10
	return value

def image_to_c(img, varname):
	print('const unsigned short tex_' + varname + '[] = {');
	for color in list(img.getdata()):
		print('0x%X,' % rgb_to_rgb555(color))
	print('};')

img = Image.open(sys.argv[1]).transpose(Image.FLIP_TOP_BOTTOM).convert('RGB').resize((128,128), Image.NEAREST)
image_to_c(img, sys.argv[2])
