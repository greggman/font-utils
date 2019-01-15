const fs = require('fs');
const path = require('path');
const util = require('util');
const child_process = require('child_process');
const makeOptions = require('optionator');
const crypto = require('crypto');

const execFile = util.promisify(child_process.execFile);

const optionSpec = {
  options: [
    { option: 'help', alias: 'h', type: 'Boolean', description: 'displays help' },
    { option: 'column', type: 'String', description: 'column to extract', default: 'I', },
    { option: 'project-path',   type: 'String',  required: true, description: 'path to project', },
    { option: 'output-name',    type: 'String',  required: true, description: 'output name (eg. "fnt_ja_main")', },
    { option: 'font', type: 'String', required: true, description: 'path to truetype font file to use ', },
    { option: 'font-name',  type: 'String',  required: true, description: 'name of font in ttf file', },
    { option: 'font-size',  type: 'Number',  required: true, description: 'font size', },
    { option: 'font-index',  type: 'Number',  description: 'font index in font file', default: '0'},
    { option: 'light',  type: 'Boolean',  description: 'use freetype2 light rendering algo', default: 'true' },
    { option: 'alpha-min',  type: 'Number',  description: 'min alpha', default: '0'},
    { option: 'alpha-max',  type: 'Number',  description: 'max alpha', default: '255'},
    { option: 'glyph-height',  type: 'Number',  required: true, description: 'glyph-height', },
    { option: 'y-offset',  type: 'Number', description: 'y-offset to add to baseline', default: '0'},
    { option: 'show-grid',  type: 'Boolean',  description: 'add grid for debugging', },
    { option: 'debug-color',  type: 'String',  description: 'color for grid for debugging'},
    { option: 'max-atlas-size',  type: 'Number',  description: 'max atlas size (default, 2048)', default: '2048'},
    { option: 'error-on-crop',  type: 'Boolean',  description: 'error if glyph cropped', default: 'true' },
  ],
  helpStyle: {
    typeSeparator: '=',
    descriptionSeparator: ' : ',
    initialIndent: 4,
  },
  prepend: 'gen-font.js options',
};

const optionator = makeOptions(optionSpec);

let args;
try {
  args = optionator.parse(process.argv);
} catch (e) {
  console.error(e);
  printHelp();
}

function printHelp() {
  console.log(optionator.generateHelp());
  process.exit(1);  // eslint-disable-line
}

if (args.help) {
  printHelp();
}

