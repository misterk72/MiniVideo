/*!
 * COPYRIGHT (C) 2012 Emeric Grange - All Rights Reserved
 *
 * This file is part of MiniVideo.
 *
 * MiniVideo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MiniVideo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MiniVideo.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \file      avcodecs.h
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2012
 */

#ifndef AV_CODECS_H
#define AV_CODECS_H

#include "typedef.h"
#include "avutils.h"
#include "../build/minivideo_Export.h"
/* ************************************************************************** */

//! Container file formats
typedef enum ContainerFormat_e
{
    CONTAINER_UNKNOWN = 0,

    // General purpose containers //////////////////////////////////////////////

    CONTAINER_AVI     =  1, //!< AVI "Audio Video Interleave" (.avi, ...)
    CONTAINER_ASF     =  2, //!< ASF "Advanced Systems Format" (.asf, .wma, .wmv, ...)
    CONTAINER_MKV     =  3, //!< Matroska (.mkv, .mka, .webm)
    CONTAINER_MP4     =  4, //!< ISOM "ISO Base Media" format (.mov, .mp4, .3gp, .f4v, ...)
    CONTAINER_MPEG_PS =  5, //!< MPEG "Program Stream" (.mpg, .vob, ...)
    CONTAINER_MPEG_TS =  6, //!< MPEG "Transport Stream" (.ts, .mts, .m2ts, ...)
    CONTAINER_MPEG_MT =  7, //!< MPEG "Media Transport" (.mt, .mmt)
    CONTAINER_MXF     =  8, //!< MXF "Material eXchange Format" (.mxf)
    CONTAINER_FLV     =  9, //!< SWF "Small Web Format" (.flv)
    CONTAINER_OGG     = 10, //!< OGG (.ogg, .ogv, .oga, ...)
    CONTAINER_RM      = 11, //!< RealMedia (.rm, .rmvb)

    // Audio containers ////////////////////////////////////////////////////////

    CONTAINER_FLAC    = 12, //!< FLAC "Free Lossless Audio Codec" (.flac)
    CONTAINER_WAVE    = 13, //!< WAVE "Waveform Audio File Format" (.wav)

    // ES formats (not containers!) ////////////////////////////////////////////

    CONTAINER_ES      = 16, //!< Undefined "Elementary Stream"
    CONTAINER_ES_AAC  = 17, //!< AAC "Elementary Stream"
    CONTAINER_ES_AC3  = 18, //!< AC3 "Elementary Stream"
    CONTAINER_ES_MP3  = 19, //!< MP3 "Elementary Stream"

} ContainerFormat_e;

