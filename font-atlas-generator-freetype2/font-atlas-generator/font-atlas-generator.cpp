#pragma warning(disable : 4996)

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <set>
#include <filesystem>

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_rect_pack.h"
#include "stb_image_write.h"
#include "stb_truetype.h"

#include <ft2build.h>
#include FT_FREETYPE_H

struct Range {
  Range(int s, int e) : start(s), end(e) { };
  bool inRange(int v) {
    return v >= start && v <= end;
  }
  int start = 0;
  int end = 0;
};

struct Options {
  std::string font_filename;
  bool verbose = false;
  bool light = false;
  float font_size = 0.0f;
  int font_index = 0;
  int oversample = 1;
  int alpha_min = 0;
  int alpha_max = 255;
  int padding = 1;
  int atlas_width = 0;
  int atlas_height = 0;
  int glyph_height = 0;   // if > 0 then all glyphs will be the same height in the atlas
  int y_offset = 0;
  float shift_x = 0;
  float shift_y = 0;
  int debug_color[3] = { 0xFF, 0xFF, 0xFF };
  bool show_grid = false;
  bool ignore_errors = false;  // used to generate output during debugging
  bool error_on_crop = false;
  std::string out_name;
  std::vector<Range> ranges;
};

bool readFile(const char* filename, std::vector<unsigned char>* data) {
  std::experimental::filesystem::path path(filename);
  if (!std::experimental::filesystem::exists(path)) {
    fprintf(stderr, "%s does not exist\n", filename);
    return false;
  }
  unsigned int file_size = (unsigned int)std::experimental::filesystem::file_size(path);
  data->resize(file_size);

  printf("read: %s\n", filename);
  FILE* fp = fopen(filename, "rb");
  if (!fp) {
    return false;
  }
  fread(data->data(), 1, file_size, fp);
  fclose(fp);

  return true;
}

bool addUsedCodepointsFromUTF8File(const char* filename, std::set<int>* used) {
  std::vector<unsigned char> data;
  if (!readFile(filename, &data)) {
    return false;
  }

  size_t start = 0;

  // check for BOM
  if (data.size() >= 3 &&
      data[0] == 0xEF &&
      data[1] == 0xBB &&
      data[2] == 0xBF) {
    start = 3;
  }

  for (size_t i = start; i < data.size(); ++i) {
    const unsigned char c = data[i];
    if (c < 32) {
      continue;
    } else if (c < 128) {
      used->insert(c);
    } else {
      size_t remain = data.size() - i;
      size_t need = 0;
      int v = 0;
      const unsigned char masks[] = { 0xF8, 0xF0, 0xE0, };
      const unsigned char match[] = { 0xF0, 0xE0, 0xC0, };
      const size_t needs[] =        {    4,    3,    2, };

      bool found = false;
      for (size_t m = 0; m < std::size(masks); ++m) {
        unsigned char mask = masks[m];
        if ((c & mask) == match[m]) {
          found = true;
          need = needs[m];
          v = c & ~mask;
          break;
        }
      }

      if (!found) {
        fprintf(stderr, "error: bad char %02x at offset %d in %s\n", c, i, filename);
        return false;
      }

      if (remain < need) {
        fprintf(stderr, "error: bad end, needed %d chars but only %d left in %s\n", need, remain, filename);
        return false;
      }

      for (size_t j = 1; j < need; ++j) {
        const unsigned char c = data[i + j];
        if ((c & 0xC0) != 0x80) {
          fprintf(stderr, "error: bad char %02x at offset %d in %s\n", c, i + j, filename);
          return false;
        }
        v = (v << 6) | (c & 0x3F);
      }

      used->insert(v);

      i += need - 1;
    }
  }
  return true;
}

void generateRangesFromUsed(const std::set<int>& used, std::vector<Range>* ranges) {
  if (used.empty()) {
    return;
  }
  int min = *used.begin();
  int max = min;
  //2,3,4,11,12,13
  int c;
  for (const int& cc : used) {
    c = cc;
    if (c == max || c == max + 1) {
      max = std::max(c, max);
    } else {
      ranges->push_back(Range(min, max));
      min = c;
      max = c;
    }
  }
  ranges->push_back(Range(min, c));
}

bool parse_bool(const char* arg, bool* dst) {
  if (!strcmp(arg, "true")) {
    *dst = true;
    return true;
  }
  if (!strcmp(arg, "false")) {
    *dst = false;
    return true;
  }
  return false;
}

