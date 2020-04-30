#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <VapourSynth.h>
#include <VSHelper.h>


template <typename PixelType>
static void asharp_run_c(const uint8_t *srcp8, uint8_t *dstp8, int pitch,
                         int height, int width,
                         int T, int D, int B, int B2, bool bf,
                         int bits) {

    const PixelType *srcp = (const PixelType *)srcp8;
    PixelType *dstp = (PixelType *)dstp8;
    pitch /= sizeof(PixelType);

    int pixel_max = (1 << bits) - 1;

    memcpy(dstp, srcp, width * sizeof(PixelType));

    srcp += pitch;
    dstp += pitch;

    for (int y = 1; y < height - 1; y++) {
        dstp[0] = srcp[0];

        for (int x = 1; x < width - 1; x++) {

            int avg = 0;
            int dev = 0;

            avg += srcp[x - pitch - 1];
            avg += srcp[x - pitch];
            avg += srcp[x - pitch + 1];
            avg += srcp[x - 1];
            avg += srcp[x];
            avg += srcp[x + 1];
            avg += srcp[x + pitch - 1];
            avg += srcp[x + pitch];
            avg += srcp[x + pitch + 1];

            avg /= 9;

#define CHECK(A)                \
    if (abs(A - srcp[x]) > dev) \
        dev = abs(A - srcp[x]);

            if (bf) {

                if (y % 8 > 0) {
                    if (x % 8 > 0)
                        CHECK(srcp[x - pitch - 1])
                    CHECK(srcp[x - pitch])
                    if (x % 8 < 7)
                        CHECK(srcp[x - pitch + 1])
                }
                if (x % 8 > 0)
                    CHECK(srcp[x - 1])
                if (x % 8 < 7)
                    CHECK(srcp[x + 1])
                if (y % 8 < 7) {
                    if (x % 8 > 0)
                        CHECK(srcp[x + pitch - 1])
                    CHECK(srcp[x + pitch])
                    if (x % 8 < 7)
                        CHECK(srcp[x + pitch + 1])
                }

            } else {
                CHECK(srcp[x - pitch - 1])
                CHECK(srcp[x - pitch])
                CHECK(srcp[x - pitch + 1])
                CHECK(srcp[x - 1])
                CHECK(srcp[x + 1])
                CHECK(srcp[x + pitch - 1])
                CHECK(srcp[x + pitch])
                CHECK(srcp[x + pitch + 1])
            }
#undef CHECK

            int T2 = T;
            int64_t diff = srcp[x] - avg;
            int64_t D2 = D;

            if (B2 != 256 && B != 256) {
                if (x % 8 == 6)
                    D2 = (D2 * B2) >> 8;
                if (x % 8 == 7)
                    D2 = (D2 * B) >> 8;
                if (x % 8 == 0)
                    D2 = (D2 * B) >> 8;
                if (x % 8 == 1)
                    D2 = (D2 * B2) >> 8;
                
                if (y % 8 == 6)
                    D2 = (D2 * B2) >> 8;
                if (y % 8 == 7)
                    D2 = (D2 * B) >> 8;
                if (y % 8 == 0)
                    D2 = (D2 * B) >> 8;
                if (y % 8 == 1)
                    D2 = (D2 * B2) >> 8;
            }
            
            int Da = -32 + (D >> 7);
            if (D > 0)
                T2 = ((((dev << 7) * D2) >> (16 + (bits - 8))) + Da) * 16;

            if (T2 > T)
                T2 = T;
            if (T2 < -32)
                T2 = -32;

            int tmp = (((diff * 128) * T2) >> 16) + srcp[x];

            if (tmp < 0)
                tmp = 0;
            if (tmp > pixel_max)
                tmp = pixel_max;

            dstp[x] = tmp;
        }

        dstp[width - 1] = srcp[width - 1];

        srcp += pitch;
        dstp += pitch;
    }

    memcpy(dstp, srcp, width * sizeof(PixelType));
}


typedef struct ASharpData {
    VSNodeRef *clip;
    const VSVideoInfo *vi;

    int t;
    int d;
    int b;
    int b2;
    bool bf;
} ASharpData;


static void VS_CC asharpInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    (void)in;
    (void)out;
    (void)core;

    ASharpData *d = (ASharpData *) *instanceData;

    vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC asharpGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    const ASharpData *d = (const ASharpData *) *instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->clip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->clip, frameCtx);

        const VSFormat *format = vsapi->getFrameFormat(src);
        int width = vsapi->getFrameWidth(src, 0);
        int height = vsapi->getFrameHeight(src, 0);

        const VSFrameRef *plane_src[3] = { nullptr, src, src };
        int planes[3] = { 0, 1, 2 };

        VSFrameRef *dst = vsapi->newVideoFrame2(format, width, height, plane_src, planes, src, core);

        const uint8_t *srcp = vsapi->getReadPtr(src, 0);
        uint8_t *dstp = vsapi->getWritePtr(dst, 0);
        int stride = vsapi->getStride(dst, 0);

        (format->bitsPerSample == 8 ? asharp_run_c<uint8_t>
                                    : asharp_run_c<uint16_t>)
                (srcp, dstp, stride, height, width, d->t, d->d, d->b, d->b2, d->bf, format->bitsPerSample);

        vsapi->freeFrame(src);

        return dst;
    }

    return nullptr;
}


static void VS_CC asharpFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    ASharpData *d = (ASharpData *)instanceData;

    vsapi->freeNode(d->clip);
    free(d);
}


static void VS_CC asharpCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    (void)userData;

    ASharpData d;
    memset(&d, 0, sizeof(d));

    int err;

    double t = vsapi->propGetFloat(in, "t", 0, &err);
    if (err)
        t = 2;

    double dd = vsapi->propGetFloat(in, "d", 0, &err);
    if (err)
        dd = 4;

    double b = vsapi->propGetFloat(in, "b", 0, &err);
    if (err)
        b = -1;

    d.bf = !!vsapi->propGetInt(in, "hqbf", 0, &err);
    if (err)
        d.bf = false;


    d.t = (int)(t * (4 << 7));
    d.d = (int)(dd * (4 << 7));
    d.b = (int)(256 - b * 64);
    d.b2 = (int)(256 - b * 48);


    d.t = std::max(-(4 << 7), std::min(d.t, 32 * (4 << 7)));
    d.d = std::max(0, std::min(d.d, 16 * (4 << 7)));
    d.b = std::max(0, std::min(d.b, 256));
    d.b2 = std::max(0, std::min(d.b2, 256));


    d.clip = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.clip);


    if (!d.vi->format) {
        vsapi->setError(out, "ASharp: clips with variable format are not supported.");
        vsapi->freeNode(d.clip);
        return;
    }

    if (d.vi->format->colorFamily == cmRGB) {
        vsapi->setError(out, "ASharp: RGB clips are not supported.");
        vsapi->freeNode(d.clip);
        return;
    }

    if (d.vi->format->sampleType == stFloat) {
        vsapi->setError(out, "ASharp: float clips are not supported.");
        vsapi->freeNode(d.clip);
        return;
    }


    ASharpData *data = (ASharpData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "ASharp", asharpInit, asharpGetFrame, asharpFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.asharp", "asharp", "adaptive sharpening filter", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("ASharp",
                 "clip:clip;"
                 "t:float:opt;"
                 "d:float:opt;"
                 "b:float:opt;"
                 "hqbf:int:opt;"
                 , asharpCreate, nullptr, plugin);
}
