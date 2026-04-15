/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "vf_amf_common.h"

#include "libavutil/avassert.h"
#include "avfilter.h"
#include "avfilter_internal.h"
#include "formats.h"
#include "libavutil/mem.h"
#include "libavutil/imgutils.h"

#include "AMF/components/VideoDecoderUVD.h"
#include "libavutil/hwcontext_amf.h"
#include "libavutil/hwcontext_amf_internal.h"
#include "scale_eval.h"

#if CONFIG_DXVA2
#include <d3d9.h>
#endif

#if CONFIG_D3D11VA
#include <d3d11.h>
#endif

#if VF_VPP_AMF_D3D11_HWACCEL
static int amf_d3d11_preprocess_init(AMFFilterContext *ctx,
                                     AMFSurface *surface)
{
    HRESULT hr;
    AMFPlane *plane = NULL;
    ID3D11Texture2D *tex = NULL;
    ID3D11Device *device = NULL;

    if (ctx->d3d11_video_context)
        return 0;

    /* Get DX11 texture from AMF surface */
    plane = surface->pVtbl->GetPlaneAt(surface, 0);
    if (!plane) {
        av_log(ctx, AV_LOG_ERROR, "init : failed : GetPlaneAt(surface, 0)\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO,"init : pass : GetPlaneAt(surface, 0)\n");

    tex = (ID3D11Texture2D *)plane->pVtbl->GetNative(plane);
    if (!tex) {
        av_log(ctx, AV_LOG_ERROR, "init : failed : GetNative(plane)\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO,"init : pass : GetNative(plane)\n");

    /* Get device from texture */
    tex->lpVtbl->GetDevice(tex, &device);
    if (!device) {
        av_log(ctx, AV_LOG_ERROR, "init : failed : GetDevice(device)\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO,"init : pass : GetDevice(device)\n");

    /* Store device */
    /* refcount incremented by GetDevice */
    ctx->d3d11_device = device;

    /* Get immediate context */
    device->lpVtbl->GetImmediateContext(device, &ctx->d3d11_context);
    if (!ctx->d3d11_context) {
        av_log(ctx, AV_LOG_ERROR, "init : failed : GetImmediateContext(context)\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO,"init : pass : GetImmediateContext(context)\n");

    /* Query video interfaces */
    hr = device->lpVtbl->QueryInterface(
        device,
        &IID_ID3D11VideoDevice,
        (void **)&ctx->d3d11_video_device);
    if (FAILED(hr)) {
        av_log(ctx, AV_LOG_ERROR, "init : failed : QueryInterface(video_device)\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO,"init : pass : QueryInterface(video_device)\n");

    hr = ctx->d3d11_context->lpVtbl->QueryInterface(
        ctx->d3d11_context,
        &IID_ID3D11VideoContext,
        (void **)&ctx->d3d11_video_context);
    if (FAILED(hr)) {
        av_log(ctx, AV_LOG_ERROR, "init : failed : QueryInterface(video_context)\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO,"init : pass : QueryInterface(video_context)\n");

    av_log(ctx, AV_LOG_INFO,"init : clear\n");

    return 0;
}

static void amf_d3d11_preprocess_uninit(AMFFilterContext *ctx)
{
    if (ctx->deint_frame_last) {
        av_frame_free(&(ctx->deint_frame_last));
        ctx->deint_frame_last = NULL;
    }

    if (ctx->d3d11_out_view) {
        ctx->d3d11_out_view->lpVtbl->Release(ctx->d3d11_out_view);
        ctx->d3d11_out_view = NULL;
    }
    if (ctx->d3d11_tex_out) {
        ctx->d3d11_tex_out->lpVtbl->Release(ctx->d3d11_tex_out);
        ctx->d3d11_tex_out = NULL;
    }
    for (int i=0; i<ctx->deint_frames_max; i++) {
        if (ctx->d3d11_in_view[i]) {
            ctx->d3d11_in_view[i]->lpVtbl->Release(ctx->d3d11_in_view[i]);
            ctx->d3d11_in_view[i] = NULL;
        }
        if (ctx->d3d11_tex_in[i]) {
            ctx->d3d11_tex_in[i]->lpVtbl->Release(ctx->d3d11_tex_in[i]);
            ctx->d3d11_tex_in[i] = NULL;
        }
    }
    if (ctx->d3d11_vp) {
        ctx->d3d11_vp->lpVtbl->Release(ctx->d3d11_vp);
        ctx->d3d11_vp = NULL;
    }
    if (ctx->d3d11_vp_enum) {
        ctx->d3d11_vp_enum->lpVtbl->Release(ctx->d3d11_vp_enum);
        ctx->d3d11_vp_enum = NULL;
    }
    if (ctx->d3d11_video_context) {
        ctx->d3d11_video_context->lpVtbl->Release(ctx->d3d11_video_context);
        ctx->d3d11_video_context = NULL;
    }
    if (ctx->d3d11_video_device) {
        ctx->d3d11_video_device->lpVtbl->Release(ctx->d3d11_video_device);
        ctx->d3d11_video_device = NULL;
    }
    if (ctx->d3d11_context) {
        ctx->d3d11_context->lpVtbl->Release(ctx->d3d11_context);
        ctx->d3d11_context = NULL;
    }
    if (ctx->d3d11_device) {
        ctx->d3d11_device->lpVtbl->Release(ctx->d3d11_device);
        ctx->d3d11_device = NULL;
    }
}

static int amf_d3d11_preprocess_conv_memtype(AMFFilterContext *ctx,
                                                 AMFSurface *surface)
{
    int av_log_level = AV_LOG_VERBOSE;

    if (!ctx->d3d11_log_info)
        av_log_level = AV_LOG_INFO;

    /*
     * Copy amf surface to Convertprovided texture to input texture
     */
    if (surface->pVtbl->GetMemoryType(surface) != AMF_MEMORY_DX11) {
        av_log(ctx, av_log_level,
            "conv_memtype : warn : GetMemoryType != AMF_MEMORY_DX11\n");
        if (surface->pVtbl->Convert(surface, AMF_MEMORY_DX11) != AMF_OK) {
            av_log(ctx, AV_LOG_ERROR,
               "conv_memtype : failed : Convert(surface, AMF_MEMORY_DX11)\n");
            return AVERROR_EXTERNAL;
        }
        av_log(ctx, av_log_level,
            "conv_memtype : pass : Convert(surface, AMF_MEMORY_DX11)\n");
    }
    av_log(ctx, av_log_level,
        "conv_memtype : pass : GetMemoryType(surface) == AMF_MEMORY_DX11\n");

    av_log(ctx, av_log_level, "conv_memtype : clear\n");

    return 0;
}

static int amf_d3d11_preprocess_calc_sizes(AVFilterContext *avctx)
{
    AVFilterLink *inlink = avctx->inputs[0];
    AMFFilterContext  *ctx = avctx->priv;

    if (ctx->target_w || ctx->target_h)
        return 0;

    /*
     * Get input x, y
     */
    int in_w = inlink->w;
    int in_h = inlink->h;
    av_log(ctx, AV_LOG_INFO, "calc_sizes : pass : iw:[%d] ih:[%d]\n", in_w, in_h);
    av_log(ctx, AV_LOG_INFO, "calc_sizes : pass : ow:[%d] oh:[%d]\n", ctx->width, ctx->height);

    /*
     * log comment only
     */
    av_log(ctx, AV_LOG_INFO,
        "calc_sizes : chck : cx:[%d] cy:[%d] cw:[%d] ch:[%d]\n", 
        ctx->crop_x, ctx->crop_y, ctx->crop_w, ctx->crop_h);

    /*
     * validate crop rectangle coordinates and dimensions.
     */
    if (ctx->crop_x < 0 || ctx->crop_y < 0 || ctx->crop_w < 0 || ctx->crop_h < 0) {
        av_log(ctx, AV_LOG_ERROR,
            "calc_sizes : failed : negative(-) values input.\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO, "calc_sizes : pass : crop rectangle are valid.\n");

    /*
     * ensure crop_x and crop_y are within in_w and in_h.
     */
    if (ctx->crop_x >= in_w || ctx->crop_y >= in_h) {
        av_log(ctx, AV_LOG_ERROR,
               "calc_sizes : failed : cx or cy exceeds the image size.\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO, "calc_sizes : pass : cx and cy values are valid.\n");

    /*
     * adjust crop_w and crop_h
     */
    if (!ctx->crop_w) ctx->crop_w_eff = in_w;
        else          ctx->crop_w_eff = ctx->crop_w;

    if (!ctx->crop_h) ctx->crop_h_eff = in_h;
        else          ctx->crop_h_eff = ctx->crop_h;

    ctx->crop_w_eff = FFMIN(ctx->crop_w_eff, in_w - ctx->crop_x);
    ctx->crop_h_eff = FFMIN(ctx->crop_h_eff, in_h - ctx->crop_y);
    av_log(ctx, AV_LOG_INFO, "calc_sizes : pass : cw_eff:[%d] ch_eff:[%d]\n",
        ctx->crop_w_eff, ctx->crop_h_eff);

    /*
     * set input sar
     */
    AVRational av_sar = inlink->sample_aspect_ratio;
    if (!av_sar.num || !av_sar.den) {
        av_sar = (AVRational){1, 1};
        ctx->dxgi_in_sar.Numerator = 1;
        ctx->dxgi_in_sar.Denominator = 1;
        av_log(ctx, AV_LOG_WARNING,
            "calc_sizes : warn : Fallback to SAR [1:1] due to invalid input SAR [%d:%d].\n", 
            av_sar.num, av_sar.den);
    } else {
        ctx->dxgi_in_sar.Numerator = (unsigned int)av_sar.num;
        ctx->dxgi_in_sar.Denominator = (unsigned int)av_sar.den;
    }
    av_log(ctx, AV_LOG_INFO,
        "calc_sizes : pass : SAR in [%d:%d]\n",
        ctx->dxgi_in_sar.Numerator, ctx->dxgi_in_sar.Denominator);

    /*
     * calc input dar
     */
    AVRational av_dar = av_mul_q((AVRational){inlink->w, inlink->h}, av_sar);
    av_log(ctx, AV_LOG_INFO,
        "calc_sizes : pass : DAR in [%d:%d]\n", av_dar.num, av_dar.den);

    /*
     * Calculate Aspect Ratio (DAR) of the cropped image.
     */
    double in_dar   = (double)ctx->crop_w_eff / ctx->crop_h_eff;

    /*
     * Calculate target canvas Aspect Ratio to match the input Source Aspect Ratio (SAR).
     */
    double target_dar = (double)ctx->width / ctx->height;
    if (ctx->reset_sar)
        target_dar = target_dar / av_sar.num * av_sar.den;

    /* 
     * Calculate minimum base canvas size to fit the input image without scaling.
     * The canvas aspect ratio is fixed to target_dar.
     */
    if (ctx->scale_adjust == VPP_AMF_SCALE_ADJUST_STRETCH || fabs(in_dar - target_dar) < 1e-6) {
        /* If DARs match, use the cropped size as-is. */
        ctx->target_w = ctx->crop_w_eff;
        ctx->target_h = ctx->crop_h_eff;
    } else if (in_dar > target_dar) {
        /* Input is wider: Set canvas width to crop width and extend canvas height. */
        ctx->target_w = ctx->crop_w_eff;
        ctx->target_h = (int)((double)ctx->crop_w_eff / target_dar + 0.5);
    } else {
        /* Input is taller: Set canvas height to crop height and extend canvas width. */
        ctx->target_h = ctx->crop_h_eff;
        ctx->target_w = (int)((double)ctx->crop_h_eff * target_dar + 0.5);
    }

    /*
     * Ensure dimensions are even to satisfy hardware/format constraints (e.g., NV12).
     */
    if (ctx->target_w % 2) ctx->target_w ++;
    if (ctx->target_h % 2) ctx->target_h ++;
    av_log(ctx, AV_LOG_INFO,
        "calc_sizes : pass : tw:[%d] th:[%d]\n", 
        ctx->target_w, ctx->target_h);

    /*
     * Calculate coordinates to center the image within the base canvas.
     */
    ctx->dest_x = (ctx->target_w - ctx->crop_w_eff) / 2;
    ctx->dest_y = (ctx->target_h - ctx->crop_h_eff) / 2;
    av_log(ctx, AV_LOG_INFO,
        "calc_sizes : pass : dx:[%d] dy:[%d]\n", 
        ctx->dest_x, ctx->dest_y);

    av_log(ctx, AV_LOG_INFO, "calc_sizes : clear\n");

    return 0;
}

static int amf_d3d11_preprocess_init_vp_enum(AMFFilterContext *ctx)
{
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc = {0};
    HRESULT hr;

    if (ctx->d3d11_vp_enum)
        return 0;

    memset(&vp_desc, 0, sizeof(vp_desc));

    /*
     * set input frame format
     */
    if (!ctx->d3d11_deint) {
        vp_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    } else if (ctx->d3d11_deint_frame_format == 2) {
        vp_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
    } else {
        vp_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
    }

    /*
     * Attempting high-quality scaling by setting the High Quality Usage option
     * when scale_type=bicubic is requested, though the actual implementation is driver-dependent.
     */
    if (ctx->scale_type)
        vp_desc.Usage = D3D11_VIDEO_USAGE_OPTIMAL_QUALITY;
    else
        vp_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    /*
     * 
     */
    vp_desc.InputWidth  = ctx->crop_w_eff;
    vp_desc.InputHeight = ctx->crop_h_eff;

    vp_desc.OutputWidth  = ctx->target_w;
    vp_desc.OutputHeight = ctx->target_h;

    hr = ctx->d3d11_video_device->lpVtbl->CreateVideoProcessorEnumerator(
        ctx->d3d11_video_device,
        &vp_desc,
        &ctx->d3d11_vp_enum);

    if (FAILED(hr)) {
        av_log(ctx, AV_LOG_ERROR,
               "init_vp_enum : failed : CreateVideoProcessorEnumerator\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "init_vp_enum : pass : CreateVideoProcessorEnumerator\n");


    D3D11_VIDEO_PROCESSOR_CAPS caps;
    hr = ctx->d3d11_vp_enum->lpVtbl->GetVideoProcessorCaps(ctx->d3d11_vp_enum, &caps);
    if (FAILED(hr)) {
        av_log(ctx, AV_LOG_ERROR,
               "init_vp_enum : failed : GetVideoProcessorCaps\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "init_vp_enum : pass : GetVideoProcessorCaps\n");

    // 1. Log basic capabilities and input format support
    av_log(ctx, AV_LOG_INFO, "Direct3D 11 Video Processor Capabilities:\n");
    av_log(ctx, AV_LOG_INFO, "  Max Input Streams           : %u\n", caps.MaxInputStreams);

    // Check device capabilities (e.g., color space conversion, alpha blending)
    av_log(ctx, AV_LOG_INFO, "  Device Caps\n");
    av_log(ctx, AV_LOG_INFO, "    linear space              : %s\n",
        caps.DeviceCaps & D3D11_VIDEO_PROCESSOR_DEVICE_CAPS_LINEAR_SPACE ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    xvYCC color gamut         : %s\n",
        caps.DeviceCaps & D3D11_VIDEO_PROCESSOR_DEVICE_CAPS_xvYCC ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    RGB range conversion      : %s\n",
        caps.DeviceCaps & D3D11_VIDEO_PROCESSOR_DEVICE_CAPS_RGB_RANGE_CONVERSION ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    YCbCr matrix conversion   : %s\n",
        caps.DeviceCaps & D3D11_VIDEO_PROCESSOR_DEVICE_CAPS_YCbCr_MATRIX_CONVERSION ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    nominal range             : %s\n",
        caps.DeviceCaps & D3D11_VIDEO_PROCESSOR_DEVICE_CAPS_NOMINAL_RANGE ? "Enable" : "Disable");

    // Check video filter support (Useful for pre-processing)
    av_log(ctx, AV_LOG_INFO, "  Filter Caps\n");
    av_log(ctx, AV_LOG_INFO, "    Brightness                : %s\n",
        caps.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_CAPS_BRIGHTNESS ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    Contrast                  : %s\n",
        caps.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_CAPS_CONTRAST ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    Hue                       : %s\n",
        caps.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_CAPS_HUE ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    Saturation                : %s\n",
        caps.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_CAPS_SATURATION ? "Enable" : "Disable");

    // Check input format support for interlaced content
    av_log(ctx, AV_LOG_INFO, "  Input Format\n");
    av_log(ctx, AV_LOG_INFO, "    RGB Interlaced            : %s\n",
        caps.InputFormatCaps & D3D11_VIDEO_PROCESSOR_FORMAT_CAPS_RGB_INTERLACED ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    RGB Procamp               : %s\n",
        caps.InputFormatCaps & D3D11_VIDEO_PROCESSOR_FORMAT_CAPS_RGB_PROCAMP ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    RGB Luma Key              : %s\n",
        caps.InputFormatCaps & D3D11_VIDEO_PROCESSOR_FORMAT_CAPS_RGB_LUMA_KEY ? "Enable" : "Disable");
    av_log(ctx, AV_LOG_INFO, "    Palette Interlaced        : %s\n",
        caps.InputFormatCaps & D3D11_VIDEO_PROCESSOR_FORMAT_CAPS_PALETTE_INTERLACED ? "Enable" : "Disable");

    // Verify if specific YUV format (NV12) is supported for video processing
    UINT supportFlags = 0;
    av_log(ctx, AV_LOG_INFO, "  Input DXGI Format\n");
    hr = ctx->d3d11_vp_enum->lpVtbl->CheckVideoProcessorFormat(ctx->d3d11_vp_enum, DXGI_FORMAT_NV12, &supportFlags);
    if (SUCCEEDED(hr)) {
        av_log(ctx, AV_LOG_INFO, "    NV12                      : %s\n",
           supportFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT ? "Enable" : "Disable");
    }
    hr = ctx->d3d11_vp_enum->lpVtbl->CheckVideoProcessorFormat(ctx->d3d11_vp_enum, DXGI_FORMAT_P010, &supportFlags);
    if (SUCCEEDED(hr)) {
        av_log(ctx, AV_LOG_INFO, "    P010                      : %s\n",
           supportFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT ? "Enable" : "Disable");
    }

    // 2. Log details for each rate conversion (deinterlacing) capability
    for (UINT i = 0; i < caps.RateConversionCapsCount; i++) {
        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS convCaps;
        hr = ctx->d3d11_vp_enum->lpVtbl->GetVideoProcessorRateConversionCaps(ctx->d3d11_vp_enum, i, &convCaps);
    
        if (SUCCEEDED(hr)) {
            av_log(ctx, AV_LOG_INFO, "  Rate Conversion Caps [%u]\n", i);
        
            // Check deinterlacing methods
            av_log(ctx, AV_LOG_INFO, "    Deinterlace\n");
            av_log(ctx, AV_LOG_INFO, "      High Quality            : %s\n",
                convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE ? "Enable" : "Disable");
            av_log(ctx, AV_LOG_INFO, "      Bob                     : %s\n",
                convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB ? "Enable" : "Disable");
            av_log(ctx, AV_LOG_INFO, "      Blend                   : %s\n",
                convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND ? "Enable" : "Disable");
            av_log(ctx, AV_LOG_INFO, "      Motion Compensation     : %s\n",
                convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION ? "Enable" : "Disable");
            av_log(ctx, AV_LOG_INFO, "      Inverse Telecine (IVTC) : %s\n",
                convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE ? "Enable" : "Disable");

            // Log required reference frames for optimal deinterlacing
            // Member names are PastFrames and FutureFrames in d3d11.h
            av_log(ctx, AV_LOG_INFO, "    Frames Required\n");
            av_log(ctx, AV_LOG_INFO, "      Past                    : %u\n", convCaps.PastFrames);
            av_log(ctx, AV_LOG_INFO, "      Future                  : %u\n", convCaps.FutureFrames);

            ctx->deint_frames_min = convCaps.PastFrames + convCaps.FutureFrames + 1;
            ctx->deint_frames_max = VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX;
            ctx->deint_frames_cur = 0;
            ctx->deint_frames_new = 0;
            ctx->deint_frames_que = 0;
        }
    }

    av_log(ctx, AV_LOG_INFO, "init_vp_enum : clear\n");

    return 0;
}

static int amf_d3d11_preprocess_init_vp(AMFFilterContext *ctx,
                                        AMFSurface *surface)
{
    HRESULT hr;

    if (ctx->d3d11_vp)
        return 0;

    if (!ctx->d3d11_vp_enum) {
        av_log(ctx, AV_LOG_ERROR,
               "init_vp : failed : d3d11_vp_enum is not created\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO, "init_vp : pass : d3d11_vp_enum is already created\n");

    hr = ctx->d3d11_video_device->lpVtbl->CreateVideoProcessor(
        ctx->d3d11_video_device,
        ctx->d3d11_vp_enum,
        0,
        &ctx->d3d11_vp);

    if (FAILED(hr) || !ctx->d3d11_vp) {
        av_log(ctx, AV_LOG_ERROR,
               "init_vp : failed : CreateVideoProcessor\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "init_vp : pass : CreateVideoProcessor\n");

    av_log(ctx, AV_LOG_INFO, "init_vp : clear\n");

    return 0;
}

static DXGI_FORMAT amf_format_to_dxgi(AMF_SURFACE_FORMAT fmt)
{
    switch (fmt) {
    case AMF_SURFACE_NV12:
        return DXGI_FORMAT_NV12;
    case AMF_SURFACE_P010:
        return DXGI_FORMAT_P010;
    case AMF_SURFACE_BGRA:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

static const char *amf_format_to_str(AMF_SURFACE_FORMAT fmt) {
    switch (fmt) {
        case AMF_SURFACE_UNKNOWN: return "UNKNOWN";
        case AMF_SURFACE_NV12:    return "NV12";
        case AMF_SURFACE_P010:    return "P010";
        case AMF_SURFACE_YUV420P: return "YUV420P";
        case AMF_SURFACE_RGBA:    return "RGBA";
        case AMF_SURFACE_BGRA:    return "BGRA";
        case AMF_SURFACE_YUY2:    return "YUY2";
        default:                  return "OTHER_UNSUPPORTED";
    }
}

static int amf_d3d11_preprocess_config_vp(AMFFilterContext *ctx,
                                          AMFSurface *surface)
{
    HRESULT hr;
    int i;

    if (ctx->d3d11_tex_in[0])
        return 0;

    if (!ctx->d3d11_video_context) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : d3d11_video_context is not created\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : d3d11_video_context is already created\n");

    if (!ctx->d3d11_vp) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : d3d11_vp is not created\n");
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : d3d11_vp is already created\n");

    /*
     * set input / output sar
     */
    DXGI_RATIONAL dxgi_out_sar = {ctx->dxgi_in_sar.Numerator, ctx->dxgi_in_sar.Denominator};
    ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamPixelAspectRatio(
        ctx->d3d11_video_context,
        ctx->d3d11_vp,
        0,
        TRUE,
        &(ctx->dxgi_in_sar),
        &dxgi_out_sar);
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamPixelAspectRatio\n");

    /*
     * set crop rect
     */
    RECT src_rect = {
        ctx->crop_x,
        ctx->crop_y,
        ctx->crop_x + ctx->crop_w_eff,
        ctx->crop_y + ctx->crop_h_eff
    };
    ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamSourceRect(
        ctx->d3d11_video_context,
        ctx->d3d11_vp,
        0,
        TRUE,
        &src_rect);
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamSourceRect\n");

    /* 
     * set dest rect
     */

    RECT dst_rect = {
        ctx->dest_x,
        ctx->dest_y,
        ctx->dest_x + ctx->crop_w_eff,
        ctx->dest_y + ctx->crop_h_eff
    };
    ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamDestRect(
        ctx->d3d11_video_context,
        ctx->d3d11_vp,
        0,
        TRUE,
        &dst_rect);
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamDestRect\n");

    /*
     * set output target rect
     */
    RECT target_rect = {
        0,
        0,
        ctx->target_w,
        ctx->target_h
    };
    ctx->d3d11_video_context->lpVtbl->VideoProcessorSetOutputTargetRect(
        ctx->d3d11_video_context,
        ctx->d3d11_vp,
        TRUE,
        &target_rect);
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetOutputTargetRect\n");

    /*
     * get amf frame format
     */
    AMF_FRAME_TYPE frame_type = surface->pVtbl->GetFrameType(surface);
    switch (frame_type) {
    case AMF_FRAME_PROGRESSIVE:
        ctx->deint_frames_format[0] = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : first frame type     PRG [%d]\n", ctx->deint_frames_new);
        break;
    case AMF_FRAME_INTERLEAVED_EVEN_FIRST:
        ctx->deint_frames_format[0] = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : first frame type     TFF [%d]\n", ctx->deint_frames_new);
        break;
    case AMF_FRAME_INTERLEAVED_ODD_FIRST:
        ctx->deint_frames_format[0] = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : first frame type     BFF [%d]\n", ctx->deint_frames_new);
        break;
    case AMF_FRAME_UNKNOWN:
        av_log(ctx, AV_LOG_ERROR, "config_vp : failed : invalid frame type [AMF_FRAME_UNKNOWN]\n");
        return AVERROR(EINVAL);
        break;
    }

    if (!ctx->d3d11_deint) {
        ctx->deint_frames_format[0] = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : deint=off,              input frame format forcing [PRG]\n");
    } else {
        if (ctx->d3d11_deint_frame_format == 1) {
            ctx->deint_frames_format[0] = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
            av_log(ctx, AV_LOG_INFO, "config_vp : pass : deint_frame_format=tff, input frame format forcing [TFF]\n");
        } else if (ctx->d3d11_deint_frame_format == 2) {
            ctx->deint_frames_format[0] = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
            av_log(ctx, AV_LOG_INFO, "config_vp : pass : deint_frame_format=bff, input frame format forcing [TFF]\n");
        } else {
            av_log(ctx, AV_LOG_INFO, "config_vp : pass : deint_frame_format=auto, input frame format [auto-detect]\n");
        }
    }

    /*
     * Notify interlaced input
     */
    ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamFrameFormat(
        ctx->d3d11_video_context,
        ctx->d3d11_vp,
        0,
        ctx->deint_frames_format[0]);
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamFrameFormat\n");

    /*
     * Enable driver auto processing (deinterlace is implicit here)
     */
    switch (ctx->d3d11_deint) {
    case 1: // fast (driver auto)
        ctx->deint_frames = 1;
        ctx->deint_frames_cur = 0;
        ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamAutoProcessingMode(
            ctx->d3d11_video_context,
            ctx->d3d11_vp,
            0,
            TRUE);
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamAutoProcessingMode(TRUE)\n");
        break;
    case 2: // adaptive
        ctx->deint_frames = ctx->deint_frames_min;
        ctx->deint_frames_cur = (ctx->deint_frames + 1) / 2;
        for (i = 1; i < ctx->deint_frames; i++) {
            ctx->deint_frames_format[i] = ctx->deint_frames_format[0];
        }
        ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamAutoProcessingMode(
            ctx->d3d11_video_context,
            ctx->d3d11_vp,
            0,
            TRUE);
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamAutoProcessingMode(TRUE)\n");
        break;
    default:
        ctx->deint_frames = 1;
        ctx->deint_frames_cur = 0;
        ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamAutoProcessingMode(
            ctx->d3d11_video_context,
            ctx->d3d11_vp,
            0,
            FALSE);
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : VideoProcessorSetStreamAutoProcessingMode(FALSE)\n");
        break;
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : queue size for deint_frames [%d]\n", ctx->deint_frames);

    /*
     * create ID3D11Texture2D tex for processor input view
     */
    AMF_SURFACE_FORMAT amf_fmt = surface->pVtbl->GetFormat(surface);
    if (amf_fmt != AMF_SURFACE_NV12 && amf_fmt != AMF_SURFACE_P010) {
        av_log(ctx, AV_LOG_ERROR,
            "config_vp : failed : GetFormat(surface) ... Unsupported AMF format: %s\n",
            amf_format_to_str(amf_fmt));
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO,
        "config_vp : pass : GetFormat(surface) == AMF_SURFACE_NV12\n");

    AMFPlane *plane;
    ID3D11Texture2D *src_tex;
    D3D11_TEXTURE2D_DESC src_tex_desc;

    plane = surface->pVtbl->GetPlaneAt(surface, 0);
    if (!plane) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : GetPlaneAt\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : GetPlaneAt\n");

    src_tex = (ID3D11Texture2D *)plane->pVtbl->GetNative(plane);
    if(!src_tex) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : GetNative\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : GetNative\n");

    src_tex->lpVtbl->GetDesc(src_tex, &src_tex_desc);
    if (src_tex_desc.ArraySize > 1) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : cannot proceed (ArraySize > 1)\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : checked ArraySize.\n");

    /*
     * create Texture2D for input view
     */
    src_tex_desc.BindFlags = D3D11_BIND_DECODER;

    for (i=0; i<ctx->deint_frames; i++ ) {
        hr = ctx->d3d11_device->lpVtbl->CreateTexture2D(
            ctx->d3d11_device,
            &src_tex_desc,
            NULL,
            &(ctx->d3d11_tex_in[i]));
        if (FAILED(hr) || !ctx->d3d11_tex_in[i]) {
            av_log(ctx, AV_LOG_ERROR,
                   "config_vp : failed : CreateTexture2D(d3d11_tex_in[%d]) : hr=0x%08lx\n",
                   i, (unsigned long)hr);
            return AVERROR_EXTERNAL;
        }
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : CreateTexture2D(d3d11_tex_in[%d])\n", i);
    }

    /*
     * create processor input view
     */
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd;
    memset(&ivd, 0, sizeof(ivd));
    ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    ivd.Texture2D.MipSlice = 0;
    ivd.Texture2D.ArraySlice = 0;

    for (i=0; i<ctx->deint_frames; i++ ) {
        hr = ctx->d3d11_video_device->lpVtbl->CreateVideoProcessorInputView(
            ctx->d3d11_video_device,
            (ID3D11Resource *)(ctx->d3d11_tex_in[i]),
            ctx->d3d11_vp_enum,
            &ivd,
            &(ctx->d3d11_in_view[i]));
        if (FAILED(hr) || !ctx->d3d11_in_view[i]) {
            av_log(ctx, AV_LOG_ERROR,
                   "config_vp : failed : CreateVideoProcessorInputView(d3d11_in_view[%d]) : hr=0x%08lx\n",
                   i, (unsigned long)hr);
            return AVERROR_EXTERNAL;
        }
        av_log(ctx, AV_LOG_INFO, "config_vp : pass : CreateVideoProcessorInputView(d3d11_in_view[%d])\n", i);
    }

    /*
     * create ID3D11Texture2D tex for processor output view
     */
    DXGI_FORMAT dxgi_fmt = amf_format_to_dxgi(amf_fmt);

    D3D11_TEXTURE2D_DESC dst_tex_desc;
    memset(&dst_tex_desc, 0, sizeof(dst_tex_desc));
    dst_tex_desc.Width              = ctx->target_w;
    dst_tex_desc.Height             = ctx->target_h;
    dst_tex_desc.MipLevels          = 1;
    dst_tex_desc.ArraySize          = 1;
    dst_tex_desc.Format             = dxgi_fmt;
    dst_tex_desc.SampleDesc.Count   = 1;
    dst_tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    dst_tex_desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = ctx->d3d11_device->lpVtbl->CreateTexture2D(
        ctx->d3d11_device, &dst_tex_desc, NULL, &(ctx->d3d11_tex_out));
    if (FAILED(hr) || !ctx->d3d11_tex_out) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : CreateTexture2D(ctx->d3d11_tex_out)\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : CreateTexture2D(d3d11_tex_out)\n");

    /*
     * create processor output view
     */
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd;
    memset(&ovd, 0, sizeof(ovd));
    ovd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ovd.Texture2D.MipSlice = 0;

    hr = ctx->d3d11_video_device->lpVtbl->CreateVideoProcessorOutputView(
        ctx->d3d11_video_device,
        (ID3D11Resource *)(ctx->d3d11_tex_out),
        ctx->d3d11_vp_enum,
        &ovd,
        &(ctx->d3d11_out_view));
    if (FAILED(hr) || !ctx->d3d11_out_view) {
        av_log(ctx, AV_LOG_ERROR,
               "config_vp : failed : CreateVideoProcessorOutputView : hr=0x%08lx\n",
               (unsigned long)hr);
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, AV_LOG_INFO, "config_vp : pass : CreateVideoProcessorOutputView\n");

    av_log(ctx, AV_LOG_INFO, "config_vp : clear\n");

    return 0;
}

/*
 * D3D11 VideoProcessor pre-process
 *
 * Actual implementation (deinterlace / crop / aspect-fit)
 * will be added in later change sets.
 */
static int amf_d3d11_preprocess(AVFilterContext *avctx,
                                AMFFilterContext *ctx,
                                AMFSurface **surface)
{
    HRESULT hr;
    int ret;
    int av_log_level = AV_LOG_VERBOSE;
    int i, pos;

    if (!ctx->d3d11_log_info)
        av_log_level = AV_LOG_INFO;

    ret = amf_d3d11_preprocess_conv_memtype(ctx, *surface);
    if (ret < 0)
        return ret;

    ret = amf_d3d11_preprocess_init(ctx, *surface);
    if (ret < 0)
        return ret;

    ret = amf_d3d11_preprocess_calc_sizes(avctx);
    if (ret < 0)
        return ret;

    ret = amf_d3d11_preprocess_init_vp_enum(ctx);
    if (ret < 0)
        return ret;

    ret = amf_d3d11_preprocess_init_vp(ctx, *surface);
    if (ret < 0)
        return ret;

    ret = amf_d3d11_preprocess_config_vp(ctx, *surface);
    if (ret < 0)
        return ret;

    /*
     * Copy amf provided texture to input texture
     */
    AMFPlane *plane;
    ID3D11Texture2D *src_tex;

    plane = (*surface)->pVtbl->GetPlaneAt((*surface), 0);
    if (!plane) {
        av_log(ctx, AV_LOG_ERROR,
               "d3d11_preprocess : failed : GetPlaneAt\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, av_log_level, "d3d11_preprocess : pass : GetPlaneAt\n");

    src_tex = (ID3D11Texture2D *)plane->pVtbl->GetNative(plane);
    if(!src_tex) {
        av_log(ctx, AV_LOG_ERROR,
               "d3d11_preprocess : failed : GetNative\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, av_log_level, "d3d11_preprocess : pass : GetNative\n");

    ctx->d3d11_context->lpVtbl->CopyResource(
        ctx->d3d11_context,
        (ID3D11Resource *)(ctx->d3d11_tex_in[ctx->deint_frames_new]),
        (ID3D11Resource *)src_tex);
    av_log(ctx, av_log_level, "d3d11_preprocess : pass : CopyResource(d3d11_tex_in[%d])\n",
        ctx->deint_frames_new);

    /*
     * copy frame format at first past.
     * must copy befor save frame format.
     */
    pos = ctx->deint_frames_cur - 1; // pos of frame format at first past
    if (pos < 0)
        pos = ctx->deint_frames - 1;
    D3D11_VIDEO_FRAME_FORMAT frame_format_past_first = ctx->deint_frames_format[pos];
    switch (frame_format_past_first) {
    case D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE:
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : past frame type      PRG [%d]\n", pos);
        break;
    case D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST:
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : past frame type      TFF [%d]\n", pos);
        break;
    case D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST:
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : past frame type      BFF [%d]\n", pos);
        break;
    }

    /*
     * save new frame format
     */
    AMF_FRAME_TYPE frame_type = (*surface)->pVtbl->GetFrameType(*surface);
    switch (frame_type) {
    case AMF_FRAME_PROGRESSIVE:
        ctx->deint_frames_format[ctx->deint_frames_new] = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : save new frame type  PRG [%d]\n", ctx->deint_frames_new);
        break;
    case AMF_FRAME_INTERLEAVED_EVEN_FIRST:
        ctx->deint_frames_format[ctx->deint_frames_new] = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : save new frame type  TFF [%d]\n", ctx->deint_frames_new);
        break;
    case AMF_FRAME_INTERLEAVED_ODD_FIRST:
        ctx->deint_frames_format[ctx->deint_frames_new] = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : save new frame type  BFF [%d]\n", ctx->deint_frames_new);
        break;
    case AMF_FRAME_UNKNOWN:
        av_log(ctx, AV_LOG_ERROR, "d3d11_preprocess : failed : invalid frame type [AMF_FRAME_UNKNOWN]\n");
        return AVERROR(EINVAL);
        break;
    }

    /*
     * count up deint_frames_new
     */
    if (ctx->deint_frames > 1) {
        ctx->deint_frames_new ++;
        if (ctx->deint_frames_new == ctx->deint_frames)
            ctx->deint_frames_new = 0;
    }

    /*
     * queuing only
     */
    if (ctx->deint_frames_que < (ctx->deint_frames - 1) / 2) {
        ctx->deint_frames_cur ++;
        if (ctx->deint_frames_cur == ctx->deint_frames)
            ctx->deint_frames_cur = 0;

        return 0;
    }

    /*
     * setup input stream
     */
    D3D11_VIDEO_PROCESSOR_STREAM stream;
    ID3D11VideoProcessorInputView *view_past[VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX];
    ID3D11VideoProcessorInputView *view_future[VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX];

    memset(&stream, 0, sizeof(stream));
    stream.Enable = TRUE;
    stream.pInputSurface = ctx->d3d11_in_view[ctx->deint_frames_cur];

    /*
     * Notify interlaced input
     */
    switch (ctx->deint_frames_format[ctx->deint_frames_cur]) {
    case D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE:
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : current frame type   PRG [%d]\n", pos);
        break;
    case D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST:
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : current frame type   TFF [%d]\n", pos);
        break;
    case D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST:
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : current frame type   BFF [%d]\n", pos);
        break;
    }

    if (ctx->deint_frames_format[ctx->deint_frames_cur] != frame_format_past_first) {
        if (!ctx->d3d11_deint) {
            av_log(ctx, av_log_level, "d3d11_preprocess : pass : deint=off\n");
            av_log(ctx, av_log_level, "d3d11_preprocess : pass : skip VideoProcessorSetStreamFrameFormat()\n");
        } else {
            av_log(ctx, av_log_level, "d3d11_preprocess : pass : deint=%d\n", ctx->d3d11_deint);
            av_log(ctx, av_log_level, "d3d11_preprocess : pass : deint_frame_format=%d\n", ctx->d3d11_deint_frame_format);
            if (ctx->d3d11_deint_frame_format) {
                av_log(ctx, av_log_level, "d3d11_preprocess : pass : skip VideoProcessorSetStreamFrameFormat()\n");
            } else {
                ctx->d3d11_video_context->lpVtbl->VideoProcessorSetStreamFrameFormat(
                    ctx->d3d11_video_context,
                    ctx->d3d11_vp,
                    0,
                    ctx->deint_frames_format[ctx->deint_frames_cur]);

                if (ctx->deint_frames_format[ctx->deint_frames_cur] == D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST)
                    stream.InputFrameOrField = 1;

                av_log(ctx, av_log_level, "d3d11_preprocess : pass : VideoProcessorSetStreamFrameFormat(current)\n");
            }
        }
    }

    /*
     * setup view_past / view_future
     */
    if (ctx->deint_frames > 1) {
        pos = ctx->deint_frames_cur - 1;
        stream.ppPastSurfaces = view_past;
        stream.PastFrames = (ctx->deint_frames - 1) / 2;
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : past frames   ");
        for (i=0; i<stream.PastFrames; i++) {
            if (pos < 0)
                pos = ctx->deint_frames - 1;

            view_past[i] = ctx->d3d11_in_view[pos];
            av_log(ctx, av_log_level, "[%d]", pos);
            pos --;
        }
        av_log(ctx, av_log_level, "\n");

        pos = ctx->deint_frames_cur + 1;
        stream.ppFutureSurfaces = view_future;
        stream.FutureFrames = (ctx->deint_frames - 1) / 2;
        av_log(ctx, av_log_level, "d3d11_preprocess : pass : future frames ");
        for (i=0; i<stream.FutureFrames; i++) {
            if (pos >= ctx->deint_frames)
                pos = 0;

            view_future[i] = ctx->d3d11_in_view[pos];
            av_log(ctx, av_log_level, "[%d]", pos);
            pos ++;
        }
        av_log(ctx, av_log_level, "\n");
    }

    /*
     * VideoProcessor Blt
     */
    hr = ctx->d3d11_video_context->lpVtbl->VideoProcessorBlt(
        ctx->d3d11_video_context,
        ctx->d3d11_vp,
        ctx->d3d11_out_view,
        0,
        1,
        &stream);
    if (FAILED(hr)) {
        av_log(ctx, AV_LOG_ERROR,
            "d3d11_preprocess : failed : VideoProcessorBlt : hr=0x%08lx\n",
            (unsigned long)hr);
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, av_log_level,
        "d3d11_preprocess : pass : VideoProcessorBlt\n");

    /*
     * count up deint_frames_cur
     */
    ctx->deint_frames_cur ++;
    if (ctx->deint_frames_cur == ctx->deint_frames)
        ctx->deint_frames_cur = 0;

    /*
     * DX11 Texture → AMF Surface
     */
    AMFSurface *surface_out = NULL;
    AMF_RESULT res;

    res = ctx->amf_device_ctx->context->pVtbl->CreateSurfaceFromDX11Native(
        ctx->amf_device_ctx->context,
        (void *)(ctx->d3d11_tex_out),
        &surface_out, NULL);
    if (res != AMF_OK || !surface_out) {
        av_log(ctx, AV_LOG_ERROR,
               "d3d11_preprocess : failed : CreateSurfaceFromDX11Native\n");
        return AVERROR_EXTERNAL;
    }
    av_log(ctx, av_log_level,
        "d3d11_preprocess : pass : CreateSurfaceFromDX11Native\n");

    /*
     * replace input surface with VP processed surface
     */
    (*surface)->pVtbl->Release(*surface);
    *surface = surface_out;

    if (!ctx->d3d11_log_info)
      ctx->d3d11_log_info = 1;

    return 0;
}

int amf_d3d11_preprocess_required(AVFilterContext *avctx)
{
    AMFFilterContext  *ctx = avctx->priv;

    /*
     * Deinterlacing is required.
     */
    if (ctx->d3d11_deint)
        return 1;

    /*
     * Image cropping is specified.
     */
    if (ctx->crop_x || ctx->crop_y || ctx->crop_w || ctx->crop_h)
        return 1;

    /*
     * If the aspect ratio of the cropped input image
     * differs from that of the output image.
     *
     * Since non-zero errors are only possible during crop value validation,
     * calling it here will guaranteed a return value of 0.
     */
    if (amf_d3d11_preprocess_calc_sizes(avctx) == 0) {
        if (ctx->dest_x || ctx->dest_y)
            return 1;
    }

    return 0;
}

int amf_activate(AVFilterContext *avctx)
{
    AMFFilterContext *ctx = avctx->priv;
    AVFilterLink *inlink = avctx->inputs[0];
    AVFilterLink *outlink = avctx->outputs[0];
    AVFrame *in = NULL;
    int ret, status;
    int64_t pts;

    /*
     * Forward status (EOF/Error) and back-propagate frame requests from downstream to upstream
     */
    FF_FILTER_FORWARD_STATUS_BACK(outlink, inlink);

    /*
     * If the downstream filter doesn't need a frame right now, do nothing
     */
    if (!ff_outlink_frame_wanted(outlink)) {
        av_log(ctx, AV_LOG_WARNING, "amf_activate : warn : ff_outlink_frame_wanted() return 0\n");
        return 0;
    }

    /*
     * Step 1: Buffer initial frames until the queue reaches the required delay
     */
    if (!ctx->deint_frame_eof) {
        if (ctx->deint_frames_que < (ctx->deint_frames - 1) / 2) {
            ret = ff_inlink_consume_frame(inlink, &in);
            if (ret < 0) {
                av_log(ctx, AV_LOG_ERROR, "amf_activate : failed : ff_inlink_consume_frame():p1\n");
                return ret;
            }

            if (ret > 0) {
                ret = amf_filter_filter_frame(inlink, in);
                if (ret < 0)
                    return ret;

                /*
                 * Frame consumed and queued; return 0 to allow scheduler to re-evaluate
                 */
                return 0;
            }

            /*
             * If no frame is available, request one from upstream to avoid deadlock
             */
            ff_inlink_request_frame(inlink);
            return FFERROR_NOT_READY;
        }
    }

    /*
     * Step 2: Check if we've hit EOF or an error status is set on the input link
     */
    ret = ff_inlink_acknowledge_status(inlink, &status, &pts);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "amf_activate : failed : ff_inlink_acknowledge_status()\n");
        return ret;
    }

    /*
     * Handle EOF or continuing flush after EOF detection
     */
    if (ret > 0 || ctx->deint_frame_eof) {
        if (amf_d3d11_preprocess_required(avctx)) {
            av_log(ctx, AV_LOG_INFO, "amf_activate : pass : enter flush after EOF\n");
            av_log(ctx, AV_LOG_INFO, "amf_activate : pass : queue [%d]\n", ctx->deint_frames_que);

            ctx->deint_frame_eof = 1;
            ctx->d3d11_log_info = 0;

            /*
             * If there are still frames in the queue, schedule the filter to run again
             */
            if (ctx->deint_frames_que > 0) {
                /*
                 * Clone the last frame to fill the pipeline during flush (padding)
                 */
                AVFrame *next = av_frame_clone(ctx->deint_frame_last);
                if (!next) {
                    av_log(ctx, AV_LOG_ERROR, "amf_activate : failed : av_frame_clone()\n");
                    return AVERROR(ENOMEM);
                }

                ret = amf_filter_filter_frame(inlink, next);
                if (ret < 0)
                    return ret;

                if (ctx->deint_frames_que > 0) {
                    ff_filter_set_ready(avctx, 100);
                    return 0;
                }
            }
        }

        /*
         * Finalize: Notify the downstream filter that the stream has ended
         */
        ff_outlink_set_status(outlink, status ? status : AVERROR_EOF, pts);
        return 0;
    }

    /*
     * Step 3: Regular processing - consume one frame and process it
     */
    ret = ff_inlink_consume_frame(inlink, &in);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "amf_activate : failed : ff_inlink_consume_frame():p2\n");
        return ret;
    }

    if (ret > 0) {
        ret = amf_filter_filter_frame(inlink, in);
        if (ret < 0)
            return ret;
    }

    /*
     * Request another frame from upstream to keep the pipeline moving
     */
    ff_inlink_request_frame(inlink);

    return 0;
}
#endif // VF_VPP_AMF_D3D11_HWACCEL

int amf_filter_init(AVFilterContext *avctx)
{
    AMFFilterContext     *ctx = avctx->priv;

    if (!strcmp(ctx->format_str, "same")) {
        ctx->format = AV_PIX_FMT_NONE;
    } else {
        ctx->format = av_get_pix_fmt(ctx->format_str);
        if (ctx->format == AV_PIX_FMT_NONE) {
            av_log(avctx, AV_LOG_ERROR, "Unrecognized pixel format: %s\n", ctx->format_str);
            return AVERROR(EINVAL);
        }
    }

    return 0;
}

void amf_filter_uninit(AVFilterContext *avctx)
{
    AMFFilterContext *ctx = avctx->priv;

    if (ctx->component) {
        ctx->component->pVtbl->Terminate(ctx->component);
        ctx->component->pVtbl->Release(ctx->component);
        ctx->component = NULL;
    }

    if (ctx->master_display)
        av_freep(&ctx->master_display);

    if (ctx->light_meta)
        av_freep(&ctx->light_meta);

    av_buffer_unref(&ctx->amf_device_ref);
    av_buffer_unref(&ctx->hwdevice_ref);
    av_buffer_unref(&ctx->hwframes_in_ref);
    av_buffer_unref(&ctx->hwframes_out_ref);

#if VF_VPP_AMF_D3D11_HWACCEL
    amf_d3d11_preprocess_uninit(ctx);
#endif // VF_VPP_AMF_D3D11_HWACCEL
}

int amf_filter_filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext             *avctx = inlink->dst;
    AMFFilterContext             *ctx = avctx->priv;
    AVFilterLink                *outlink = avctx->outputs[0];
    AMF_RESULT  res;
    AMFSurface *surface_in;
    AMFSurface *surface_out;
    AMFData *data_out = NULL;
    enum AVColorSpace out_colorspace;
    enum AVColorRange out_color_range;

    AVFrame *out = NULL;
    int ret = 0;

    if (!ctx->component)
        return AVERROR(EINVAL);

    ret = amf_avframe_to_amfsurface(avctx, in, &surface_in);
    if (ret < 0)
        goto fail;

#if VF_VPP_AMF_D3D11_HWACCEL
    /*
     * ------------------------------------------------------------------
     * D3D11 VideoProcessor pre-process insertion point (DESIGN FIXED)
     *
     * All GPU-side pre-processing (deinterlace / crop / aspect-fit)
     * MUST be performed here:
     *
     *   - AFTER amf_avframe_to_amfsurface()
     *       -> surface_in is an AMFSurface backed by DX11 texture
     *   - BEFORE AMF component SubmitInput()
     *
     * Rationale:
     *   - Ensures zero-copy GPU pipeline
     *   - Avoids CPU roundtrip
     *   - Keeps AMF VideoConverter responsible only for final scaling (B1)
     *   - Prevents double VideoProcessor ambiguity
     *
     * Any D3D11 VideoProcessor-based preprocessing added elsewhere
     * is considered a design error.
     * ------------------------------------------------------------------
     */

    /*
     * D3D11 preprocess decision
     *
     * Run preprocess if:
     *  - deinterlace enabled
     *  - crop specified
     *  - aspect ratio mismatch between input and output
     */
    if (amf_d3d11_preprocess_required(avctx)) {
        ret = amf_d3d11_preprocess(avctx, ctx, &surface_in);
        if (ret < 0) {
            surface_in->pVtbl->Release(surface_in);
            goto fail;
        }

        if ((ctx->deint_frames - 1) / 2) {
            if(ctx->deint_frame_last)
                av_frame_free(&(ctx->deint_frame_last));

            ctx->deint_frame_last = av_frame_clone(in);
            if (!ctx->deint_frame_last) {
                surface_in->pVtbl->Release(surface_in);
                goto fail;
            }

            if (ctx->deint_frame_eof) {
                ctx->deint_frames_que --;
            } else
            if (ctx->deint_frames_que < (ctx->deint_frames - 1) / 2) {
                ctx->deint_frames_que ++;
                surface_in->pVtbl->Release(surface_in);
                return 0;
            }
        }
    }
#endif // VF_VPP_AMF_D3D11_HWACCEL

    res = ctx->component->pVtbl->SubmitInput(ctx->component, (AMFData*)surface_in);
    surface_in->pVtbl->Release(surface_in); // release surface after use
    AMF_GOTO_FAIL_IF_FALSE(avctx, res == AMF_OK, AVERROR_UNKNOWN, "SubmitInput() failed with error %d\n", res);
    res = ctx->component->pVtbl->QueryOutput(ctx->component, &data_out);
    AMF_GOTO_FAIL_IF_FALSE(avctx, res == AMF_OK, AVERROR_UNKNOWN, "QueryOutput() failed with error %d\n", res);

    if (data_out) {
        AMFGuid guid = IID_AMFSurface();
        res = data_out->pVtbl->QueryInterface(data_out, &guid, (void**)&surface_out); // query for buffer interface
        data_out->pVtbl->Release(data_out);
        AMF_RETURN_IF_FALSE(avctx, res == AMF_OK, AVERROR_UNKNOWN, "QueryInterface(IID_AMFSurface) failed with error %d\n", res);
    } else {
        return AVERROR(EAGAIN);
    }

    out = amf_amfsurface_to_avframe(avctx, surface_out);

    ret = av_frame_copy_props(out, in);
    av_frame_unref(in);

    out_colorspace = AVCOL_SPC_UNSPECIFIED;

    if (ctx->color_profile != AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN) {
        switch(ctx->color_profile) {
        case AMF_VIDEO_CONVERTER_COLOR_PROFILE_601:
            out_colorspace = AVCOL_SPC_SMPTE170M;
        break;
        case AMF_VIDEO_CONVERTER_COLOR_PROFILE_709:
            out_colorspace = AVCOL_SPC_BT709;
        break;
        case AMF_VIDEO_CONVERTER_COLOR_PROFILE_2020:
            out_colorspace = AVCOL_SPC_BT2020_NCL;
        break;
        case AMF_VIDEO_CONVERTER_COLOR_PROFILE_JPEG:
            out_colorspace = AVCOL_SPC_RGB;
        break;
        default:
            out_colorspace = AVCOL_SPC_UNSPECIFIED;
        break;
        }
        out->colorspace = out_colorspace;
    }

    out_color_range = AVCOL_RANGE_UNSPECIFIED;
    if (ctx->out_color_range == AMF_COLOR_RANGE_FULL)
        out_color_range = AVCOL_RANGE_JPEG;
    else if (ctx->out_color_range == AMF_COLOR_RANGE_STUDIO)
        out_color_range = AVCOL_RANGE_MPEG;

    if (ctx->out_color_range != AMF_COLOR_RANGE_UNDEFINED)
        out->color_range = out_color_range;

    if (ctx->out_primaries != AMF_COLOR_PRIMARIES_UNDEFINED)
        out->color_primaries = ctx->out_primaries;

    if (ctx->out_trc != AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED)
        out->color_trc = ctx->out_trc;

#if VF_VPP_AMF_D3D11_HWACCEL
    if (amf_d3d11_preprocess_required(avctx)) {
        out->crop_top    = 0;
        out->crop_bottom = 0;
        out->crop_left   = 0;
        out->crop_right  = 0;

        if (ctx->d3d11_deint) {
            out->flags &= ~AV_FRAME_FLAG_INTERLACED;
            out->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
    }

    if (ctx->reset_sar)
        out->sample_aspect_ratio = (AVRational){1, 1};
#endif // VF_VPP_AMF_D3D11_HWACCEL

    if (ret < 0)
        goto fail;

    out->hw_frames_ctx = av_buffer_ref(ctx->hwframes_out_ref);
    if (!out->hw_frames_ctx) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
fail:
    av_frame_free(&in);
    av_frame_free(&out);
    return ret;
}



int amf_setup_input_output_formats(AVFilterContext *avctx,
                                    const enum AVPixelFormat *input_pix_fmts,
                                    const enum AVPixelFormat *output_pix_fmts)
{
    int err;
    AVFilterFormats *input_formats;
    AVFilterFormats *output_formats;

    //in case if hw_device_ctx is set to DXVA2 we change order of pixel formats to set DXVA2 be chosen by default
    //The order is ignored if hw_frames_ctx is not NULL on the config_output stage
    if (avctx->hw_device_ctx) {
        AVHWDeviceContext *device_ctx = (AVHWDeviceContext*)avctx->hw_device_ctx->data;

        switch (device_ctx->type) {
    #if CONFIG_D3D11VA
        case AV_HWDEVICE_TYPE_D3D11VA:
            {
                static const enum AVPixelFormat output_pix_fmts_d3d11[] = {
                    AV_PIX_FMT_D3D11,
                    AV_PIX_FMT_NONE,
                };
                output_pix_fmts = output_pix_fmts_d3d11;
            }
            break;
    #endif
    #if CONFIG_DXVA2
        case AV_HWDEVICE_TYPE_DXVA2:
            {
                static const enum AVPixelFormat output_pix_fmts_dxva2[] = {
                    AV_PIX_FMT_DXVA2_VLD,
                    AV_PIX_FMT_NONE,
                };
                output_pix_fmts = output_pix_fmts_dxva2;
            }
            break;
    #endif
        case AV_HWDEVICE_TYPE_AMF:
            break;
        default:
            {
                av_log(avctx, AV_LOG_ERROR, "Unsupported device : %s\n", av_hwdevice_get_type_name(device_ctx->type));
                return AVERROR(EINVAL);
            }
            break;
        }
    }

    input_formats = ff_make_pixel_format_list(output_pix_fmts);
    if (!input_formats) {
        return AVERROR(ENOMEM);
    }
    output_formats = ff_make_pixel_format_list(output_pix_fmts);
    if (!output_formats) {
        return AVERROR(ENOMEM);
    }

    if ((err = ff_formats_ref(input_formats, &avctx->inputs[0]->outcfg.formats)) < 0)
        return err;

    if ((err = ff_formats_ref(output_formats, &avctx->outputs[0]->incfg.formats)) < 0)
        return err;
    return 0;
}

int amf_copy_surface(AVFilterContext *avctx, const AVFrame *frame,
    AMFSurface* surface)
{
    AMFPlane *plane;
    uint8_t  *dst_data[4];
    int       dst_linesize[4];
    int       planes;
    int       i;

    planes = (int)surface->pVtbl->GetPlanesCount(surface);
    av_assert0(planes < FF_ARRAY_ELEMS(dst_data));

    for (i = 0; i < planes; i++) {
        plane = surface->pVtbl->GetPlaneAt(surface, i);
        dst_data[i] = plane->pVtbl->GetNative(plane);
        dst_linesize[i] = plane->pVtbl->GetHPitch(plane);
    }
    av_image_copy(dst_data, dst_linesize,
        (const uint8_t**)frame->data, frame->linesize, frame->format,
        frame->width, frame->height);

    return 0;
}

int amf_init_filter_config(AVFilterLink *outlink, enum AVPixelFormat *in_format)
{
    int err;
    AMF_RESULT res;
    AVFilterContext *avctx = outlink->src;
    AVFilterLink   *inlink = avctx->inputs[0];
    AMFFilterContext  *ctx = avctx->priv;
    AVHWFramesContext *hwframes_out;
    AVHWDeviceContext   *hwdev_ctx;
    enum AVPixelFormat in_sw_format = inlink->format;
    enum AVPixelFormat out_sw_format = ctx->format;
    FilterLink        *inl = ff_filter_link(inlink);
    FilterLink        *outl = ff_filter_link(outlink);
    double w_adj = 1.0;

    if ((err = ff_scale_eval_dimensions(avctx,
                                        ctx->w_expr, ctx->h_expr,
                                        inlink, outlink,
                                        &ctx->width, &ctx->height)) < 0)
        return err;

#if VF_VPP_AMF_D3D11_HWACCEL
    if (strlen(ctx->w_expr) == 2 && strstr(ctx->w_expr, "iw")
     && strlen(ctx->h_expr) == 2 && strstr(ctx->h_expr, "ih")) {
        if (ctx->crop_w || ctx->crop_h) {
            av_log(ctx, AV_LOG_INFO, "amf_init_filter_config : info : output size default, crop size given.\n");
            av_log(ctx, AV_LOG_INFO, "amf_init_filter_config : info : iw:[%d] ih:[%d]\n", inlink->w, inlink->h);
            av_log(ctx, AV_LOG_INFO, "amf_init_filter_config : info : ow:[%d] oh:[%d]\n", ctx->width, ctx->height);
            av_log(ctx, AV_LOG_INFO, "amf_init_filter_config : info : cw:[%d] ch:[%d]\n", ctx->crop_w, ctx->crop_h);

            ctx->width = ctx->crop_w;
            ctx->height = ctx->crop_h;
            if (ctx->width % 2) ctx->width ++;
            if (ctx->height % 2) ctx->height ++;
            av_log(ctx, AV_LOG_INFO, "amf_init_filter_config : pass : applied crop to output ow:[%d] oh:[%d]\n", ctx->width, ctx->height);
        }
    }

    /*
     * Regarding inlink->sample_aspect_ratio.num:
     * When w=-1 or w=-2 is specified, inlink->sample_aspect_ratio.num is non-zero. 
     * However, when w is unspecified (implicit w=iw), set as a fixed value (e.g., w=1280:h=720), 
     * or as an expression (e.g., w=ih*sar), inlink->sample_aspect_ratio.num becomes 0. 
     * This results in ff_scale_adjust_dimensions() being executed with w_adj remaining at 1.0.
     * 
     * This behavior seems unintended by design and is particularly problematic for my extension. 
     * Since inlink->sample_aspect_ratio.num is rarely 0 in individual frames passed to the filter, 
     * this might be due to a change in FFmpeg's filter specifications or a bug in the current version.
     * 
     * If this is a bug, it may be fixed in a future stable release. For now, I will handle this 
     * by choosing different command-line parameters depending on the specific objective.
     * 
     * 
     * Workaround for potential FFmpeg expression evaluation issues (as of 2026/04/15):
     * The following behaviors have been observed regarding 'w' (width) and 'reset_sar'.
     * Command-line arguments must be adjusted based on these patterns:
     *
     * 1. w unspecified & reset_sar=false:
     *    w is converted to 'iw' by expr; output SAR matches input SAR.
     * 2. w unspecified & reset_sar=true:
     *    w is converted to 'iw' by expr; output SAR is forced to 1.
     *    Note: This causes distortion if input SAR is not 1:1.
     * 3. w=-2 & reset_sar=false:
     *    w is calculated as 'h * dar / sar' by expr; output SAR matches input SAR.
     * 4. w=-2 & reset_sar=true:
     *    w is calculated as 'h * dar / sar', then scaled by 'w * sar' in vpp_amf;
     *    output SAR is forced to 1.
     * 5. w=value/expr & reset_sar=false:
     *    Output SAR matches input SAR. Input value should be (Display Width / SAR).
     * 6. w=value/expr & reset_sar=true:
     *    Output SAR is forced to 1. Input value should be the actual Display Width.
     */
#endif // VF_VPP_AMF_D3D11_HWACCEL

    if (ctx->reset_sar && inlink->sample_aspect_ratio.num)
        w_adj = (double) inlink->sample_aspect_ratio.num / inlink->sample_aspect_ratio.den;

    ff_scale_adjust_dimensions(inlink, &ctx->width, &ctx->height,
                               ctx->force_original_aspect_ratio, ctx->force_divisible_by, w_adj);

    av_buffer_unref(&ctx->amf_device_ref);
    av_buffer_unref(&ctx->hwframes_in_ref);
    av_buffer_unref(&ctx->hwframes_out_ref);
    ctx->local_context = 0;
    if (inl->hw_frames_ctx) {
        AVHWFramesContext *frames_ctx = (AVHWFramesContext*)inl->hw_frames_ctx->data;
        if (av_av_to_amf_format(frames_ctx->sw_format) == AMF_SURFACE_UNKNOWN) {
            av_log(avctx, AV_LOG_ERROR, "Format of input frames context (%s) is not supported by AMF.\n",
                   av_get_pix_fmt_name(frames_ctx->sw_format));
            return AVERROR(EINVAL);
        }

        err = av_hwdevice_ctx_create_derived(&ctx->amf_device_ref, AV_HWDEVICE_TYPE_AMF, frames_ctx->device_ref, 0);
        if (err < 0)
            return err;

        ctx->hwframes_in_ref = av_buffer_ref(inl->hw_frames_ctx);
        if (!ctx->hwframes_in_ref)
            return AVERROR(ENOMEM);

        in_sw_format = frames_ctx->sw_format;
    } else if (avctx->hw_device_ctx) {
        err = av_hwdevice_ctx_create_derived(&ctx->amf_device_ref, AV_HWDEVICE_TYPE_AMF, avctx->hw_device_ctx, 0);
        if (err < 0)
            return err;
        ctx->hwdevice_ref = av_buffer_ref(avctx->hw_device_ctx);
        if (!ctx->hwdevice_ref)
            return AVERROR(ENOMEM);
    } else {
        res = av_hwdevice_ctx_create(&ctx->amf_device_ref, AV_HWDEVICE_TYPE_AMF, NULL, NULL, 0);
        AMF_RETURN_IF_FALSE(avctx, res == 0, res, "Failed to create  hardware device context (AMF) : %s\n", av_err2str(res));

    }
    if(out_sw_format == AV_PIX_FMT_NONE){
        if(outlink->format == AV_PIX_FMT_AMF_SURFACE)
            out_sw_format = in_sw_format;
        else
            out_sw_format = outlink->format;
    }
    ctx->hwframes_out_ref = av_hwframe_ctx_alloc(ctx->amf_device_ref);
    if (!ctx->hwframes_out_ref)
        return AVERROR(ENOMEM);
    hwframes_out = (AVHWFramesContext*)ctx->hwframes_out_ref->data;
    hwdev_ctx = (AVHWDeviceContext*)ctx->amf_device_ref->data;
    if (hwdev_ctx->type == AV_HWDEVICE_TYPE_AMF)
    {
        ctx->amf_device_ctx =  hwdev_ctx->hwctx;
    }
    hwframes_out->format    = AV_PIX_FMT_AMF_SURFACE;
    hwframes_out->sw_format = out_sw_format;

    if (inlink->format == AV_PIX_FMT_AMF_SURFACE) {
        *in_format = in_sw_format;
    } else {
        *in_format = inlink->format;
    }
    outlink->w = ctx->width;
    outlink->h = ctx->height;

    if (ctx->reset_sar)
        outlink->sample_aspect_ratio = (AVRational){1, 1};
    else if (inlink->sample_aspect_ratio.num) {
        outlink->sample_aspect_ratio = av_mul_q((AVRational){outlink->h * inlink->w, outlink->w * inlink->h}, inlink->sample_aspect_ratio);
    } else
        outlink->sample_aspect_ratio = inlink->sample_aspect_ratio;

    hwframes_out->width = outlink->w;
    hwframes_out->height = outlink->h;

    err = av_hwframe_ctx_init(ctx->hwframes_out_ref);
    if (err < 0)
        return err;

    outl->hw_frames_ctx = av_buffer_ref(ctx->hwframes_out_ref);
    if (!outl->hw_frames_ctx) {
        return AVERROR(ENOMEM);
    }
    return 0;
}

void amf_free_amfsurface(void *opaque, uint8_t *data)
{
    AMFSurface *surface = (AMFSurface*)data;
    surface->pVtbl->Release(surface);
}

AVFrame *amf_amfsurface_to_avframe(AVFilterContext *avctx, AMFSurface* pSurface)
{
    AVFrame *frame = av_frame_alloc();
    AMFFilterContext  *ctx = avctx->priv;

    if (!frame)
        return NULL;

    if (ctx->hwframes_out_ref) {
        AVHWFramesContext *hwframes_out = (AVHWFramesContext *)ctx->hwframes_out_ref->data;
        if (hwframes_out->format == AV_PIX_FMT_AMF_SURFACE) {
            int ret = av_hwframe_get_buffer(ctx->hwframes_out_ref, frame, 0);
            if (ret < 0) {
                av_log(avctx, AV_LOG_ERROR, "Get hw frame failed.\n");
                av_frame_free(&frame);
                return NULL;
            }
            frame->data[0] = (uint8_t *)pSurface;
            frame->buf[1] = av_buffer_create((uint8_t *)pSurface, sizeof(AMFSurface),
                                            amf_free_amfsurface,
                                            (void*)avctx,
                                            AV_BUFFER_FLAG_READONLY);
        } else { // FIXME: add processing of other hw formats
            av_log(ctx, AV_LOG_ERROR, "Unknown pixel format\n");
            return NULL;
        }
    } else {

        switch (pSurface->pVtbl->GetMemoryType(pSurface))
        {
    #if CONFIG_D3D11VA
            case AMF_MEMORY_DX11:
            {
                AMFPlane *plane0 = pSurface->pVtbl->GetPlaneAt(pSurface, 0);
                frame->data[0] = plane0->pVtbl->GetNative(plane0);
                frame->data[1] = (uint8_t*)(intptr_t)0;

                frame->buf[0] = av_buffer_create(NULL,
                                        0,
                                        amf_free_amfsurface,
                                        pSurface,
                                        AV_BUFFER_FLAG_READONLY);
            }
            break;
    #endif
    #if CONFIG_DXVA2
            case AMF_MEMORY_DX9:
            {
                AMFPlane *plane0 = pSurface->pVtbl->GetPlaneAt(pSurface, 0);
                frame->data[3] = plane0->pVtbl->GetNative(plane0);

                frame->buf[0] = av_buffer_create(NULL,
                                        0,
                                        amf_free_amfsurface,
                                        pSurface,
                                        AV_BUFFER_FLAG_READONLY);
            }
            break;
    #endif
        default:
            {
                av_log(avctx, AV_LOG_ERROR, "Unsupported memory type : %d\n", pSurface->pVtbl->GetMemoryType(pSurface));
                return NULL;
            }
        }
    }

    return frame;
}

int amf_avframe_to_amfsurface(AVFilterContext *avctx, const AVFrame *frame, AMFSurface** ppSurface)
{
    AMFVariantStruct var = { 0 };
    AMFFilterContext *ctx = avctx->priv;
    AMFSurface *surface;
    AMF_RESULT  res;
    int hw_surface = 0;

    switch (frame->format) {
#if CONFIG_D3D11VA
    case AV_PIX_FMT_D3D11:
        {
            static const GUID AMFTextureArrayIndexGUID = { 0x28115527, 0xe7c3, 0x4b66, { 0x99, 0xd3, 0x4f, 0x2a, 0xe6, 0xb4, 0x7f, 0xaf } };
            ID3D11Texture2D *texture = (ID3D11Texture2D*)frame->data[0]; // actual texture
            int index = (intptr_t)frame->data[1]; // index is a slice in texture array is - set to tell AMF which slice to use
            texture->lpVtbl->SetPrivateData(texture, &AMFTextureArrayIndexGUID, sizeof(index), &index);

            res = ctx->amf_device_ctx->context->pVtbl->CreateSurfaceFromDX11Native(ctx->amf_device_ctx->context, texture, &surface, NULL); // wrap to AMF surface
            AMF_RETURN_IF_FALSE(avctx, res == AMF_OK, AVERROR(ENOMEM), "CreateSurfaceFromDX11Native() failed  with error %d\n", res);
            hw_surface = 1;
        }
        break;
#endif
    case AV_PIX_FMT_AMF_SURFACE:
        {
            surface = (AMFSurface*)frame->data[0]; // actual surface
            surface->pVtbl->Acquire(surface); // returned surface has to be to be ref++
            hw_surface = 1;
        }
        break;

#if CONFIG_DXVA2
    case AV_PIX_FMT_DXVA2_VLD:
        {
            IDirect3DSurface9 *texture = (IDirect3DSurface9 *)frame->data[3]; // actual texture

            res = ctx->amf_device_ctx->context->pVtbl->CreateSurfaceFromDX9Native(ctx->amf_device_ctx->context, texture, &surface, NULL); // wrap to AMF surface
            AMF_RETURN_IF_FALSE(avctx, res == AMF_OK, AVERROR(ENOMEM), "CreateSurfaceFromDX9Native() failed  with error %d\n", res);
            hw_surface = 1;
        }
        break;
#endif
    default:
        {
            AMF_SURFACE_FORMAT amf_fmt = av_av_to_amf_format(frame->format);
            res = ctx->amf_device_ctx->context->pVtbl->AllocSurface(ctx->amf_device_ctx->context, AMF_MEMORY_HOST, amf_fmt, frame->width, frame->height, &surface);
            AMF_RETURN_IF_FALSE(avctx, res == AMF_OK, AVERROR(ENOMEM), "AllocSurface() failed  with error %d\n", res);
            amf_copy_surface(avctx, frame, surface);
        }
        break;
    }

    // If AMFSurface comes from other AMF components, it may have various
    // properties already set. These properties can be used by other AMF
    // components to perform their tasks. In the context of the AMF video
    // filter, that other component could be an AMFVideoConverter. By default,
    // AMFVideoConverter will use HDR related properties assigned to a surface
    // by an AMFDecoder. If frames (surfaces) originated from any other source,
    // i.e. from hevcdec, assign those properties from avframe; do not
    // overwrite these properties if they already have a value.
    res = surface->pVtbl->GetProperty(surface, AMF_VIDEO_DECODER_COLOR_TRANSFER_CHARACTERISTIC, &var);

    if (res == AMF_NOT_FOUND && frame->color_trc != AVCOL_TRC_UNSPECIFIED)
        // Note: as of now(Feb 2026), most AV and AMF enums are interchangeable.
        // TBD: can enums change their values in the future?
        // For better future-proofing it's better to have dedicated
        // enum mapping functions.
        AMF_ASSIGN_PROPERTY_INT64(res, surface, AMF_VIDEO_DECODER_COLOR_TRANSFER_CHARACTERISTIC, frame->color_trc);

    res = surface->pVtbl->GetProperty(surface, AMF_VIDEO_DECODER_COLOR_PRIMARIES, &var);
    if (res == AMF_NOT_FOUND && frame->color_primaries != AVCOL_PRI_UNSPECIFIED)
        AMF_ASSIGN_PROPERTY_INT64(res, surface, AMF_VIDEO_DECODER_COLOR_PRIMARIES, frame->color_primaries);

    res = surface->pVtbl->GetProperty(surface, AMF_VIDEO_DECODER_COLOR_RANGE, &var);
    if (res == AMF_NOT_FOUND && frame->color_range != AVCOL_RANGE_UNSPECIFIED)
        AMF_ASSIGN_PROPERTY_INT64(res, surface, AMF_VIDEO_DECODER_COLOR_RANGE, frame->color_range);

    // Color range for older drivers
    if (frame->color_range == AVCOL_RANGE_JPEG) {
        AMF_ASSIGN_PROPERTY_BOOL(res, surface, AMF_VIDEO_DECODER_FULL_RANGE_COLOR, 1);
    } else if (frame->color_range != AVCOL_RANGE_UNSPECIFIED)
        AMF_ASSIGN_PROPERTY_BOOL(res, surface, AMF_VIDEO_DECODER_FULL_RANGE_COLOR, 0);

    // Color profile for newer drivers
    res = surface->pVtbl->GetProperty(surface, AMF_VIDEO_DECODER_COLOR_PROFILE, &var);
    if (res == AMF_NOT_FOUND && frame->color_range != AVCOL_RANGE_UNSPECIFIED && frame->colorspace != AVCOL_SPC_UNSPECIFIED) {
        amf_int64 color_profile = color_profile = av_amf_get_color_profile(frame->color_range, frame->colorspace);

        if (color_profile != AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN)
            AMF_ASSIGN_PROPERTY_INT64(res, surface, AMF_VIDEO_DECODER_COLOR_PROFILE, color_profile);
    }

    if (ctx->in_trc == AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084 && (ctx->master_display || ctx->light_meta)) {
        AMFBuffer *hdrmeta_buffer = NULL;
        res = ctx->amf_device_ctx->context->pVtbl->AllocBuffer(ctx->amf_device_ctx->context, AMF_MEMORY_HOST, sizeof(AMFHDRMetadata), &hdrmeta_buffer);
        if (res == AMF_OK) {
            AMFHDRMetadata *hdrmeta = (AMFHDRMetadata*)hdrmeta_buffer->pVtbl->GetNative(hdrmeta_buffer);

            av_amf_display_mastering_meta_to_hdrmeta(ctx->master_display, hdrmeta);
            av_amf_light_metadata_to_hdrmeta(ctx->light_meta, hdrmeta);
            AMF_ASSIGN_PROPERTY_INTERFACE(res, surface, AMF_VIDEO_DECODER_HDR_METADATA, hdrmeta_buffer);
        }
    } else if (frame->color_trc == AVCOL_TRC_SMPTE2084) {
        res = surface->pVtbl->GetProperty(surface, AMF_VIDEO_DECODER_HDR_METADATA, &var);
        if (res == AMF_NOT_FOUND) {
            AMFBuffer *hdrmeta_buffer = NULL;
            res = ctx->amf_device_ctx->context->pVtbl->AllocBuffer(ctx->amf_device_ctx->context, AMF_MEMORY_HOST, sizeof(AMFHDRMetadata), &hdrmeta_buffer);
            if (res == AMF_OK) {
                AMFHDRMetadata *hdrmeta = (AMFHDRMetadata*)hdrmeta_buffer->pVtbl->GetNative(hdrmeta_buffer);

                if (av_amf_extract_hdr_metadata(frame, hdrmeta) == 0)
                    AMF_ASSIGN_PROPERTY_INTERFACE(res, surface, AMF_VIDEO_DECODER_HDR_METADATA, hdrmeta_buffer);
                hdrmeta_buffer->pVtbl->Release(hdrmeta_buffer);
            }
        }
    }

    if (frame->crop_left || frame->crop_right || frame->crop_top || frame->crop_bottom) {
        size_t crop_x = frame->crop_left;
        size_t crop_y = frame->crop_top;
        size_t crop_w = frame->width - (frame->crop_left + frame->crop_right);
        size_t crop_h = frame->height - (frame->crop_top + frame->crop_bottom);
        AVFilterLink *outlink = avctx->outputs[0];
        if (crop_x || crop_y) {
            if (crop_w == outlink->w && crop_h == outlink->h) {
                AMFData *cropped_buffer = NULL;
                res = surface->pVtbl->Duplicate(surface, surface->pVtbl->GetMemoryType(surface), &cropped_buffer);
                AMF_RETURN_IF_FALSE(avctx, res == AMF_OK, AVERROR(ENOMEM), "Duplicate() failed  with error %d\n", res);
                surface->pVtbl->Release(surface);
                surface = (AMFSurface*)cropped_buffer;
            }
            else
                surface->pVtbl->SetCrop(surface, (amf_int32)crop_x, (amf_int32)crop_y, (amf_int32)crop_w, (amf_int32)crop_h);
        }
        else
            surface->pVtbl->SetCrop(surface, (amf_int32)crop_x, (amf_int32)crop_y, (amf_int32)crop_w, (amf_int32)crop_h);
    }
    else if (hw_surface) {
        // input HW surfaces can be vertically aligned by 16; tell AMF the real size
        surface->pVtbl->SetCrop(surface, 0, 0, frame->width, frame->height);
    }

    surface->pVtbl->SetPts(surface, frame->pts);
#if VF_VPP_AMF_D3D11_HWACCEL
    AMF_FRAME_TYPE frame_type = surface->pVtbl->GetFrameType(surface);
    if (frame_type == AMF_FRAME_UNKNOWN) {
        if (frame->flags & AV_FRAME_FLAG_INTERLACED) {
            if (frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) {
                surface->pVtbl->SetFrameType(surface, AMF_FRAME_INTERLEAVED_EVEN_FIRST);
            } else {
                surface->pVtbl->SetFrameType(surface, AMF_FRAME_INTERLEAVED_ODD_FIRST);
            }
        } else {
            surface->pVtbl->SetFrameType(surface, AMF_FRAME_PROGRESSIVE);
        }
    }
#endif // VF_VPP_AMF_D3D11_HWACCEL
    *ppSurface = surface;
    return 0;
}
