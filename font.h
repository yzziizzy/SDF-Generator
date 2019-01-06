#ifndef __EACSMB_font_h__
#define __EACSMB_font_h__

#include <stdatomic.h>

#include "c3dlas.h"

#include "ds.h"
#include "hash.h"


#include <ft2build.h>
#include FT_FREETYPE_H




struct charInfo {
	uint32_t code;
	
	// final output texture coordinates
	int texIndex;
	Vector2i texelOffset; // from the top left
	Vector2i texelSize; // size of the character data in texels
	Vector2 texNormOffset; // normalized texture coordinates
	Vector2 texNormSize; 
	
	// typographic info
	float advance; // horizonatal distance to advance after this char
	Vector2 topLeftOffset; // offset from the baseline to the top left vertex of the quad
	Vector2 size;
	
};



typedef struct GUIFont {
	
	char* name;
	
	char hasRegular;
	char hasItalic;
	char hasBold;
	char hasBoldItalic;
	
	int charsLen;
	struct charInfo* regular;
	struct charInfo* italic;
	struct charInfo* bold;
	struct charInfo* boldItalic;
	
	int size;
	
	// TODO: kerning info
	
} GUIFont;


// generation info for a single character
typedef struct FontGen {
	GUIFont* font;
	
	uint32_t code;
	char italic;
	char bold;
	
	
	// the raw glyph is oversampled by FontManager.oversample times
	int oversample;
	int magnitude;
	
	// metrics for the raw glyph, in pixels
	uint8_t* rawGlyph;
	Vector2i rawGlyphSize; // size of the raw bitmap

	Vector rawBearing; // distance from the origin to the top left corner of the glyph
	float rawAdvance; // horizontal advance, in pixels
	
	// the sdf is smaller than the raw glyph
	
	// metrics for the sdf glyph, in pixels
	uint8_t* sdfGlyph;
	Vector2i sdfGlyphSize; // size of the sdf bitmap
	AABB2 sdfBounds; // bounding box of the non-empty data in the sdf bitmap, in pixels
	Vector2i sdfDataSize; // size of the non-empty sdf data in the bitmap
	
	Vector sdfBearing; // distance from the origin to the top left corner of the clipped sdf data
	float sdfAdvance; // horizontal advance, in pixels
	
	
	// final texture data
	struct charInfo charinfo;
} FontGen;


typedef struct FontManager {
	VEC(GUIFont*) fonts;
	
	FT_Face fallback;
	
	// SDF generation 
	VEC(FontGen*) gen;
	atomic_int genCounter;
	
	// SDF config
	int oversample;
	int magnitude;
	
	int maxAtlasSize;
	VEC(uint8_t*) atlas;
	uint32_t atlasSize;
	
	char* pngFileFormat;
	
	int maxThreads;
	int verbose;
	
} FontManager;


void FontManager_createAtlas(FontManager* fm);
void FontManager_saveAtlas(FontManager* fm, char* path);
int FontManager_loadAtlas(FontManager* fm, char* path);
void FontManager_addFont2(FontManager* fm, char* name, uint32_t* charset, int size, char bold, char italic);
void FontManager_finalize(FontManager* fm);

void FontManager_saveJSON(FontManager* fm, char* path);

GUIFont* FontManager_findFont(FontManager* fm, char* name);

FontManager* FontManager_alloc();
void FontManager_init(FontManager* fm);




static size_t u32strlen(const uint32_t* const s) {
	size_t l = 0;
	while(s[l] != 0) l++;
	return l;
}



#endif // __EACSMB_font_h__