std::string underscore_to_dash(std::string s) {
  std::string t(s);
  std::replace(t.begin(), t.end(), '_', '-');
  return t;
}

#define ARG_PARSE_BOOL(field)  (!option.compare(underscore_to_dash(std::string("--" #field)))) \
  { \
    if (!parse_bool(value, &opt->field)) { \
       fprintf(stderr, "bad value for --%s, must be true or false, was %s\n", underscore_to_dash(#field).c_str(), value); \
       return 0; \
    } \
  }

int parse_command_line(int argc, const char* argv[], Options* opt) {
  std::set<int> used;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg[0] == '-' ) {
      const char* equal = strchr(arg, '=');
      if (!equal && i == argc - 1) {
        fprintf(stderr, "error: missing last argument\n");
        return 0;
      }
      const std::string option(arg, equal ? equal - arg : strlen(arg));
      const char *value = equal ? equal + 1 : argv[++i];
      if ARG_PARSE_BOOL(verbose)
      else if ARG_PARSE_BOOL(show_grid)
      else if ARG_PARSE_BOOL(light)
      else if ARG_PARSE_BOOL(ignore_errors)
      else if ARG_PARSE_BOOL(error_on_crop)
      else if (!option.compare("--font")) {
        opt->font_filename = value;
      } else if (!option.compare("--font-size")) {
        opt->font_size = (float)atof(value);
      } else if (!option.compare("--font-index")) {
        opt->font_index = atoi(value);
      } else if (!option.compare("--padding")) {
        opt->padding = atoi(value);
      } else if (!option.compare("--y-offset")) {
        opt->y_offset = atoi(value);
      } else if (!option.compare("--atlas-width")) {
        opt->atlas_width = atoi(value);
      } else if (!option.compare("--atlas-height")) {
        opt->atlas_height = atoi(value);
      } else if (!option.compare("--outname")) {
        opt->out_name = value;
      } else if (!option.compare("--glyph-height")) {
        opt->glyph_height = atoi(value);
      } else if (!option.compare("--outname")) {
        opt->out_name = value;
      } else if (!option.compare("--oversample")) {
        opt->oversample = atoi(value);
        if (opt->oversample < 1 || opt->alpha_min > 32) {
          fprintf(stderr, "oversample out of range, 1 to 32, was %d\n", opt->oversample);
          return 0;
        }
      } else if (!option.compare("--shift-x")) {
        opt->shift_x = (float)atof(value);
      } else if (!option.compare("--shift-y")) {
        opt->shift_y = (float)atof(value);
      } else if (!option.compare("--alpha-min")) {
        opt->alpha_min = atoi(value);
        if (opt->alpha_min < 0 || opt->alpha_min > 255) {
          fprintf(stderr, "alpha-min out of range, 0 to 255, was %d\n", opt->alpha_min);
          return 0;
        }
      } else if (!option.compare("--alpha-max")) {
        opt->alpha_max = atoi(value);
        if (opt->alpha_max < 0 || opt->alpha_max > 255) {
          fprintf(stderr, "alpha-max out of range, 0 to 255, was %d\n", opt->alpha_max);
          return 0;
        }
      } else if (!option.compare("--debug-color")) {
        if (value[0] != '0' && value[1] != 'x') {
          fprintf(stderr, "error: bad debug color: %s\n", value);
          return 0;
        }
        char *p;
        int color =(int)strtol(value + 2, &p, 16);
        opt->debug_color[0] = (color >> 16) & 0xFF;
        opt->debug_color[1] = (color >>  8) & 0xFF;
        opt->debug_color[2] = (color >>  0) & 0xFF;
      } else if (!option.compare("--range")) {
        const char* dash = strchr(value, '-');
        if (!dash) {
          fprintf(stderr, "error: bad range: %s\n", value);
          return 0;
        }
        int start = atoi(value);
        int end = atoi(dash + 1);
        if (start <= 0 || end < start) {
          fprintf(stderr, "error: bad range: %s\n", value);
          return 0;
        }
        for (int i = start; i <= end; ++i) {
          used.insert(i);
        }
      } else if (!option.compare("--used-chars-file")) {
        if (!addUsedCodepointsFromUTF8File(value, &used)) {
          fprintf(stderr, "error: can't read file: %s\n", value);
          return 0;
        }
      } else {
        fprintf(stderr, "error: unknown option: %s\n", arg);
        return 0;
      }
    }
  }

  if (opt->font_filename.empty()) {
    fprintf(stderr, "error: no font specified\n");
    return 0;
  }

  if (opt->font_size <= 0.0) {
    fprintf(stderr, "error: font size not specified\n");
    return 0;
  }

  if (opt->out_name.empty()) {
    fprintf(stderr, "error: outname not specified\n");
    return 0;
  }

  generateRangesFromUsed(used, &opt->ranges);

  if (!opt->ranges.size()) {
    fprintf(stderr, "error: no ranges specified\n");
    return 0;
  }

  if (opt->verbose) {
    printf("Ranges:\n");
    for (auto range : opt->ranges) {
      printf("  %d - %d\n", range.start, range.end);
    }
  }

  return 1;
}

