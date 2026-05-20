#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/Picasso96.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/Picasso96.h>
#include <stdlib.h>

struct Library *P96Base;

int main(int argc, char **argv)
{
    ULONG display_id = 0x50031000;
    ULONG width = 640;
    ULONG height = 480;
    ULONG depth = 16;
    RGBFTYPE rgb_format = RGBFB_R5G6B5PC;

    if (argc > 1) {
        display_id = strtoul(argv[1], 0, 0);
    }
    if (argc > 2) {
        width = strtoul(argv[2], 0, 0);
    }
    if (argc > 3) {
        height = strtoul(argv[3], 0, 0);
    }
    if (argc > 4) {
        depth = strtoul(argv[4], 0, 0);
    }

    switch (depth) {
    case 15:
        rgb_format = RGBFB_R5G5B5PC;
        break;
    case 16:
        rgb_format = RGBFB_R5G6B5PC;
        break;
    case 32:
        rgb_format = RGBFB_B8G8R8A8;
        break;
    default:
        rgb_format = RGBFB_CLUT;
        break;
    }

    P96Base = OpenLibrary(P96NAME, 0);
    if (!P96Base) {
        Printf("OPENSCREEN FAILED no %s\n", (ULONG)P96NAME);
        return 5;
    }

    ULONG best_id = p96BestModeIDTags(
        P96BIDTAG_NominalWidth, width,
        P96BIDTAG_NominalHeight, height,
        P96BIDTAG_Depth, depth,
        P96BIDTAG_FormatsAllowed, 1UL << rgb_format,
        TAG_DONE);
    if (best_id != ~0UL) {
        display_id = best_id;
    }

    struct Screen *screen = p96OpenScreenTags(
        P96SA_DisplayID, display_id,
        P96SA_Width, width,
        P96SA_Height, height,
        P96SA_Depth, depth,
        P96SA_RGBFormat, rgb_format,
        P96SA_Title, (ULONG)"Unix P96 direct-color smoke",
        TAG_DONE);
    if (!screen) {
        Printf("OPENSCREEN FAILED %08lx %ldx%ldx%ld\n", display_id, width, height, depth);
        CloseLibrary(P96Base);
        return 5;
    }

    Printf("OPENSCREEN OK %08lx %ldx%ldx%ld\n", display_id, width, height, depth);
    Delay(100);
    p96CloseScreen(screen);
    CloseLibrary(P96Base);
    return 0;
}
