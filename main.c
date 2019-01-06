#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/sysinfo.h>

#include "font.h"
#include "ds.h"



static uint32_t* utf8_to_utf32( char*  u8, size_t* outLen) {
	size_t len = 0;
	size_t u8len = strlen(u8);
	
	uint32_t* u32 = malloc((u8len + 1) * sizeof(u32)); // just overallocate
	
	uint8_t* s = (uint8_t*)u8;
	
#define to32s(x, s) (((uint32_t)(x)) << (s))
	
	int i = 0;
	while(*s) {
		if((s[0] & 0x80) == 0x00) { // single byte
			u32[i] = s[0];
			s++;
		}
		else if((s[0] & 0xe0) == 0xc0) { // two bytes
			if(s[1] == 0) goto MALFORMED;
			u32[i] = to32s(s[0] & 0x1f, 6) | to32s(s[1] & 0x3f, 0);
			s += 2;
		}
		else if((s[0] & 0xf0) == 0xe0) { // three bytes
			if(s[1] == 0 || s[2] == 0) goto MALFORMED;
			u32[i] = to32s(s[0] & 0x1f, 12) | to32s(s[1] & 0x3f, 6) | to32s(s[2] & 0x3f, 0);
			s += 3;
		}
		else if((s[0] & 0xf8) == 0xf0) { // four bytes
			if(s[1] == 0 || s[2] == 0 || s[3] == 0) goto MALFORMED;
			u32[i] = to32s(s[0] & 0x1f, 18) | to32s(s[1] & 0x3f, 12) | to32s(s[2] & 0x3f, 6) | to32s(s[3] & 0x3f, 0);
			s += 4;
		}
		
		i++;
	}
	
	u32[i] = 0;
	if(outLen) *outLen = i;
	
	return u32;
	
	
MALFORMED:
	fprintf(stderr, "Malformed UTF-8 sequence.\n");
	exit(1); // we ain't havin none of that shit
}


static char* strappend(char* a, const char* const b);
static char* strappendf(char* a, const char* const b);

static char* strappendf(char* a, const char* const b) {
	char* o = strappend(a, b);
	free(a);
	return o;
}

static char* strappend(char* a, const char* const b) {
	if(a == NULL) return strdup(b);
	if(b == NULL) return a;
	
	size_t la = strlen(a);
	size_t lb = strlen(b);
	char* o = malloc(la + lb + 1);
	strcpy(o, a);
	strcpy(o + la, b);
	o[la + lb] = '\0';
	return o;
}


static uint32_t* u32strdup(const uint32_t* const s) {
	size_t l = u32strlen(s);
	uint32_t* o = malloc(l * sizeof(*s));
	memcpy(o, s, (l + 1) * sizeof(*s));
	return o;
}

static uint32_t* u32strappend(uint32_t* a, const uint32_t* const b) {
	if(a == NULL) return u32strdup(b);
	if(b == NULL) return a;
	
	size_t la = u32strlen(a);
	size_t lb = u32strlen(b);
	uint32_t* o = malloc(la + lb + 1);
	memcpy(o, a, la * sizeof(*a));
	memcpy(o + (la * sizeof(*a)), b, lb * sizeof(*b));
	o[(la + lb) * sizeof(*o)] = 0;
	return o;
}

static uint32_t* u32strappendf(uint32_t* a, const uint32_t* const b) {
	uint32_t* o = u32strappend(a, b);
	free(a);
	return o;
}

static void struniq(char* s) {
	if(s == NULL || s[0] == 0 || s[1] == 0) return;
	char* b = s;
	s++;
	while(*s) {
		if(*s != *b) {
			b++;
			*b = *s;
		}
		s++;
	}
	*(b + 1) = 0;
}

static void u32struniq(uint32_t* s) {
	if(s == NULL || s[0] == 0 || s[1] == 0) return;
	uint32_t* b = s;
	s++;
	while(*s) {
		if(*s != *b) {
			b++;
			*b = *s;
		}
		s++;
	}
	*(b + 1) = 0;
}



static int cs_comp(uint32_t* a, uint32_t* b) {
	return *a - *b;
}

// frees a
static uint32_t* appendCharset(uint32_t* a, const uint32_t* const b) {
	uint32_t* o = u32strappendf(a, b);
	
	qsort(o, u32strlen(o), sizeof(uint32_t), (void*)cs_comp);
	
	u32struniq(o);
	
	return o;
}

static int int_comp(int* a, int* b) {
	return *a - *b;
}