const char* help = R"(
font-atlas-generator [options]
   --font <path to font>
   --font-index <font index of font to use in font file. default: 0>
   --font-size <size to generate, eg: 14>
   --padding <padding between characters, default: 1>
   --light <true> use FreeType's light rendering mode
   --atlas-width <width of atlas to generate, default: 0 = automatic>
   --atlas-height <height of atlas to generate, default: 0 = automatic>
   --glyph-height <make all glyphs this size, 0 = varying size, default: 0>
   --y-offset <offset to baseline, default: 0>
   --outname <base name of output, eg: foo, generates foo.json and foo.png>
   --oversample <amount to oversample, default: 1>
   --alpha-min <alpha> min alpha, alpha is stretched between min and max. default = 0
   --alpha-max <alpha> max alpha, alpha is stretched between min and max. default = 255
   --range <range to generate eg 32-127> note you can specify this multiple times
   --used-chars-file <file to scan for used files>
   --verbose <true> show more stuff
   --shift-x <shift-x> fractional amount to shift
   --shift-y <shift-y> fractional amount to shift
   --show-grid <true> change colors of each character rect
   --debug-color <hexcolor eg 0xFF0000> color to use for show-grid
   --ignore-errors <true> used for debugging to generate output
)";

std::string json_string(const std::string& s) {
  std::string d("\"");

  for (const auto c : s) {
    switch (c) {
    case '"':
    case '\\':
    case '\b':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
      d.push_back('\\');
      d.push_back(c);
      break;
    default:
      d.push_back(c);
      break;
    }
  }

  d.push_back('\"');
  return d;
}

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-AWESOME (tm Ryan Gordon) packing using stb_rect_pack.h. If
// stb_rect_pack.h isn't available, it uses the BakeFontBitmap strategy.

STBTT_DEF int PackBegin(stbtt_pack_context *spc, int pw, int ph, int stride_in_bytes, int padding, void *alloc_context)
{
   stbrp_context *context = (stbrp_context *) STBTT_malloc(sizeof(*context)            ,alloc_context);
   int            num_nodes = pw - padding;
   stbrp_node    *nodes   = (stbrp_node    *) STBTT_malloc(sizeof(*nodes  ) * num_nodes,alloc_context);

   if (context == NULL || nodes == NULL) {
      if (context != NULL) STBTT_free(context, alloc_context);
      if (nodes   != NULL) STBTT_free(nodes  , alloc_context);
      return 0;
   }

   spc->user_allocator_context = alloc_context;
   spc->width = pw;
   spc->height = ph;
   spc->pack_info = context;
   spc->nodes = nodes;
   spc->padding = padding;
   spc->stride_in_bytes = stride_in_bytes != 0 ? stride_in_bytes : pw;
   spc->h_oversample = 1;
   spc->v_oversample = 1;

   stbrp_init_target(context, pw-padding, ph-padding, nodes, num_nodes);

   return 1;
}

STBTT_DEF void PackEnd  (stbtt_pack_context *spc)
{
   STBTT_free(spc->nodes    , spc->user_allocator_context);
   STBTT_free(spc->pack_info, spc->user_allocator_context);
}

STBTT_DEF void PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects)
{
   stbrp_pack_rects((stbrp_context *) spc->pack_info, rects, num_rects);
}

