



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <pthread.h>
#include <sys/sysinfo.h>

// FontConfig
#include "fcfg.h"

// for sdf debugging
#include "dumpImage.h"


// #include "utilities.h"
#include "font.h"



// temp
static void addFont(FontManager* fm, char* name);
static FontGen* addChar(FontManager* fm, FT_Face* ff, int code, int fontSize, char bold, char italic);



// super nifty site:
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline int nextPOT(int in) {
	
	in--;
	in |= in >> 1;
	in |= in >> 2;
	in |= in >> 4;
	in |= in >> 8;
	in |= in >> 16;
	in++;
	
	return in;
}




FontManager* FontManager_alloc() {
	FontManager* fm;
	
	fm = calloc(1, sizeof(*fm));
	HT_init(&fm->fonts, 4);
	fm->oversample = 16;
	fm->maxAtlasSize = 512;

	FontManager_init(fm);
	
	return fm;
}


void FontManager_init(FontManager* fm) {
	
// 	if(FontManager_loadAtlas(fm, "fonts.atlas")) {
// 		
// 		FontManager_addFont(fm, "Arial");
// 		FontManager_addFont(fm, "Impact");
// 		FontManager_addFont(fm, "Modern");
// 		FontManager_addFont(fm, "Times New Roman");
// 		FontManager_addFont(fm, "Courier New");
// 		FontManager_addFont(fm, "Copperplate");
// 		
// 		FontManager_finalize(fm);
// 
// 		FontManager_createAtlas(fm);
// 		FontManager_saveAtlas(fm, "fonts.atlas");
// 	}
// 	
// 	HT_get(&fm->fonts, "Arial", &fm->helv);
}




GUIFont* FontManager_findFont(FontManager* fm, char* name) {
	GUIFont* f;
	
// 	if(HT_get(&fm->fonts, name, &f)) {
// 		return fm->helv; // fallback
// 	}
	
	return f;
}