char* helpText = "" \
	"Usage:\n" \
	"sdfgen [GLOBAL OPTION]... [-f FONT SIZES [OPTION]...]... [outfile]\n" \
	"\n" \
	"EXAMPLE:\n" \
	"  sdfgen -b -m 8 -o 4 -t 256 -f \"Courier\" 8,12,16 -i -f \"Arial\" 32 myfonts_sdf\n" \
	"     -b - generate bold for all fonts\n" \
	"     -m 8 - SDF magnitude of 8 pixels\n" \
	"     -o 4 - oversample by 4x\n" \
	"     -t 256 - output textures have 256px dimensions\n" \
	"     -f \"Courier\" - first font name \n" \
	"        8,12,16 - generate outputs of size 8, 12, and 16 px for this font \n" \
	"        -i - also generate italic for this font\n" \
	"     -f \"Arial\" - second font name \n" \
	"        32 - generate outputs of size 32px for this font \n" \
	"     myfonts_sdf - output will be myfonts_sdf_[n].png and myfonts_sdf.json\n" \
	"\n" \
	"OPTIONS:\n" \
	"     All Font Options can be included as global options for all fonts.\n" \
	"  -J         Specify the output JSON file name\n" \
	"  -l N       Limit computation to N threads\n" \
	"  -m         SDF magnitude in pixels. Default 8.\n" \
	"  -o         Amount of pixels to oversample by. Default 16.\n" \
	"  -P FORMAT  Specify the png file name; %d will be expanded to the index.\n" \
	"  -q         Quiet. Suppress all output, even warnings and errors.\n" \
	"  -t [N|fit] Size of output texture in pixels\n" \
	"               'fit' generates a single texture large enough to hold all glyphs\n" \
	"  -v         Verbose.\n" \
	"\n" \
	"FONT OPTIONS:\n" \
	"  --          Stop parsing a font and return to global options. Not required.\n" \
	"  -b         Generate Bold\n" \
	"  -bi        Generate Bold Italic\n" \
	"  -c CHARSET Specify the charset. Default is every character on a US keyboard.\n" \
	"  -i         Generate Italic\n" \
	"  --omit-regular  Do not generate the regular font face\n" \
	"  -s N[,...] Generate these output font sizes.\n" \
	"\n" \
	"MISC OPTIONS:\n" \
	"  --help      Print this message and exit\n" \
	"  --pretend   Verify parameters but do not actually generate output.\n" \
;




typedef struct FontOpts {
	char* name;
	char bold;
	char italic;
	char bold_italic;
	char omit_regular;
	uint32_t* charset;
	VEC(int) sizes;
} FontOpts;


static FontOpts* newFontOpts(char* name);
static FontOpts* newFontOpts(char* name) {
	FontOpts* fo;
	
	fo = calloc(1, sizeof(*fo));
	fo->name = strdup(name);
	VEC_INIT(&fo->sizes);
	
	return fo;
}



