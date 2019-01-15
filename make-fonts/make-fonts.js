"use strict";

const fs = require('fs');
const path = require('path');
const makeOptions = require('optionator');
const child_process = require('child_process');
const util = require('util');

const execFile = util.promisify(child_process.execFile);

const optionSpec = {
  options: [
    { option: 'help', alias: 'h', type: 'Boolean', description: 'displays help' },
    { option: 'font', type: 'String', description: 'which font to generate' },
  ],
  prepend: 'make-fonts.js [options]',
  helpStyle: {
    typeSeparator: '=',
    descriptionSeparator: ' : ',
    initialIndent: 4,
  },
};

const optionator = makeOptions(optionSpec);

let args;
try {
  args = optionator.parse(process.argv);
  if (args._.length > 0) {
    throw new Error("unknown arguments");
  }
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

// rem node gen-font\gen-font.js --project-path=..\build-new\dr_ch1_beta_0.gmx --bmfont-config=bmfont-scripts\fnt-ja-comicsans.bmfc  --output-name=fnt_ja_comicsans
// rem node gen-font\gen-font.js --project-path=..\build-new\dr_ch1_beta_0.gmx --bmfont-config=bmfont-scripts\fnt-ja-dotumche.bmfc   --output-name=fnt_ja_dotumche
// rem node gen-font\gen-font.js --project-path=..\build-new\dr_ch1_beta_0.gmx --bmfont-config=bmfont-scripts\fnt-ja-mainbig.bmfc    --output-name=fnt_ja_mainbig
// rem node gen-font\gen-font.js --project-path=..\build-new\dr_ch1_beta_0.gmx --bmfont-config=bmfont-scripts\fnt-ja-main.bmfc       --output-name=fnt_ja_main
// rem node gen-font\gen-font.js --project-path=..\build-new\dr_ch1_beta_0.gmx --bmfont-config=bmfont-scripts\fnt-ja-small.bmfc      --output-name=fnt_ja_small
// rem node gen-font\gen-font.js --project-path=..\build-new\dr_ch1_beta_0.gmx --bmfont-config=bmfont-scripts\fnt-ja-tinynoelle.bmfc --output-name=fnt_ja_tinynoelle

function genFont(options) {
  const args = [
    path.join(__dirname, '..', 'gen-font', 'gen-font.js'),
    `--project-path=${path.join('..', 'build-new', 'DELTARUNE')}`,
  ];

  for (const [key, value] of Object.entries(options)) {
    args.push(`--${camelCaseToDash(key)}=${value}`)
  }

  return execFile("node.exe", args);
}

function camelCaseToDash(str) {
  return str.replace(/[A-Z]/g, r => `-${r.toLowerCase()}`);
}

async function main() {
  let fonts = [
    {
      fontName: "HappyRuikaKyohkan-07",
      font: "Fonts-for-Gregg/HappyRuikaKyohkan-07.ttf",
      debugColor: "0xFF0000",
      light: true,
      glyphHeight: 31,
      fontSize: 30,
      yOffset: 0,
      outputName: "fnt_ja_comicsans",
    },
    {
      fontName: "DotumChe",
      font: "Fonts-for-Gregg/TT_UDKakugoC80-M.ttf",
      debugColor: "0xFFFF00",
      glyphHeight: 19,
      fontIndex: 0,
      fontSize: 14,
      yOffset: 1,
      light: true,
      outputName: "fnt_ja_dotumche",
    },
    {
      fontName: "JF Dot Shinonome Gothic 14",
      font: "Fonts-for-Gregg/JF-Dot-Shinonome14.ttf",
      debugColor: "0x00FF00",
      glyphHeight: 16,
      fontSize: 14,
      yOffset: 1,
      outputName: "fnt_ja_main",
    },
    {
      fontName: "JF Dot Shinonome Gothic 14",
      font: "Fonts-for-Gregg/JF-Dot-Shinonome14.ttf",
      debugColor: "0x00FFFF",
      glyphHeight: 32,
      fontSize: 28,
      yOffset: 2,
      outputName: "fnt_ja_mainbig",
    },
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
    {
      fontName: "FTT-UDKakugoC80 M",
      font: "Fonts-for-Gregg/TT_UDKakugoC80-M.ttf",
      debugColor: "0xFF00FF",
      glyphHeight: 28,
      fontSize: 20,
      yOffset: 2,
      light: true,
      // ---
      //fontName: "FTT-UDKakugoC80 M",
      //font: "Fonts-for-Gregg/TT_UDKakugoC80-M.ttf",
      //debugColor: "0xFF00FF",
      //glyphHeight: 14,
      //fontSize: 10,
      //yOffset: 1,
      //light: true,
      //alphaMin: 10,
      //alphaMax: 144,
      // ---
      //fontName: "JF Dot Shinonome Gothic 14",
      //font: "Fonts-for-Gregg/JF-Dot-Shinonome14.ttf",
      //debugColor: "0x00FF00",
      //glyphHeight: 16,
      //fontSize: 14,
      //yOffset: 1,
      outputName: "fnt_ja_tinynoelle",
    },
  ];
  if (args.font) {
    const re = new RegExp(args.font);
    fonts = fonts.filter(f => re.test(f.outputName));
  }
  for(const font of fonts) {
    console.log('=======[', font.outputName, ']===================================');
    try {
      const result = await genFont(font);
      console.log(result.stdout);
      console.log(result.stderr);
    } catch (e) {
      console.log(e.stdout);
      console.error(e.stderr);
      throw new Error(`failed for font: ${font.outputName}`);
    }
  }
}

main()
.then(() => {
  console.log("----done----");
})
.catch((e) => {
  console.error("error: ", e);
});