// new font rendering info
static char* defaultCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 `~!@#$%^&*()_+|-=\\{}[]:;<>?,./'\"";

// 16.16 fixed point to float conversion
static float f2f(uint32_t i) {
	return ((double)(i) / 65536.0);
}

// 26.6 fixed point to float conversion
static float f2f26_6(uint32_t i) {
	return ((double)(i) / 64.0);
}


static void blit(
	int src_x, int src_y, int dst_x, int dst_y, int w, int h,
	int src_w, int dst_w, unsigned char* src, unsigned char* dst) {
	
	
	int y, x, s, d;
	
	// this may be upside down...
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			s = ((y + src_y) * src_w) + src_x + x;
			d = ((y + dst_y) * dst_w) + dst_x + x;
			
			dst[d] = src[s];
		}
	}
}

static FT_Library ftLib = NULL;
static void checkFTlib();

static void checkFTlib() {
	FT_Error err;
	if(!ftLib) {
		err = FT_Init_FreeType(&ftLib);
		if(err) {
			fprintf(stderr, "Could not initialize FreeType library.\n");
			exit(1);
		}
	}
}


static float dist(int a, int b) {
	return a*a + b*b;
}
static float dmin(int a, int b, float d) {
	return fmin(dist(a, b), d);
}

static int boundedOffset(int x, int y, int ox, int oy, int w, int h) {
	int x1 = x + ox;
	int y1 = y + oy;
	if(x1 < 0 || y1 < 0 || x1 >= w || y1 >= h) return -1;
	return x1 + (w * y1);
}

static uint8_t sdfEncode(float d, int inside, float maxDist) {
	int o;
	d = sqrt(d);
	float norm = d / maxDist;
	if(inside) norm = -norm;
	
	o = (norm * 192) + 64;
	
	return o < 0 ? 0 : (o > 255 ? 255 : o);
}

void CalcSDF_Software_(FontGen* fg) {
	
	int searchSize;
	int x, y, ox, oy, sx, sy;
	int dw, dh;
	
	uint8_t* input;
	uint8_t* output;
	
	float d, maxDist;
	
	searchSize = fg->oversample * fg->magnitude;
	maxDist = 0.5 * searchSize;
	dw = fg->rawGlyphSize.x;
	dh = fg->rawGlyphSize.y;
	
	// this is wrong
	fg->sdfGlyphSize.x = floor(((float)(dw/* + (2*fg->magnitude)*/) / (float)(fg->oversample)) + .5);
	fg->sdfGlyphSize.y = floor(((float)(dh/* + (2*fg->magnitude)*/) / (float)(fg->oversample)) + .5); 
	
	fg->sdfGlyph = output = malloc(fg->sdfGlyphSize.x * fg->sdfGlyphSize.y * sizeof(uint8_t));
	input = fg->rawGlyph;
	
	fg->sdfBounds.min.x = 0;
	fg->sdfBounds.max.y =  fg->sdfGlyphSize.y;
	fg->sdfBounds.max.x = fg->sdfGlyphSize.x; 
	fg->sdfBounds.min.y = 0;
	
	// calculate the sdf 
	for(y = 0; y < fg->sdfGlyphSize.y; y++) {
		for(x = 0; x < fg->sdfGlyphSize.x; x++) {
			int sx = x * fg->oversample;
			int sy = y * fg->oversample;
			//printf(".");
			// value right under the center of the pixel, to determine if we are inside
			// or outside the glyph
			int v = input[sx + (sy * dw)];
			
			d = 999999.99999;
			
			
			for(oy = -searchSize / 2; oy < searchSize; oy++) {
				for(ox = -searchSize / 2; ox < searchSize; ox++) {
					int off = boundedOffset(sx, sy, ox, oy, dw, dh);
					if(off >= 0 && input[off] != v) 
						d = dmin(ox, oy, d);
				}
			}
			
			int q = sdfEncode(d, v, maxDist);
			//printf("%d,%d = %d (%f)\n",x,y,q,d);
			
			output[x + (y * fg->sdfGlyphSize.x)] = q;
		}
	}
	
	
	// find the bounds of the sdf data
	// first rows
	for(y = 0; y < fg->sdfGlyphSize.y; y++) {
		int hasData = 0;
		for(x = 0; x < fg->sdfGlyphSize.x; x++) {
			hasData += output[x + (y * fg->sdfGlyphSize.x)];
		}
		
		if(hasData / fg->sdfGlyphSize.x < 255) {
			fg->sdfBounds.min.y = y;
			break;
		}
	}
	for(y = fg->sdfGlyphSize.y - 1; y >= 0; y--) {
		int hasData = 0;
		for(x = 0; x < fg->sdfGlyphSize.x; x++) {
			hasData += output[x + (y * fg->sdfGlyphSize.x)];
		}
		
		if(hasData / fg->sdfGlyphSize.x < 255) {
			fg->sdfBounds.max.y = y + 1;
			break;
		}
	}

	for(x = 0; x < fg->sdfGlyphSize.x; x++) {
		int hasData = 0;
		for(y = 0; y < fg->sdfGlyphSize.y; y++) {
			hasData += output[x + (y * fg->sdfGlyphSize.x)];
		}
		
		if(hasData / fg->sdfGlyphSize.y < 255) {
			fg->sdfBounds.min.x = x;
			break;
		}
	}
	for(x = fg->sdfGlyphSize.x - 1; x >= 0; x++) {
		int hasData = 0;
		for(y = 0; y < fg->sdfGlyphSize.y; y++) {
			hasData += output[x + (y * fg->sdfGlyphSize.x)];
		}
		
		if(hasData / fg->sdfGlyphSize.y < 255) {
			fg->sdfBounds.max.x = x + 1;
			break;
		}
	}
	
	fg->sdfDataSize.x = fg->sdfBounds.max.x - fg->sdfBounds.min.x;
	fg->sdfDataSize.y = fg->sdfBounds.max.y - fg->sdfBounds.min.y;
	
	free(fg->rawGlyph);
}




static FontGen* addChar(FontManager* fm, FT_Face* ff, int code, int fontSize, char bold, char italic) {
	FontGen* fg;
	FT_Error err;
	FT_GlyphSlot slot;
	
	fg = calloc(1, sizeof(*fg));
	fg->code = code;
	fg->italic = italic;
	fg->bold = bold;
	fg->oversample = fm->oversample;
	fg->magnitude = fm->magnitude;
	
	int rawSize = fontSize * fm->oversample;
	
	
	err = FT_Set_Pixel_Sizes(*ff, 0, rawSize);
	if(err) {
		fprintf(stderr, "Could not set pixel size to %dpx.\n", rawSize);
		free(fg);
		exit(1);
	}
	
	
	err = FT_Load_Char(*ff, code, FT_LOAD_DEFAULT | FT_LOAD_MONOCHROME);
	
	//f2f(slot->metrics.horiBearingY);
	
	// draw character to freetype's internal buffer and copy it here
	FT_Load_Char(*ff, code, FT_LOAD_RENDER);
	// slot is a pointer
	slot = (*ff)->glyph;
	
	// typographic metrics for later. has nothing to do with sdf generation
	fg->rawAdvance = f2f(slot->linearHoriAdvance); 
	fg->rawBearing.x = f2f26_6(slot->metrics.horiBearingX); 
	fg->rawBearing.y = f2f26_6(slot->metrics.horiBearingY); 
	
	
	// back to sdf generation
	Vector2i rawImgSz = {(slot->metrics.width >> 6), (slot->metrics.height >> 6)};
	
	fg->rawGlyphSize.x = (slot->metrics.width >> 6) + (fg->oversample * fg->magnitude); 
	fg->rawGlyphSize.y = (slot->metrics.height >> 6) + (fg->oversample * fg->magnitude); 
	
	// the raw glyph is copied to the middle of a larger buffer to make the sdf algorithm simpler 
	fg->rawGlyph = calloc(1, sizeof(*fg->rawGlyph) * fg->rawGlyphSize.x * fg->rawGlyphSize.y);
	
	blit(
		0, 0, // src x and y offset for the image
		 (fg->oversample * fg->magnitude * .5), (fg->oversample * fg->magnitude * .5), // dst offset
		rawImgSz.x, rawImgSz.y, // width and height
		slot->bitmap.pitch, fg->rawGlyphSize.x, // src and dst row widths
		slot->bitmap.buffer, // source
		fg->rawGlyph); // destination
	
	/*
	/// TODO move to multithreaded pass
	CalcSDF_Software_(fg);
	
	// done with the raw data
	free(fg->rawGlyph);
	*/
	
	/*
	
	printf("raw size: %d, %d\n", fg->rawGlyphSize.x, fg->rawGlyphSize.y);
	printf("sdf size: %d, %d\n", fg->sdfGlyphSize.x, fg->sdfGlyphSize.y);
	printf("bounds: %f,%f, %f,%f\n",
		fg->sdfBounds.min.x,
		fg->sdfBounds.min.y,
		fg->sdfBounds.max.x,
		fg->sdfBounds.max.y
		   
	);

	writePNG("sdf-raw.png", 1, fg->rawGlyph, fg->rawGlyphSize.x, fg->rawGlyphSize.y);
	writePNG("sdf-pct.png", 1, fg->sdfGlyph, fg->sdfGlyphSize.x, fg->sdfGlyphSize.y);
	*/
	
	return fg;
}


void* sdf_thread(void* _fm) {
	FontManager* fm = (FontManager*)_fm;
	
	int i = 0;
	while(1) {
		i = atomic_fetch_add(&fm->genCounter, 1);
		
		if(i >= VEC_LEN(&fm->gen)) break;
		
		FontGen* fg = VEC_ITEM(&fm->gen, i);
		if(fm->verbose >= 0) printf("calc: '%s':%d:%d %c\n", fg->font->name, fg->bold, fg->italic, fg->code);
		CalcSDF_Software_(fg);
		
	}
	
	
	pthread_exit(NULL);
}

void FontManager_finalize(FontManager* fm) {
	
	int maxThreads = MIN(get_nprocs(), fm->maxThreads);
	pthread_t threads[maxThreads];
	
	for(int i = 0; i < maxThreads; i++) {
		int ret = pthread_create(&threads[i], NULL, sdf_thread, fm);
		if(ret) {
			printf("failed to spawn thread in FontManager\n");
			exit(1);
		}
	}
	
	// wait for the work to get done
	for(int i = 0; i < maxThreads; i++) {
		pthread_join(threads[i], NULL);
	}
}


// sorting function for fontgen array
static int gen_comp(const void* aa, const void * bb) {
	FontGen* a = *((FontGen**)aa);
	FontGen* b = *((FontGen**)bb);
	
	if(a->sdfDataSize.y == b->sdfDataSize.y) {
		return b->sdfDataSize.x - a->sdfDataSize.x;
	}
	else {
		return b->sdfDataSize.y - a->sdfDataSize.y;
	}
}



GUIFont* GUIFont_alloc(char* name) {
	GUIFont* f;
	
	f = calloc(1, sizeof(*f));
	
	f->name = strdup(name);
	f->charsLen = 128;
	f->regular = calloc(1, sizeof(*f->regular) * f->charsLen);
	f->bold = calloc(1, sizeof(*f->bold) * f->charsLen);
	f->italic= calloc(1, sizeof(*f->italic) * f->charsLen);
	f->boldItalic = calloc(1, sizeof(*f->boldItalic) * f->charsLen);
	
	return f;
}



void FontManager_addFont2(FontManager* fm, char* name, char* charset, int size, char bold, char italic) {
	GUIFont* f;
	FT_Error err;
	FT_Face fontFace;
	
	//defaultCharset = "I";
	
	int len = strlen(charset);
	
	int fontSize = size; // pixels
	
	checkFTlib();
	
	// TODO: load font
	char* fontPath = getFontFile2(name, bold, italic);
	if(!fontPath) {
		fprintf(stderr, "Could not load font '%s'\n", name);
		exit(1);
	}
	if(fm->verbose >= 0) printf("Font path: %s: %s\n", name, fontPath);

	err = FT_New_Face(ftLib, fontPath, 0, &fontFace);
	if(err) {
		fprintf(stderr, "Could not access font '%s' at '%s'.\n", name, fontPath);
		exit(1);
	}
	
	if(HT_get(&fm->fonts, name, &f)) {
		f = GUIFont_alloc(name);
		HT_set(&fm->fonts, name, f);
	}
	
	for(int i = 0; i < len; i++) {
// 		printf("calc: '%s':%d:%d %c\n", name, bold, italic, charset[i]);
		FontGen* fg = addChar(fm, &fontFace, charset[i], fontSize, bold, italic);
		fg->font = f;
		
		VEC_PUSH(&fm->gen, fg);
	}
	
	

}


void FontManager_createAtlas(FontManager* fm) {
	char buf[32];
	
	// order the characters by height then width, tallest and widest first.
	VEC_SORT(&fm->gen, gen_comp);
	
	int totalWidth = 0;
	VEC_EACH(&fm->gen, ind, gen) {
		//printf("%c: h: %d, w: %d \n", gen->code, gen->sdfDataSize.y, gen->sdfDataSize.x);
		totalWidth += gen->sdfDataSize.x;
	}
	
	int maxHeight = VEC_ITEM(&fm->gen, 0)->sdfDataSize.y;
	int naiveSize = ceil(sqrt(maxHeight * totalWidth));
	int pot = nextPOT(naiveSize);
	int pot2 = naiveSize / 2;
	
	//printf("naive min tex size: %d -> %d (%d)\n", naiveSize, pot, totalWidth);
	
	pot = MIN(pot, fm->maxAtlasSize);
	
	
	// test the packing
	int row = 0;
	int hext = maxHeight;
	int rowWidth = 0;
	
	// copy the chars into the atlas, cleaning as we go
	uint8_t* texData = malloc(sizeof(*texData) * pot * pot);
	memset(texData, 255, sizeof(*texData) * pot * pot);
	fm->atlasSize = pot;
	
	
	row = 0;
	hext = 0;
	int prevhext = maxHeight;
	rowWidth = 0;
	VEC_EACH(&fm->gen, ind, gen) {
		
		if(rowWidth + gen->sdfDataSize.x > pot) {
			row++;
			rowWidth = 0;
			hext += prevhext;
			prevhext = gen->sdfDataSize.y;
			
			// next texture
			if(hext + prevhext > pot) { 
				VEC_PUSH(&fm->atlas, texData);
				
				sprintf(buf, fm->pngFileFormat, (int)VEC_LEN(&fm->atlas));
				writePNG(buf, 1, texData, pot, pot);
				if(fm->verbose >= 0) printf("Saved atlas file '%s'\n", buf);
				
				texData = malloc(sizeof(*texData) * pot * pot);
				// make everything white, the "empty" value
				memset(texData, 255, sizeof(*texData) * pot * pot);
				
				hext = 0;
			}
		}
		
		// blit the sdf bitmap data
		blit(
			gen->sdfBounds.min.x, gen->sdfBounds.min.y, // src x and y offset for the image
			rowWidth, hext, // dst offset
			gen->sdfDataSize.x, gen->sdfDataSize.y, // width and height
			gen->sdfGlyphSize.x, pot, // src and dst row widths
			gen->sdfGlyph, // source
			texData); // destination
		
		
		// copy info over to font
		struct charInfo* c;
		if(gen->bold && gen->italic) {
			gen->font->hasBoldItalic |= 1;
			c = &gen->font->boldItalic[gen->code];
		}
		else if(gen->bold) {
			gen->font->hasBold |= 1;
			c = &gen->font->bold[gen->code];
		}
		else if(gen->italic) {
			gen->font->hasItalic |= 1;
			c = &gen->font->italic[gen->code];
		}
		else {
			gen->font->hasRegular |= 1;
			c = &gen->font->regular[gen->code];
		}
		
		c->code = gen->code;
		c->texIndex = VEC_LEN(&fm->atlas);
		c->texelOffset.x = rowWidth;
		c->texelOffset.y = hext;
		c->texelSize = gen->sdfDataSize;
		c->texNormOffset.x = (float)rowWidth / (float)pot;
		c->texNormOffset.y = (float)hext / (float)pot;
		c->texNormSize.x = (float)gen->sdfDataSize.x / (float)pot;
		c->texNormSize.y = (float)gen->sdfDataSize.y / (float)pot;
		
		// BUG: wrong? needs magnitude?
		c->advance = gen->rawAdvance / (float)gen->oversample;
		c->topLeftOffset.x = (gen->rawBearing.x / (float)gen->oversample);// + (float)gen->sdfBounds.min.x;
		c->topLeftOffset.y = (gen->rawBearing.y / (float)gen->oversample);// - (float)gen->sdfBounds.min.y;
		c->size.x = gen->sdfDataSize.x;
		c->size.y = gen->sdfDataSize.y;
		
// 		printf("toff: %f, %f \n", c->texNormOffset.x, c->texNormOffset.y);
//		printf("tsize: %f, %f \n", c->texNormSize.x, c->texNormSize.y);
//		printf("ltoff: %f, %f \n", c->topLeftOffset.x, c->topLeftOffset.y);
		
		// advance the write offset
		rowWidth += gen->sdfDataSize.x;
		
		// clean up the FontGen struct
		free(gen->sdfGlyph);
		free(gen);
	}
	
	
	VEC_PUSH(&fm->atlas, texData);
	
	sprintf(buf, fm->pngFileFormat, (int)VEC_LEN(&fm->atlas));
	writePNG(buf, 1, texData, pot, pot);
	if(fm->verbose >= 0) printf("Saved atlas file '%s'\n", buf);
	
	VEC_FREE(&fm->gen);
	
	
// 	writePNG("sdf-comp.png", 1, texData, pot, pot);

	
	//exit(1);
	
	
}


void printCharinfo(FILE* f, char* prefix, struct charInfo* ci) {
	if(ci->code == 0) return;
// 	fprintf(f, "%s%d: {\n", prefix, ci->code);
// 	fprintf(f, "%s\tcode: %d,\n", prefix, ci->code);
// 	fprintf(f, "%s\ttexIndex: %d,\n", prefix, ci->texIndex);
// 	fprintf(f, "%s\ttexelOffset: [%d, %d],\n", prefix, ci->texelOffset.x, ci->texelOffset.y);
// 	fprintf(f, "%s\ttexelSize: [%d, %d],\n", prefix, ci->texelSize.x, ci->texelSize.y);
// 	fprintf(f, "%s\tnormalizedOffset: [%f, %f],\n", prefix, ci->texNormOffset.x, ci->texNormOffset.y);
// 	fprintf(f, "%s\tnormalizedSize: [%f, %f],\n", prefix, ci->texNormSize.x, ci->texNormSize.y);
// 	fprintf(f, "%s\tadvance: %f,\n", prefix, ci->advance);
// 	fprintf(f, "%s\tboxOffset: [%f, %f],\n", prefix, ci->topLeftOffset.x, ci->topLeftOffset.y);
// 	fprintf(f, "%s\tboxSize: [%f, %f]\n", prefix, ci->size.x, ci->size.y);
// 	fprintf(f, "%s},\n", prefix);
	fprintf(f, "%s%d: [ ", prefix, ci->code);
	fprintf(f, "%d, ", ci->code);
	fprintf(f, "%d, ", ci->texIndex);
	fprintf(f, "[%d, %d], ", ci->texelOffset.x, ci->texelOffset.y);
	fprintf(f, "[%d, %d], ", ci->texelSize.x, ci->texelSize.y);
	fprintf(f, "[%f, %f], ", ci->texNormOffset.x, ci->texNormOffset.y);
	fprintf(f, "[%f, %f], ", ci->texNormSize.x, ci->texNormSize.y);
	fprintf(f, "%f, ", ci->advance);
	fprintf(f, "[%f, %f], ", ci->topLeftOffset.x, ci->topLeftOffset.y);
	fprintf(f, "[%f, %f] ", ci->size.x, ci->size.y);
	fprintf(f, "],\n", prefix);
}


void FontManager_saveJSON(FontManager* fm, char* path) {
	FILE* f;

	f = fopen(path, "w");
	if(!f) {
		fprintf(stderr, "Could not save JSON metadata to '%s'\n", path);
		exit(1);
	}
	
	fprintf(f, "{\n");
	
	fprintf(f, "\tlayers: [\n");
	VEC_LOOP(&fm->atlas, fi) {
		fprintf(f, "\t\t\"");
		fprintf(f, fm->pngFileFormat, (int)fi);
		fprintf(f, "\",\n");
	}
	fprintf(f, "\t],\n");
	
	fprintf(f, "\tcharInfoIndices: {,\n");
	fprintf(f, "\t\tcode: 0,\n");
	fprintf(f, "\t\ttexIndex: 1,\n");
	fprintf(f, "\t\ttexelOffset: 2,\n");
	fprintf(f, "\t\ttexelSize: 3,\n");
	fprintf(f, "\t\tnormalizedOffset: 4,\n");
	fprintf(f, "\t\tnormalizedSize: 5,\n");
	fprintf(f, "\t\tadvance: 6,\n");
	fprintf(f, "\t\tboxOffset: 7,\n");
	fprintf(f, "\t\tboxSize: 8,\n");
	
	fprintf(f, "\t},\n");
	
	fprintf(f, "\tfonts: {\n");
	HT_LOOP(&fm->fonts, name, GUIFont*, font) {
		fprintf(f, "\t\t\"%s\": {\n", font->name);
		
		fprintf(f, "\t\t\tname: \"%s\",\n", font->name);
		fprintf(f, "\t\t\tsize: %d,\n", font->size);
		
		if(font->hasRegular) {
			fprintf(f, "\t\t\tregular: {\n");
			for(int i = 0; i < font->charsLen; i++) {
				printCharinfo(f, "\t\t\t\t", font->regular + i);
			}
			fprintf(f, "\t\t\t},\n");
		}
		if(font->hasBold) {
			fprintf(f, "\t\t\tbold: {\n");
			for(int i = 0; i < font->charsLen; i++) {
				printCharinfo(f, "\t\t\t\t", &font->bold[i]);
			}
			fprintf(f, "\t\t\t},\n");
		}
		if(font->hasItalic) {
			fprintf(f, "\t\t\titalic: {\n");
			for(int i = 0; i < font->charsLen; i++) {
				printCharinfo(f, "\t\t\t\t", &font->italic[i]);
			}
			fprintf(f, "\t\t\t},\n");
		}
		if(font->hasBoldItalic) {
			fprintf(f, "\t\t\tboldItalic: {\n");
			for(int i = 0; i < font->charsLen; i++) {
				printCharinfo(f, "\t\t\t\t", &font->boldItalic[i]);
			}
			fprintf(f, "\t\t\t},\n");
		}
		
		fprintf(f, "\t\t},\n");
	}
	fprintf(f, "\t},\n");
	
	fprintf(f, "}");
	
	
	fclose(f);
	
	if(fm->verbose >= 0) printf("Saved config file '%s'\n", path);
}