int main(int argc, char* argv[]) {
	int an;
	
	#define default_outfile "sdf-output"
	#define default_png_outfile default_outfile "-%d";
	
	char* outfile = NULL;
	char* json_outfile = NULL;
	char* png_outfile = NULL;
	uint32_t* g_charset = utf8_to_utf32("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 `~!@#$%^&*()_+|-=\\{}[]:;<>?,./'\"", NULL);
	char g_bold = 0;
	char g_italic = 0;
	char g_bold_italic = 0;
	char g_omit_regular = 0;
	VEC(int) g_sizes;
	VEC(FontOpts*) fonts;
	int magnitude = 8;
	int oversample = 16;
	int maxCores = get_nprocs();
	int verbose = 0;
	char pretend = 0;
	int texSize = 0;
	
	int totalFacesToRender = 0;
	int totalGlyphsToRender = 0;
	
	VEC_INIT(&g_sizes);
	VEC_INIT(&fonts);
	
	for(an = 1; an < argc; an++) {
		char* arg = argv[an];
		
		if(0 == strcmp(arg, "--")) {
			printf("invalid usage of '--'\n");
			exit(1);
		};
		
		if(0 == strcmp(arg, "--help")) {
			puts(helpText);
			exit(0);
		}
		
		if(0 == strcmp(arg, "-b")) {
			g_bold = 1;
			continue;
		}
		if(0 == strcmp(arg, "-bi")) {
			g_bold_italic = 1;
			continue;
		}
		if(0 == strcmp(arg, "-c")) {
			printf("oh no!!\n");
			// TODO: detect and strip quotes
			an++;
			g_charset = utf8_to_utf32(argv[an], NULL);
			continue;
		}
		if(0 == strcmp(arg, "-i")) {
			g_italic = 1;
			continue;
		}

		if(0 == strcmp(arg, "-J")) {
			an++;
			json_outfile = strdup(argv[an]);
			continue;
		}

		if(0 == strcmp(arg, "-l")) {
			an++;
			int n = strtol(argv[an], NULL, 10);
			if(n > 0) maxCores = n;
			continue;
		}

		if(0 == strcmp(arg, "-m")) {
			an++;
			int n = strtol(argv[an], NULL, 10);
			if(n > 0) magnitude = n;
			continue;
		}
		
		// oversample
		if(0 == strcmp(arg, "-o")) {
			an++;
			int n = strtol(argv[an], NULL, 10);
			if(n > 0) oversample = n;
			continue;
		}
		
		// don't generate regular fonts
		if(0 == strcmp(arg, "--omit-regular")) {
			g_omit_regular = 1;
			continue;
		}
		
		if(0 == strcmp(arg, "-P")) {
			an++;
			png_outfile = strdup(argv[an]);
			continue;
		}
		
		if(0 == strcmp(arg, "--pretend")) {
			pretend = 1;
			continue;
		}
		
		if(0 == strcmp(arg, "-q")) {
			verbose = -1;
			continue;
		}
		
		
		// global sizes
		if(0 == strcmp(arg, "-s")) {
			an++;
			arg = argv[an];
			
			while(arg[0]) {
				int n = strtol(arg, &arg, 10);
				if(n <= 0) break;
				
				VEC_PUSH(&g_sizes, n);
				
				if(arg[0] != ',') break;
				arg++;
			}
			
			continue;
		}
		
		if(0 == strcmp(arg, "-t")) {
			an++;
			arg = argv[an];
			if(0 == strcmp(arg, "fit")) {
				texSize = 0;
			}
			else {
				int n = strtol(arg, NULL, 10);
				if(n > 0) texSize = n;
			}
			
			continue;
		}
		
		if(0 == strcmp(arg, "-v")) {
			verbose = 1;
			continue;
		}
		if(0 == strcmp(arg, "-vv")) {
			verbose = 2;
			continue;
		}
		
		
		// font specification
		if(0 == strcmp(arg, "-f")) {
			an++;
			
			FontOpts* fo = newFontOpts(argv[an]);
			VEC_PUSH(&fonts, fo);
			
			
			for(an++; an < argc; an++) {
				arg = argv[an];
				
				// return to normal options
				if(0 == strcmp(arg, "--")) break;
				
				// bold
				if(0 == strcmp(arg, "-b")) {
					fo->bold = 1;
					continue;
				}
				if(0 == strcmp(arg, "-bi")) {
					fo->bold_italic = 1;
					continue;
				}
				// charset
				if(0 == strcmp(arg, "-c")) {
					// TODO: detect and strip quotes
					printf("C!!\n");
					an++;
					fo->charset = utf8_to_utf32(argv[an], NULL);
					continue;
				}
				// italics
				if(0 == strcmp(arg, "-i")) {
					fo->italic = 1;
					continue;
				}
				
				// don't generate the regular font
				if(0 == strcmp(arg, "--omit-regular")) {
					fo->omit_regular = 1;
					continue;
				}
				
				// sizes
				if(0 == strcmp(arg, "-s")) {
					an++;
					arg = argv[an];
					
					while(arg[0]) {
						int n = strtol(arg, &arg, 10);
						if(n == 0) break;
						
						VEC_PUSH(&fo->sizes, n);
						
						if(arg[0] != ',') break;
						arg++;
					}
					
					continue;
				}
				
				
				// next font
				// TODO: check for -'s and error
				an--;
				break;
			}
			
			continue;
		}
		
		
		
		// output file
		// TODO: check for -'s and error
		outfile = strdup(arg);
	}
	
	
	// check and print the options
	if(!outfile) {
		json_outfile = default_outfile;
		png_outfile = default_png_outfile;
	}
	else {
		if(!json_outfile) {
			json_outfile = outfile;
		}
		if(!png_outfile) {
			png_outfile = outfile;
		}
	}
	

	// TODO: check for and append %d if needed
	
	// TODO: check for and append .png and .json 
	json_outfile = strappend(json_outfile, ".json");
	png_outfile = strappend(png_outfile, ".png");
	
	// clean things up a bit
	VEC_SORT(&g_sizes, (void*)int_comp);
	
	//  expand global options into FontOpt list
	VEC_EACH(&fonts, fi, fo) {
		fo->charset = appendCharset(fo->charset, g_charset);
		
		fo->bold |= g_bold;
		fo->italic |= g_italic;
		fo->bold_italic |= g_bold_italic;
		fo->omit_regular |= g_omit_regular;
		
		int n = 0;
		if(fo->bold) n++;
		if(fo->italic) n++;
		if(fo->bold_italic) n++;
		if(!fo->omit_regular) n++;
		
		// yeah... shutup. there aren't many sizes
		VEC_EACH(&g_sizes, gi, gs) {
			int found = 0;
			VEC_EACH(&fo->sizes, fsi, fss) {
				if(gs == fss) {
					found = 1;
					break;
				}
			}
			if(!found) VEC_PUSH(&fo->sizes, gs);
		}
		
		VEC_SORT(&fo->sizes, (void*)int_comp);
		
		totalFacesToRender += n * VEC_LEN(&fo->sizes);
		totalGlyphsToRender += n * u32strlen(fo->charset) * VEC_LEN(&fo->sizes);
	}
	
	// TODO: check that each face can be loaded and is unique
	
	
	// sanity check
	if(texSize > 0 && texSize < 16 ) {
		if(verbose >= 0) printf("WARNING: Texure Size is suspiciously low: %dpx\n", texSize);
	}
	// check for power of 2
	if(texSize > 0 && (texSize != (texSize & (1 - texSize)))) {
		if(verbose >= 0) printf("WARNING: Texure Size is not a power of two: %dpx\n  GPUs like power of two textures.\n", texSize);
	}
	
	
	
	if(verbose >= 1) {
		printf("JSON output: %s\n", json_outfile);
		printf("PNG output: %s\n", png_outfile);
		printf("Oversample: %d\n", oversample);
		printf("Magnitude: %d\n", magnitude);
		if(g_omit_regular) puts("Omitting regular font face.");
		//printf("Charset: %s\n", g_charset); // TODO: escape 
		printf("Verbosity: %d\n", verbose);
		printf("Max Threads: %d\n", maxCores);
		if(texSize == 0) {
			printf("Tex Size: fit\n");
		}
		else {
			printf("Tex Size: %dx%d\n", texSize, texSize);
		}

		
		puts("Fonts:");
		VEC_EACH(&fonts, fi, fo) {
			printf("  %s\n", fo->name);
			printf("    Faces: ");
			int n = 0;
			if(!fo->omit_regular) { n++; printf("regular "); }
			if(fo->bold) { n++; printf("bold "); }
			if(fo->italic) { n++; printf("italic "); }
			if(fo->bold_italic) { n++; printf("bold/italic "); }
			if(n == 0) printf("--none--");
			printf("\n");
			
			printf("    Sizes: ");
			VEC_EACH(&fo->sizes, i, size) {
				printf("%d", size);
				if(i < VEC_LEN(&fo->sizes) - 1) printf(",");
			}
			printf("\n");
		}
		
		
		printf("Total Font Faces to Render: %d\n", totalFacesToRender);
		printf("Total Glyphs to Render: %d\n", totalGlyphsToRender);
	}
	
	
	
	
	
	// start rendering
	if(pretend) {
		if(verbose >= 0) printf("Skipping generation (--pretend)\n");
	}
	else {
		
		if(totalGlyphsToRender == 0) {
			if(verbose >= 0) printf("Nothing to generate. Check your options (use -v).\n");
			exit(0);
		}
		
		FontManager* fm;
		
		fm = FontManager_alloc();

		fm->pngFileFormat = png_outfile;
		
		fm->oversample = oversample;
		fm->magnitude = magnitude;
		
		fm->maxThreads = maxCores;
		fm->verbose = verbose;
		fm->maxAtlasSize = texSize;
		
		VEC_EACH(&fonts, fi, fo) {
			VEC_EACH(&fo->sizes, si, size) {
				if(!fo->omit_regular) FontManager_addFont2(fm, fo->name, fo->charset, size, 0, 0);
				if(fo->bold) FontManager_addFont2(fm, fo->name, fo->charset, size, 1, 0);
				if(fo->italic) FontManager_addFont2(fm, fo->name, fo->charset, size, 0, 1);
				if(fo->bold_italic) FontManager_addFont2(fm, fo->name, fo->charset, size, 1, 1);
			}
		}
		
		FontManager_finalize(fm);
		
		if(VEC_LEN(&fm->gen) == 0) {
			if(verbose >= 0) printf("No glyphs generated. Error?\n");
			exit(0);
		}
		
		FontManager_createAtlas(fm);
		
		FontManager_saveJSON(fm, json_outfile);
		
// 		FontManager_saveAtlas(fm, "fonts.atlas");
		
		
	}
		
	return 0;
}






