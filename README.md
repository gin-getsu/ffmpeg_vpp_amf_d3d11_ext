# ffmpeg_vpp_amf_d3d11_ext  
Extended vpp_amf filter with D3D11-based deinterlace and crop for ffmpeg

## 1. Overview of This Extension

- Adds **D3D11-based deinterlace and crop functionality** to ffmpeg’s `vpp_amf` filter  
- Includes three patch files for:  
  - `vf_amf_common.c`  
  - `vf_amf_common.h`  
  - `vf_vpp_amf.c`  
- Includes a Makefile for building on **Windows (MSys2 + mingw64)**  
- When `deint` or `crop(cx,cy,cw,ch)` is specified together with the original `w` / `h` options of `vpp_amf`,  
  → the filter performs **centering and padding** using `w × h` as the canvas

---

## 2. Basic Command Examples

### 2-1. When the AMF decoder (`-hwaccel amf`) supports the codec

```bash
ffmpeg -hwaccel amf -i "input.m2ts" \
  -c:v hevc_amf \
  -vf "vpp_amf=w=1920:h=720:deint=fast:deint_frame_format=tff:format=nv12:cx=0:cy=139:cw=1920:ch=802, setsar=1" \
  -c:a copy -f mp4 "output.mp4"
```

### 2-2. When the AMF decoder does NOT support the codec

```bash
ffmpeg -i "input.m2ts" \
  -c:v hevc_amf \
  -vf "format=nv12, vpp_amf=w=1920:h=720:deint=fast:deint_frame_format=tff:format=nv12:cx=0:cy=139:cw=1920:ch=802, setsar=1" \
  -c:a copy -f mp4 "output.mp4"
```

---

## 3. Notes on Options (Based on AMD Radeon RX 7600 Behavior)

### 3-1. Hardware decoding with AMF may cause playback stutter  
- Occurs when the video has small resolution and AMF’s automatic frame-format detection mixes **tff / bff / progressive**  
- In such cases, disable hardware decoding and specify `format`, `deint`, and `deint_frame_format` in the filter (see 2-2)

---

### 3-2. `deint=adaptive` may produce jagged frames  
- Very common in 2D animation  
- Occasional in 3D animation with small source resolution  
- Rare in live-action  
→ Use **`deint=fast` + `deint_frame_format`** for stable results

---

### 3-3. When AMF does not support the codec  
- Decoding falls back to CPU  
- The vf input format becomes the same as the input file  
- This filter expects `nv12` or `p010`, so it fails  
→ Specify `format=nv12` explicitly (see 2-2)

**AMF decoder support as of 2026.1:**

```
DEFINE_AMF_DECODER(h264, H264, ...)
DEFINE_AMF_DECODER(hevc, HEVC, ...)
DEFINE_AMF_DECODER(vp9,  VP9,  ...)
DEFINE_AMF_DECODER(av1,  AV1,  ...)
```

---

### 3-4. Automatic per-frame detection of tff/bff/progressive  
- Often increases jitter depending on the source  
→ Specify `deint` and `deint_frame_format` together

---

### 3-5. Some videos are detected as tff/bff by ffprobe but are actually progressive  
- Applying deinterlace based on incorrect detection causes severe jitter  
→ Use the detection command below to confirm before converting

---

## 4. Useful Commands for Inspecting Source Videos

### 4-1. Progressive / Interlaced detection  
```bash
ffmpeg -i test.vob -filter:v idet -frames:v 300 -an -f null -
```

- Check the Multi-frame detection output  
- If tff/bff counts are significant → treat as interlaced

---

### 4-2. Check codec  
```bash
ffprobe -select_streams v:0 -show_entries stream=codec_name test.vob
```

---

### 4-3. Crop size detection  
```bash
ffmpeg -i "test.mp4" -vf cropdetect -f null -
```

- Detects black borders every few frames  
- If borders appear/disappear depending on scene, run multiple times and observe the values

---

## 5. Project Structure

```
ffmpeg_vpp_amf_d3d11_ext
+- patches
|  +- original
|  |  +- vf_amf_common.c
|  |  +- vf_amf_common.h
|  |  +- vf_vpp_amf.c
|  +- vf_amf_common_c.patch
|  +- vf_amf_common_h.patch
|  +- vf_vpp_amf_c.patch
+- Makefile
```

---

## 6. Makefile Overview

### 6-1. Preparation  
Edit the following line:

```
ROOT := /d/Data/ffmpeg_vpp_amf_d3d11_ext
INSTALL_TO := /d/utils/ffmpeg-self
```

### 6-2. Build  
```bash
cd ffmpeg_vpp_amf_d3d11_ext
make
```

### 6-3. What the Makefile does  
- Installs or builds required libraries (mingw packages or upstream sources)  
- Runs ffmpeg’s `configure`  
- Applies patches  
- Builds ffmpeg  
- Verifies that the resulting binaries **do not depend on MSys/mingw64 runtime DLLs**

---

## 7. Disclaimer / Support Policy

- This repository is provided **“as is.”**  
- Bug reports, questions, and feature requests will **not** be handled.  
- No contact information is provided.  
- Use, modify, and redistribute at your own risk.

---

## 8. License

This patch follows the ffmpeg project’s license (GPL/LGPL).  
Refer to the ffmpeg source tree for full license details.