function makeFont(args) {
  // const fontGenPath = path.join(__dirname, '..', 'font-atlas-generator', 'Debug', 'font-atlas-generator.exe');
  const fontGenPath = path.join(__dirname, '..', 'font-atlas-generator-freetype2', 'Debug', 'font-atlas-generator.exe');
  const usedCharsFilename = path.join(args.projectPath, 'datafiles', 'lang', 'lang_ja.json');
  const outPath = path.join(args.projectPath, 'fonts', args.outputName);
  const fntPath = 'delme.json';
  const pngPath = 'delme.png';
  const fntPNGFilename = path.join(outPath, `${args.outputName}.png`);
  const fntYYFilename = path.join(outPath, `${args.outputName}.yy`);

  const fontGenArgs = [
  //  `--verbose=true`,
    `--error-on-crop=${args.errorOnCrop}`,
    `--outname=delme`,
    `--font=${args.font}`,
    `--font-index=${args.fontIndex}`,
    `--font-size=${args.fontSize}`,
    `--glyph-height=${args.glyphHeight}`,
    `--y-offset=${args.yOffset}`,
    `--light=${args.light}`,
    `--alpha-min=${args.alphaMin}`,
    `--alpha-max=${args.alphaMax}`,
    `--range=32-126`,
    `--range=12353-12435`, // 0x3041-0x3093
    `--range=12443-12444`, // 0x309B-0x309C
    `--range=12449-12534`, // 0x30A1-0x30F6
    `--range=12539-12540`, // 0x30FB-0x30FC
    '--range=65313-65338', // 0xFF21-0xFF3A A-Z fullwidth
    '--range=65345-65370', // 0xFF41-0xFF4A a-z fullwidth
    `--used-chars-file=${usedCharsFilename}`,
  ];

  if (args.showGrid) {
    fontGenArgs.push('--show-grid=true');
    if (args.debugColor) {
      fontGenArgs.push(`--debug-color=${args.debugColor}`)
    }
  }

  if (fs.existsSync(pngPath)) {
    fs.unlinkSync(pngPath);
  }

  console.log(fontGenPath, ...fontGenArgs);
  return execFile(fontGenPath, fontGenArgs)
  .then((output) => {
    console.log(output.stdout);
    console.log(output.stderr);
    const fntJSON = fs.readFileSync(fntPath, {encoding: 'utf8'});
    return JSON.parse(fntJSON);
  })
  .then((fnt) => {
    if (fnt.atlasWidth > args.maxAtlasSize || fnt.aliasHeight > args.maxAtlasSize) {
      throw new Error(`altas too large: max size: ${args.maxAtlasSize}, actual size: ${fnt.atlasWidth}x${fnt.atlasHeight}`);
    }
    const ranges = makeRanges(fnt.glyphs);
    const orig = readJSON(fntYYFilename);
    orig.size = args.fontSize;
    orig.kerningPairs = [];
    orig.fontName = args.fontName;
    orig.ranges = ranges.map((r) => { return { x: r[0], y: r[1], }; });

    const oldGlyphsByCodepoint = {};
    orig.glyphs.forEach((glyph) => {
      oldGlyphsByCodepoint[glyph.Key] = glyph;
    });

    orig.glyphs = fnt.glyphs.map((glyph) => {
      const oldGlyph = oldGlyphsByCodepoint[glyph.codePoint] || { Value: {}};
      return {
        Key: glyph.codePoint,
        Value: {
          id: oldGlyph.Value.id || uuidv4(),
          modelName: "GMGlyph",
          mvc: "1.0",
          character: glyph.codePoint,
          h: args.glyphHeight ? args.glyphHeight : glyph.tex.h,
          offset: glyph.xOff,
          shift: Math.round(glyph.xAdvance),
          w: glyph.tex.w,
          x: glyph.tex.x,
          y: glyph.tex.y,
        },
      };
    });

    console.log('write:', fntYYFilename);
    // replace is hack to make diffs less different
    fs.writeFileSync(fntYYFilename, JSON.stringify(orig, null, 4).replace(`"kerningPairs": [],`, `"kerningPairs": [\n        \n    ],`).replace(/\n/g, '\r\n'), {encoding: 'utf8'});
    if (fs.existsSync(fntPNGFilename)) {
      fs.unlinkSync(fntPNGFilename);
    }
    console.log('write:', fntPNGFilename);
    fs.copyFileSync(pngPath, fntPNGFilename);
  })
  .catch((error) => {
    console.error(error.stderr, error);
    process.exit(1);
  });
}

function uuidv4() {
  return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
    (c ^ crypto.randomFillSync(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
  )
}

function makeRanges(glyphs) {
  const used = [];
  glyphs.forEach((glyph) => {
    used[glyph.codePoint] = true;
  });
  const ranges = [];
  let start = undefined;
  // note: we go past the end of the array. JS is fine with that
  // and will return undefined which will add the last range.
  for (let i = 0; i <= used.length; ++i) {
    if (used[i] === true) {
      if (start === undefined) {
        start = i;
      }
    } else {
      if (start !== undefined) {
        ranges.push([start, i - 1]);
        start = undefined;
      }
    }
  }
  return ranges;
}

function addBOMToUTF8(srcFilename, dstFilename) {
  const usedChars = fs.readFileSync(srcFilename);
  const file = fs.openSync(dstFilename, 'w');
  const bom = new Uint8Array([0xEF, 0xBB, 0xBF]);
  fs.writeSync(file, bom);
  fs.writeSync(file, usedChars);
  fs.closeSync(file);
}

function readJSON(filename) {
  const s = fs.readFileSync(filename, {encoding: 'utf8'});
  return JSON.parse(s);
}

function xmlToJSON(xml) {
  return new Promise((resolve, reject) => {
    const parser = new xml2js.Parser({
      explicitArray: false,
      mergeAttrs: true,
    });
    parser.parseString(xml, (err, json) => {
      if (err) {
        return reject(err);
      }
      resolve(json);
    });
  });
}

function fixArray(obj, arrayKey, elemKey) {
  const maybeArray = obj[arrayKey][elemKey];
  obj[arrayKey] = Array.isArray(maybeArray)
    ? maybeArray
    : [maybeArray];
}

makeFont(args);

