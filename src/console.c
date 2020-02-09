#include "console.h"
#include "data.h"
#include "log.h"

#include "stb/stb_rectpack.h"
#include "stb/stb_truetype.h"

struct console_ {
    int i;
};

console_t* console_new() {
    stbtt_pack_context spc;

    uint8_t* pixels = malloc(500 * 500);
    if (!stbtt_PackBegin(&spc, pixels, 500, 500, 0, 1, NULL)) {
        log_error_and_abort("stbtt_PackBegin failed");
    }
    stbtt_PackSetOversampling(&spc, 2, 2);

    size_t num_chars = '~' - ' ';
    stbtt_packedchar* chardata = malloc(sizeof(stbtt_packedchar) * num_chars);

    if (!stbtt_PackFontRange(&spc, FONT, 0, 16.0f, ' ',
                             num_chars, chardata))
    {
        log_error_and_abort("stbtt_PackFontRange failed");
    }
    stbtt_PackEnd(&spc);

    for (unsigned y=0; y < 500; ++y) {
        for (unsigned x=0; x < 500; ++x) {
            printf("%c", pixels[x + y*500] ? 'X' : '.');
        }
        printf("\n");
    }

    float xpos = 10.0f;
    float ypos = 0.0f;
    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(chardata, 500, 500, 'q' - ' ', &xpos, &ypos, &q, 0);

    log_trace("%f %f, %f %f\t%f %f, %f %f",
            q.x0, q.y0, q.s0, q.t0,
            q.x1, q.y1, q.s1, q.t1);

    return NULL;
}
