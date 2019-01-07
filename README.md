# SDF-Generator
Generates SDF textures and json metadata files for webgl applications.

Includes UTF-8 support.


```
Usage:
sdfgen [GLOBAL OPTION]... [-f FONT [OPTION]...]... [outfile]

EXAMPLE:
  sdfgen -b -m 8 -o 4 -t 256 -f "Courier" 8,12,16 -i -f "Arial" 32 myfonts_sdf
     -b - generate bold for all fonts
     -m 8 - SDF magnitude of 8 pixels
     -o 4 - oversample by 4x
     -t 256 - output textures have 256px dimensions
     -f "Courier" - first font name 
        8,12,16 - generate outputs of size 8, 12, and 16 px for this font 
        -i - also generate italic for this font
     -f "Arial" - second font name 
        32 - generate outputs of size 32px for this font 
     myfonts_sdf - output will be myfonts_sdf_[n].png and myfonts_sdf.json

OPTIONS:
     All Font Options can be included as global options for all fonts.
  -f ...     Add a font to render
  -J         Specify the output JSON file name
  -l N       Limit computation to N threads
  -m         SDF magnitude in pixels. Default 8.
  -o         Amount of pixels to oversample by. Default 16.
  -P FORMAT  Specify the png file name; %d will be expanded to the index.
  -q         Quiet. Suppress all output, even warnings and errors.
  -t [N|fit] Size of output texture in pixels
               'fit' generates a single texture large enough to hold all glyphs
  -v         Verbose.

FONT OPTIONS:
  --          Stop parsing a font and return to global options. Not required.
  -b         Generate Bold
  -bi        Generate Bold Italic
  -c CHARSET Specify the charset, in UTF-8. Default is every character on a US keyboard.
  -i         Generate Italic
  --omit-regular  Do not generate the regular font face
  -s N[,...] Generate these output font sizes.

MISC OPTIONS:
  --help      Print this message and exit
  --pretend   Verify parameters but do not actually generate output.
```


# Installation

SDF-Generator has the following build dependencies. Most systems should have them already.
* libPNG
* FreeType
* FontConfig

## Ubuntu

```
sudo apt-get install build-essential git libpng-dev libfreetype6-dev libfontconfig1-dev
git clone https://github.com/yzziizzy/SDF-Generator.git
cd SDF-Generator
make
./sdfgen --help
```

## License

Unless otherwise noted, this project is released under the GNU Affero General Public License version 3.0.

unifont-11.0.03.ttf is released under GPL2+ by the GNU Project. http://unifoundry.com/unifont/index.html

MurmurHash is in the public domain.