unsigned char GetPixel(const FT_Bitmap& bm, int x, int y) {
  if (x < 0 || x >= (int)bm.width || y < 0 || y >= (int)bm.rows) {
    return 0;
  }

  switch (bm.pixel_mode) {
    case FT_PIXEL_MODE_MONO:
      return (bm.buffer[y * bm.pitch + (x >> 3)] & (0x80 >> (x & 0x7)))
        ? 255
        : 0;
    case FT_PIXEL_MODE_GRAY:
      return bm.buffer[y * bm.pitch + x];
    default:
      fprintf(stderr, "unknown pixel mode!!!!\n");
      return 0;
  }
}

void DumpBitmap(const FT_Bitmap& bm) {
  for (int y = 0; y < (int)bm.rows; ++y) {
    for (int x = 0; x < (int)bm.width; ++x) {
      printf("%s", GetPixel(bm, x, y) > 0 ? "*" : ".");
    }
    printf("\n");
  }
}

int PackFontRanges(stbtt_pack_context *spc, FT_Face face, stbtt_pack_range *ranges, int num_ranges, const Options& opt, std::vector<unsigned char>* pixels)
{
  stbrp_rect    *rects;

  //int baseline = opt.font_size * opt.oversample * ((float)face->ascender / (float)face->units_per_EM + 0.5f);
  //printf("  font size: %f\n", opt.font_size);
  //printf(" face height: %d\n", face->size->metrics.height >> 6);
  //printf(" face ascender: %d\n", face->size->metrics.ascender >> 6);
  //printf("  ascender: %d\n", face->ascender);
  //printf("  units_per_EM: %d\n", face->units_per_EM);
  //printf("  baseline: %d\n", baseline);
  int baseline = ((face->size->metrics.ascender + 63) >> 6) / opt.oversample + opt.y_offset;

  // flag all characters as NOT packed
  for (int i = 0; i < num_ranges; ++i) {
    for (int j = 0; j < ranges[i].num_chars; ++j) {
      ranges[i].chardata_for_range[j].x0 = 0;
      ranges[i].chardata_for_range[j].y0 = 0;
      ranges[i].chardata_for_range[j].x1 = 0;
      ranges[i].chardata_for_range[j].y1 = 0;
     }
   }

  int num_chars = 0;
  for (int i = 0; i < num_ranges; ++i) {
    num_chars += ranges[i].num_chars;
  }

   rects = (stbrp_rect *) STBTT_malloc(sizeof(*rects) * num_chars, spc->user_allocator_context);
   if (rects == NULL)
      return 0;

   int load_flags = opt.light ? FT_LOAD_TARGET_LIGHT : FT_LOAD_TARGET_NORMAL;
   FT_Render_Mode render_flags = opt.light ? FT_RENDER_MODE_LIGHT : FT_RENDER_MODE_NORMAL;

   {
     int k = 0;
     for (int i = 0; i < num_ranges; ++i) {
       const stbtt_pack_range& range = ranges[i];
       for (int j = 0; j < range.num_chars; ++j) {
         stbrp_rect* rect = &rects[k];
         const int codepoint = range.array_of_unicode_codepoints
            ? range.array_of_unicode_codepoints[j]
            : range.first_unicode_codepoint_in_range + j;
         int glyph_index = FT_Get_Char_Index(face, codepoint);
         if (glyph_index) {
           int error = FT_Load_Glyph(
               face,          /* handle to face object */
               glyph_index,   /* glyph index           */
               load_flags);   /* load flags, see below */
           // should probably pad each side separate?
           if (error) {
             fprintf(stderr, "warn: could not load glyph for codepoint: 0x%x\n", codepoint);
           } else {
             rect->w = (stbrp_coord)(((face->glyph->metrics.width + 63) >> 6) / opt.oversample + opt.padding * 2);
             rect->h = (stbrp_coord)(((face->glyph->metrics.height + 63) >> 6) / opt.oversample + opt.padding * 2);
             if (opt.verbose) {
               printf("   codepoint: 0x%x - %d x %d\n", codepoint, rect->w, rect->h);
             }
           }
         } else {
           fprintf(stderr, "warn: no glpyh for codepoint: 0x%x\n", codepoint);
           rect->w = 0;
           rect->h = 0;
         }
         ++k;
       }
     }
   }

   if (opt.glyph_height) {
     for (int i = 0; i < num_chars; ++i) {
       stbrp_rect* r = &rects[i];
       r->h = opt.glyph_height;
     }
   }

   bool auto_size = !opt.atlas_width;
   int atlas_width = auto_size ? 8 : opt.atlas_width;
   int atlas_height = auto_size ? 8 : opt.atlas_height;

   int return_value = 1;
   for (;;) {
     if (!PackBegin(spc, atlas_width, atlas_height, atlas_width, opt.padding, NULL)) {
       fprintf(stderr, "error: PackBegin\n");
       return_value = 0;
       break;
     }

     if (false && opt.glyph_height > 0) {
       // just pack in order if all the same height
       // this is only to make it easier to compare
       int x = 0;
       int y = 0;
       for (int i = 0; i < num_chars; ++i) {
         stbrp_rect* r = &rects[i];
         r->was_packed = true;
         int horiz_space_left = spc->width - x;
         if (horiz_space_left < r->w) {
           x = 0;
           y += r->h;
         }
         int vert_space_left = spc->height - y;
         if (vert_space_left < r->h) {
           r->was_packed = false;
           break;
         }
         r->x = x;
         r->y = y;
         x += r->w;
       }
     } else {
       PackFontRangesPackRects(spc, rects, num_chars);
     }

     bool pack_successful = true;
     for (int i = 0; i < num_chars; ++i) {
       stbrp_rect* r = &rects[i];
       if (!r->was_packed) {
         pack_successful = false;
         break;
       }
     }

     if (pack_successful) {
       break;
     } else {
       if (auto_size) {
         if (atlas_width > atlas_height) {
           atlas_height *= 2;
         } else {
           atlas_width *= 2;
         }
       } else {
         return_value = 0;
         break;
       }
     }
     if (opt.verbose) {
       printf("trying: %d x %d\n", atlas_width, atlas_height);
     }
     PackEnd(spc);
   }

   if (return_value) {
     if (auto_size) {
       pixels->resize(spc->width * spc->height);
       spc->pixels = pixels->data();
       spc->stride_in_bytes = spc->width;
     }


     bool crop_error = false;
     {
       int k = 0;
       for (int i = 0; i < num_ranges; ++i) { 
         const stbtt_pack_range& range = ranges[i];
         for (int j = 0; j < range.num_chars; ++j) {
           stbtt_packedchar* packed_char = &range.chardata_for_range[j];
           const int codepoint = range.array_of_unicode_codepoints
              ? range.array_of_unicode_codepoints[j]
              : range.first_unicode_codepoint_in_range + j;
           int glyph_index = FT_Get_Char_Index(face, codepoint);
           if (glyph_index) {
             int error = FT_Load_Glyph(
                 face,          /* handle to face object */
                 glyph_index,   /* glyph index           */
                 load_flags);   /* load flags, see below */
             if (error) {
               fprintf(stderr, "warn: could not load glyph for codepoint: 0x%x\n", codepoint);
               packed_char->x0 = 0;
               packed_char->y0 = 0;
               packed_char->x1 = 0;
               packed_char->y1 = 0;
               packed_char->xadvance = 0;
               packed_char->xoff = 0;
               packed_char->yoff = 0;
               packed_char->xoff2 = 0;
               packed_char->yoff2 = 0;
             } else {
               FT_Render_Glyph(
                 face->glyph,
                 render_flags);
               const stbrp_rect rect = rects[k];
               const auto& glyph = face->glyph;
               const auto& bm = glyph->bitmap;
               //DumpBitmap(bm);

               int src_y_start = 0;
               int dst_y_start = opt.glyph_height ? baseline - ((glyph->bitmap_top + opt.oversample - 1) / opt.oversample) : 0;
               int dst_y_end = dst_y_start + ((bm.rows + opt.oversample - 1) / opt.oversample);
               if (dst_y_start < 0) {
                 dst_y_end += dst_y_start;
                 src_y_start -= dst_y_start;
                 fprintf(stderr, "codepoint 0x%x truncated at top by %d pixels\n", codepoint, -dst_y_start);
                 dst_y_start = 0;
                 crop_error = true;
               }
               int max_height = rect.h;
               if (dst_y_end > max_height) {
                 fprintf(stderr, "codepoint 0x%x truncated at bottom by %d pixels\n", codepoint, dst_y_end - max_height);
                 dst_y_end = max_height;
                 crop_error = true;
               }
               static unsigned char masks[] = { 0x66, 0x44, 0x88 };

               int alpha_range = opt.alpha_max - opt.alpha_min;
               int scale_sq = opt.oversample * opt.oversample;
               int num_rows = dst_y_end - dst_y_start;
               for (int y = 0; y < num_rows; ++y) {
                 unsigned char* dst = spc->pixels + (rect.y + dst_y_start + y + opt.padding) * spc->stride_in_bytes + rect.x + opt.padding;
                 for (int x = 0; x < (int)((bm.width + opt.oversample - 1) / opt.oversample); ++x) {
                   int pixel = 0;
                   for (int yy = 0; yy < opt.oversample; ++yy) {
                     for (int xx = 0; xx < opt.oversample; ++xx) {
                       pixel += GetPixel(bm, x * opt.oversample + xx, y * opt.oversample + yy);
                     }
                   }
                   pixel = (pixel + opt.oversample / 2) / scale_sq;

                   if (alpha_range > 1) {
                     pixel = std::min(255, std::max(0, pixel - opt.alpha_min) * 255 / alpha_range);
                   } else {
                     pixel = pixel > opt.alpha_min ? 255 : 0;
                   }

                   if (opt.show_grid) {
                     pixel |= masks[k % sizeof(masks)];
                   }
                   *dst++ = pixel;
                 }
               }

               packed_char->x0 = rect.x + opt.padding;
               packed_char->y0 = rect.y + opt.padding;
               packed_char->x1 = rect.x + rect.w - opt.padding * 2 + 1;
               packed_char->y1 = rect.y + rect.h - opt.padding * 2 + 1;
               packed_char->xadvance = (float)(glyph->advance.x) / 64.0f / opt.oversample;
               packed_char->xoff = (float)(glyph->metrics.horiBearingX) / 64.0f / (float)opt.oversample;
               packed_char->yoff = opt.glyph_height > 0
                 ? (float)(glyph->metrics.horiBearingY) / 64.0f / (float)opt.oversample
                 : 0;
               packed_char->xoff2 = -123456; // not implemented
               packed_char->yoff2 = -123456; // not implemented
             }
           }
           ++k;
         }
       }
     }

     if (opt.error_on_crop && crop_error) {
       return_value = 0;
     }
   }

   STBTT_free(rects, spc->user_allocator_context);
   stbtt_PackEnd(spc);
   return return_value;
}

