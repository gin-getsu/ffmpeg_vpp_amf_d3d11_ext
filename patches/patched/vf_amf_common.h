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

#ifndef AVFILTER_AMF_COMMON_H
#define AVFILTER_AMF_COMMON_H

#include "avfilter.h"

#include "AMF/core/Surface.h"
#include "AMF/components/Component.h"
#include "libavutil/hwcontext_amf.h"
#include "libavutil/mastering_display_metadata.h"

#ifdef VF_VPP_AMF_D3D11_HWACCEL
#undef VF_VPP_AMF_D3D11_HWACCEL
#define VF_VPP_AMF_D3D11_HWACCEL 1
#endif

#if VF_VPP_AMF_D3D11_HWACCEL
#include <d3d11.h>

enum {
    VPP_AMF_DEINT_OFF = 0,
    VPP_AMF_DEINT_FAST,
    VPP_AMF_DEINT_ADAPTIVE,
};

enum {
    VPP_AMF_SCALE_ADJUST_STRETCH = 0,
    VPP_AMF_SCALE_ADJUST_PADDING,
};

#define VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX 16
#endif

typedef struct AMFFilterContext {
    const AVClass *class;

    int width, height;
    enum AVPixelFormat format;
    int scale_type;
    int in_color_range;
    int in_primaries;
    int in_trc;
    int color_profile;
    int out_color_range;
    int out_primaries;
    int out_trc;
    int fill;
    int fill_color;
    int keep_ratio;

    AVMasteringDisplayMetadata *master_display;
    AVContentLightMetadata     *light_meta;

    // HQScaler properties
    int algorithm;
    float sharpness;

    char *w_expr;
    char *h_expr;
    char *format_str;
    int force_original_aspect_ratio;
    int force_divisible_by;
    int reset_sar;

    AMFComponent        *component;
    AVBufferRef         *amf_device_ref;

    AVBufferRef         *hwframes_in_ref;
    AVBufferRef         *hwframes_out_ref;
    AVBufferRef         *hwdevice_ref;

    AVAMFDeviceContext  *amf_device_ctx;
    int                  local_context;

#if VF_VPP_AMF_D3D11_HWACCEL
    /* ---------------- D3D11 VideoProcessor pre-process ---------------- */

    /* D3D11 core */
    ID3D11Device               *d3d11_device;
    ID3D11DeviceContext        *d3d11_context;

    /* D3D11 video interfaces */
    ID3D11VideoDevice          *d3d11_video_device;
    ID3D11VideoContext         *d3d11_video_context;
    ID3D11VideoProcessorEnumerator *d3d11_vp_enum;
    ID3D11VideoProcessor       *d3d11_vp;

    /* D3D11 video processor interfaces */
    ID3D11Texture2D            *d3d11_tex_in[VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX];
    ID3D11Texture2D            *d3d11_tex_out;
    ID3D11VideoProcessorInputView  *d3d11_in_view[VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX];
    ID3D11VideoProcessorOutputView *d3d11_out_view;

    /* Deinterlace mode */
    int                         d3d11_deint;              /* 0=off, 1=fast, 2=adaptive */
    int                         d3d11_deint_frame_format; /* 0=auto, 1=tff, 2=bff */

    int                         deint_frames_min;
    int                         deint_frames_max;
    int                         deint_frames_cur;
    int                         deint_frames_new;
    int                         deint_frames_que;
    int                         deint_frames;
    D3D11_VIDEO_FRAME_FORMAT    deint_frames_format[VF_VPP_AMF_D3D11_DEINT_FRAMES_MAX];
    AVFrame                    *deint_frame_last;
    int                         deint_frame_eof;

    /* Crop parameters */
    int                         crop_x;
    int                         crop_y;
    int                         crop_w;
    int                         crop_h;

    /* Derived sizes */
    int                         crop_w_eff;   /* cw */
    int                         crop_h_eff;   /* ch */

    /* frame input SAR */
    DXGI_RATIONAL               dxgi_in_sar;

    /* centor of target */
    int                         dest_x;       /* dx */
    int                         dest_y;       /* dy */

    /* target sizes */
    int                         target_w;     /* dw */
    int                         target_h;     /* dh */

    /* scale adjust */
    int                         scale_adjust;

    /* D3D11 VP state */
    int                         d3d11_log_info;
#endif // VF_VPP_AMF_D3D11_HWACCEL
} AMFFilterContext;

int amf_filter_init(AVFilterContext *avctx);
void amf_filter_uninit(AVFilterContext *avctx);
int amf_init_filter_config(AVFilterLink *outlink, enum AVPixelFormat *in_format);
int amf_copy_surface(AVFilterContext *avctx, const AVFrame *frame, AMFSurface* surface);
void amf_free_amfsurface(void *opaque, uint8_t *data);
AVFrame *amf_amfsurface_to_avframe(AVFilterContext *avctx, AMFSurface* pSurface);
int amf_avframe_to_amfsurface(AVFilterContext *avctx, const AVFrame *frame, AMFSurface** ppSurface);
int amf_setup_input_output_formats(AVFilterContext *avctx, const enum AVPixelFormat *input_pix_fmts, const enum AVPixelFormat *output_pix_fmts);
int amf_filter_filter_frame(AVFilterLink *inlink, AVFrame *in);

#if VF_VPP_AMF_D3D11_HWACCEL
int amf_d3d11_preprocess_required(AVFilterContext *avctx);
int amf_activate(AVFilterContext *avctx);
#endif // VF_VPP_AMF_D3D11_HWACCEL

#endif /* AVFILTER_AMF_COMMON_H */
