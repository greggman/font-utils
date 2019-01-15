# Some font utils used in a gamemaker project

Note I'm dumping these here as a place to back them up.
If you find them useful good for you but don't expect
any support.

They will not work out of the box. In particular the fonts
used are not included.

# Gamemaker Issues

The problems with gamemaker and fonts

1. gamemaker requires all glyphs to be the same height.

   So a period (.) takes the same vertical space as the letter B

2. gamemaker doesn't understand that glyphs can be larger than their font size

   In other words you set a font size for 20 pixels but some glyphs might actually
   extend above the top of a 20 pixel tall area as well as below

The tools below, you give them a font-size (the size the render the glyphs),
a glyph-height (the size to make the box the glyph will be put inside),
and a y-offset (how many pixels down from the top of the box to start rendering)

There's an option passed in, error-on-crop, that basically makes the generator
fail if any glyph extends out of the box. Looking at the error messages you can
tell if you need more space above (in which case make y-offset bigger) or more
space below (in which case make glyph-height bigger). Of course whenever you
make y-offset bigger you move all the glyphs down in their box so you probably
have to make glyph-height bigger on those cases.

The only other thing I think is not clear maybe is font-index. Some .ttf files have
multiple fonts inside. Here they are references by index. If you double click a .ttf
file a viewer will come up and you can prev/next to see the fonts and figure out
the index.


# prerequisites

* Windows 10 (otherwise you need to build font-altas-generator-freetype2)
* Install node.js from https://nodejs.org
* setup your folders like this

        +-somefolder
          +-fonts (contents of fonts.zip)
          +-build-new/DELTARUNE (gamemaker project)

* open a node command prompt (or some command prompt with node.js in your path)

    cd fonts/gen-font
    npm install
    cd ../fonts/make-fonts
    npm install

# To generate the fonts

        cd fonts
        node make-fonts/make-fonts.js

  If you only want to generate some fonts you can add --font=substr as in

        node make-fonts/make-fonts.js --font=tiny

  which will only generate fonts with the word "tiny" in them

# make-fonts/make-fonts.js

  This is effectively just a batch file but using JavaScript so I can filter etc.
  It calls `node ../gen-font/gen-font.js

  If you look inside you'll see settings for each font. Example


        {
          fontName: "FTT-UDKakugoC80 M",
          font: "Fonts-for-Gregg/TT_UDKakugoC80-M.ttf",
          glyphHeight: 14,
          fontSize: 10,
          yOffset: 1,
          light: true,
          alphaMin: 10,
          alphaMax: 144,
          debugColor: "0x0000FF",
          outputName: "fnt_ja_small",
        },

  Most of the options are passed to gen-font.js an so are more documented there

# gen-font/gen-font.js

  This calls the font generator, read the output and copies the results into
  the gamemaker folders.

# font-atlas-generator-freetype2

  This is a C/C++ app that uses FreeType2 (a font library) to try to generate a font.
  It saves out a .png file and a .json file with data about the glphys it wrote.