int main(int argc, const char *argv[])
{
  Options opt;
  if (!parse_command_line(argc, argv, &opt)) {
    fprintf(stderr, help);
    return EXIT_FAILURE;
  }

  FT_Library library;

  int error = FT_Init_FreeType(&library);
  if (error) {
    fprintf(stderr, "an error occurred during freetype library initialization");
    return EXIT_FAILURE;
  }

  FT_Face face;
  error = FT_New_Face(library, opt.font_filename.c_str(), opt.font_index, &face);
  if (error) {
    fprintf(stderr, "error: could not read: %s\n", opt.font_filename.c_str());
    return EXIT_FAILURE;
  }

  printf("font: %s\n", opt.font_filename.c_str());
  if (opt.verbose) {
    printf("  num glyphs: %d\n", face->num_glyphs);
    printf("  num fixed sizes: %d\n", face->num_fixed_sizes);
    for (int i = 0; i < face->num_fixed_sizes; ++i) {
      const FT_Bitmap_Size& size = face->available_sizes[i];
      printf("    %d: %d x %d\n", i, size.width, size.height);
    }
  }

  error = FT_Set_Char_Size(
      face,                /* handle to face object           */
      0,                   /* char_width in 1/64th of points  */
      (FT_F26Dot6)(opt.font_size * 64),  /* char_height in 1/64th of points */
      72 * opt.oversample,      /* horizontal device resolution    */
      72 * opt.oversample);     /* vertical device resolution      */



  std::vector<unsigned char> pixels(opt.atlas_width * opt.atlas_height);

  int total_chars = 0;
  for (const auto& range : opt.ranges) {
    int num_chars = range.end - range.start + 1;
    total_chars += num_chars;
  }
  std::vector<stbtt_packedchar> chardata_for_range(total_chars);
  std::vector<stbtt_pack_range> ranges(opt.ranges.size());
  int dst_ndx = 0;
  for (size_t i = 0; i < opt.ranges.size(); ++i) {
    const auto& range = opt.ranges[i];
    auto& pack_range = ranges[i];

    pack_range.font_size = opt.font_size;
    pack_range.first_unicode_codepoint_in_range = range.start;
    pack_range.array_of_unicode_codepoints = NULL;
    pack_range.num_chars = range.end - range.start + 1;
    pack_range.chardata_for_range = &chardata_for_range[dst_ndx];
    dst_ndx += pack_range.num_chars;
  }

  // printf("pack font: %s\n", opt.font_filename.c_str());

  stbtt_pack_context context = {};
  if (!PackFontRanges(&context, face, ranges.data(), ranges.size(), opt, &pixels)) {
    fprintf(stderr, "error packing font: %s\n", opt.font_filename.c_str());
    return EXIT_FAILURE;
  }


  // printf("end pack font: %s\n", opt.out_name.c_str());

  std::string png_filename = std::string(opt.out_name) + ".png";

  printf("write font atlas: %s\n", png_filename.c_str());

  // expand to 4 channels
  int channels = 4;
  std::vector<unsigned char> rgba(context.width * context.height * channels);
  for (int y = 0; y < context.height; ++y) {
    for (int x = 0; x < context.width; ++x) {
      const int srcOffset = y * context.width + x;
      const int dstOffset = srcOffset * 4;
      const int alpha = pixels[srcOffset];
      rgba[dstOffset + 0] = alpha ? opt.debug_color[0] : 0;
      rgba[dstOffset + 1] = alpha ? opt.debug_color[1] : 0;
      rgba[dstOffset + 2] = alpha ? opt.debug_color[2] : 0;
      rgba[dstOffset + 3] = alpha;
    }
  }
  if (!stbi_write_png(png_filename.c_str(), context.width, context.height, channels, rgba.data(), context.stride_in_bytes * 4)) {
    fprintf(stderr, "error: couldn't write %s\n", png_filename.c_str());
    return EXIT_FAILURE;
  }
  /*
  } else {
    int channels = 1;
    if (!stbi_write_png(png_filename.c_str(), context.width, context.height, channels, pixels.data(), context.stride_in_bytes)) {
      fprintf(stderr, "error: couldn't write %s\n", png_filename.c_str());
      return EXIT_FAILURE;
    }
  }
  */

  int baseline = 0;
#if 0
  {
    stbtt_fontinfo font;
    stbtt_InitFont(&font, font_data.data(), 0);

    const float scale = stbtt_ScaleForPixelHeight(&font, opt.font_size);
    int ascent;
    stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
    baseline = (int)(ascent * scale) + opt.y_offset;
  }
#endif

  std::string json_filename = std::string(opt.out_name) + ".json";

  printf("write font data: %s\n", json_filename.c_str());

  FILE* file = fopen(json_filename.c_str(), "wb");
  fprintf(file, R"({
  "font": %s,
  "fontSize": %g,
  "fontIndex": %d,
  "baseline": %d,
  "yOffset": %d,
  "oversample": %d,
  "padding": %d,
  "atlasWidth": %d,
  "atlasHeight": %d,
  "atlas": %s,
  "glyphs": [
)", json_string(opt.font_filename).c_str(),
    opt.font_size,
    opt.font_index,
    baseline,
    opt.y_offset,
    opt.oversample,
    opt.padding,
    context.width,
    context.height,
    json_string(png_filename).c_str());

  const int last_ndx = total_chars - 1;
  int ndx = 0;
  for (const auto& pack_range : ranges) {
    for (int i = 0; i < pack_range.num_chars; ++i) {
      const stbtt_packedchar& packed_char = pack_range.chardata_for_range[i];
      fprintf(file, R"(    {
      "codePoint": %d,
      "tex": { "x": %d, "y": %d, "w": %d, "h": %d },
      "xOff": %g,
      "yOff": %g,
      "xAdvance": %g,
      "xOff2": %g,
      "yOff2": %g
    }%s
)",
              pack_range.first_unicode_codepoint_in_range + i,
              packed_char.x0,
              packed_char.y0,
              packed_char.x1 - packed_char.x0 + 1,
              packed_char.y1 - packed_char.y0 + 1,
              packed_char.xoff,
              packed_char.yoff,
              packed_char.xadvance,
              packed_char.xoff2,
              packed_char.yoff2,
              ndx == last_ndx ? "" : ",");
      ++ndx;
    }
  }
  fprintf(file, R"(  ]
}
)");

  fclose(file);

  return EXIT_SUCCESS;
}