//! Audio and Video compression standards (codecs)
typedef enum AVCodec_e
{
    CODEC_UNKNOWN     = 0,

    // Audio codecs ////////////////////////////////////////////////////////////

    CODEC_MPEG_L1     =  1, //!< MP1, or MPEG 1/2 Audio Layer I
    CODEC_MPEG_L2     =  2, //!< MP2, or MPEG 1/2 Audio Layer II
    CODEC_MPEG_L3     =  3, //!< MP3, or MPEG 1/2 Audio Layer III
    CODEC_AAC         =  4, //!< Advanced Audio Coding, MPEG-2 Part 7 and MPEG-4 Part 3
    CODEC_AAC_HE      =  5, //!< "High Efficiency" AAC
    CODEC_AAC_LD      =  6, //!< "Low Delay" AAC
    CODEC_MPEG4_ALS   =  7, //!<
    CODEC_MPEG4_CELP  =  8, //!<
    CODEC_MPEG4_DST   =  9, //!<
    CODEC_MPEG4_HVXC  = 10, //!<
    CODEC_MPEG4_SLS   = 11, //!<
    CODEC_MPEGH_3D_AUDIO    = 12, //!<

    CODEC_SPEEX       = 32, //!< Xiph Speex
    CODEC_VORBIS      = 33, //!< Xiph Ogg Vorbis
    CODEC_OPUS        = 34, //!< Xiph Opus

    CODEC_AC3         = 35, //!< Dolby Digital, or AC-3, or A/52A
    //Dolby Digital EX
    //Dolby Digital Live
    CODEC_EAC3        = 38, //!< Dolby Digital Plus, or Enhanced AC-3, or A/52B
    //Dolby TrueHD
    CODEC_AC4         = 40, //!< Dolby AC-4

    CODEC_DTS         = 42, //!< "Dedicated To Sound"
    CODEC_DTS_HD      = 43,
    CODEC_DTS_X       = 44,

    CODEC_WMA         = 49, //!< Windows Media Audio
    CODEC_MPC         = 50, //!< Musepack

    // Uncommon audio codecs ///////////////////////////////////////////////////

    // Lossless audio codecs ///////////////////////////////////////////////////

    CODEC_APE         = 64, //!< Monkey's Audio
    CODEC_FLAC        = 65, //!< Free Lossless Audio Codec
    CODEC_ALAC        = 66, //!< Apple Lossless Audio Codec

    // Uncompressed audio //////////////////////////////////////////////////////

    CODEC_LPCM        = 128, //!< Linear pulse-code modulation
    CODEC_LogPCM      = 129, //!< Logarithmic pulse-code modulation
    CODEC_DPCM        = 130, //!< Differential pulse-code modulation
    CODEC_ADPCM       = 131, //!< Adaptative differential pulse-code modulation
    CODEC_PDM         = 132, //!< Pulse-density modulation

    // Video codecs ////////////////////////////////////////////////////////////

    CODEC_MPEG1       = 256, //!< MPEG-1 Part 2 (ISO/IEC 11172-2)
    CODEC_H261        = 257, //!< H.261
    CODEC_MPEG2       = 258, //!< H.262 or MPEG-2 Part 2 (ISO/IEC 13818-2)
    CODEC_MPEG4_ASP   = 259, //!< MPEG-4 Part 2 "ASP", XVID is a popular implementation
    CODEC_MSMPEG4     = 260, //!< MPEG-4 Part 2 "ASP" implementation from Microsoft (note: 3 different versions exist), NOT compatible with regular MPEG-4 ASP. Used in divx <= 3.
    CODEC_H263        = 261, //!< H.263 (and its numerous variants)
    CODEC_H264        = 262, //!< H.264 or MPEG-4 Part 10 "AVC"
    CODEC_H265        = 263, //!< H.265 or MPEG-H Part 2 "HEVC" or (ISO/IEC 23008-2)

    CODEC_WMV7        = 264, //!< Windows Media Video 7
    CODEC_WMV8        = 265, //!< Windows Media Video 8
    CODEC_WMV9        = 266, //!< Windows Media Video 9
    CODEC_WMSCR       = 267, //!< Windows Media Screen (7-9)
    CODEC_WMP         = 268, //!< Windows Media Picture

    CODEC_VP3         = 269, //!< On2 VP3 Video
    CODEC_VP4         = 270, //!< On2 VP4 Video / Xiph Ogg Theora
    CODEC_VP5         = 271, //!< On2 VP5 Video
    CODEC_VP6         = 272, //!< On2 VP6 Video
    CODEC_VP7         = 273, //!< On2 VP7 Video
    CODEC_VP8         = 274, //!< Google VP8
    CODEC_VP9         = 275, //!< Google VP9
    CODEC_VP10        = 276, //!< Google VP10

    CODEC_DAALA       = 277, //!< Xiph Daala

    CODEC_VC1         = 278, //!< VC-1 or Windows Media Video 9
    CODEC_VC2         = 279, //!< VC-2 or Dirac is an open and royalty-free video compression format developed by BBC Research
    CODEC_VC3         = 280, //!< VC-3, Avid DNxHD is a popular implementation
    CODEC_VC5         = 281, //!< VC-5 or CineForm

    CODEC_PRORES_4444_XQ    = 283, //!< Apple ProRes 4444 (XQ)
    CODEC_PRORES_4444       = 282, //!< Apple ProRes 4444
    CODEC_PRORES_422_HQ     = 284, //!< Apple ProRes 422 (HQ)
    CODEC_PRORES_422        = 285, //!< Apple ProRes 422
    CODEC_PRORES_422_PROXY  = 286, //!< Apple ProRes 422 (Proxy)
    CODEC_PRORES_422_LT     = 287, //!< Apple ProRes 422 (LT)

    // "Uncommon" video codecs
    CODEC_CINEPAK     = 288, //!< Cinepak
    CODEC_SVQ1        = 289, //!< Sorenson Video 1
    CODEC_SVQ3        = 290, //!< Sorenson Video 3
    CODEC_IV31        = 291, //!< Intel Indeo Video 3
    CODEC_IV41        = 292, //!< Intel Indeo Video 4
    CODEC_IV50        = 293, //!< Intel Indeo Video 5
    CODEC_icod        = 294, //!< Apple Intermediate Codec
    CODEC_rpza        = 295, //!< Apple Video / "road pizza"

    // Uncompressed video //////////////////////////////////////////////////////

    // Subtitles codecs ////////////////////////////////////////////////////////

    CODEC_SRT         = 512, //!< SubRip (.srt)
    CODEC_SSA         = 513, //!< SubStation Alpha (.ssa)
    CODEC_ASS         = 514, //!< Advanced SubStation Alpha (.ass)

} AVCodec_e;

//! Picture file formats
typedef enum PictureFormat_e
{
    PICTURE_UNKNOWN   = 0,

    PICTURE_BMP       = 1,
    PICTURE_JPG       = 2,
    PICTURE_PNG       = 3,
    PICTURE_WEBP      = 4,
    PICTURE_TGA       = 5,

    PICTURE_YUV444    = 16,  //!< Planar YCbCr file without subsampling
    PICTURE_YUV420    = 17   //!< Planar YCbCr file with 4:2:0 subsampling

} PictureFormat_e;

/* ************************************************************************** */

minivideo_EXPORT const char *getContainerString(ContainerFormat_e container, bool long_description);
minivideo_EXPORT const char *getCodecString(StreamType_e type, AVCodec_e codec, bool long_description);
minivideo_EXPORT const char *getPictureString(PictureFormat_e picture, bool long_description);

/* ************************************************************************** */
#endif // AV_CODECS_H
