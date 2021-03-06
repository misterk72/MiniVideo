/*!
 * COPYRIGHT (C) 2011 Emeric Grange - All Rights Reserved
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
 * \file      mp4.c
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2011
 */

// minivideo headers
#include "mp4.h"
#include "mp4_struct.h"
#include "../../fourcc.h"
#include "../../typedef.h"
#include "../../bitstream.h"
#include "../../bitstream_utils.h"
#include "../../minitraces.h"

// C standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* ************************************************************************** */

static int parse_mvhd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4);

static int parse_iods(Bitstream_t *bitstr, Mp4Box_t *box_header);
static int parse_trak(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4);
static int parse_tkhd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);

static int parse_edts(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
    static int parse_elst(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);

static int parse_mdia(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
static int parse_mdhd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
        static int parse_hdlr(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
        static int parse_minf(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
            static int parse_stbl(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_stsd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_avcC(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_btrt(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_clap(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_colr(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_fiel(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_gama(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                    static int parse_pasp(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_stts(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_ctts(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_stss(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_stsc(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_stsz(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);
                static int parse_stco(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track);

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \brief Jumpy protect your parsing - MP4 edition.
 * \param parent: The box containing the current box we're in.
 * \param current: The current box we're in.
 *
 * 'Jumpy' is in charge of checking your position into the stream after your
 * parser finish parsing a box / list / chunk / element, never leaving you
 * stranded  in the middle of nowhere with no easy way to get back on track.
 * It will check available informations to known if the current element has been
 * fully parsed, and if not perform a jump (or even a rewind) to the next known
 * element.
 */
int jumpy_mp4(Bitstream_t *bitstr, Mp4Box_t *parent, Mp4Box_t *current)
{
    int retcode = FAILURE;
    int64_t current_pos = bitstream_get_absolute_byte_offset(bitstr);

    if (current_pos != current->offset_end)
    {
        int64_t file_size = bitstream_get_full_size(bitstr);
        int64_t offset_end = current->offset_end;

        // If the current box have a parent, and its offset_end is 'valid' (not past file size)
        if (parent && parent->offset_end < file_size)
        {
            // If the current offset_end is past its parent offset_end, its probably
            // broken, and so we will use the one from its parent
            if (offset_end > parent->offset_end)
            {
                offset_end = parent->offset_end;
            }
        }
        else // no parent (or parent with broken offset_end)
        {
            // If the current offset_end is past file size
            if (offset_end > file_size)
                offset_end = file_size;
        }

        // If the offset_end is past the last byte of the file, we do not need to jump
        // The parser will pick that fact and finish up
        if (offset_end >= file_size)
        {
            bitstr->bitstream_offset = file_size;
            return SUCCESS;
        }

        // Now, do we need to go forward or backward to reach our goal?
        // Then, can we move in our current buffer or do we need to reload a new one?
        if (current_pos < offset_end)
        {
            int64_t jump = offset_end - current_pos;

            if (jump < (UINT_MAX/8))
                retcode = skip_bits(bitstr, (unsigned int)(jump*8));
            else
                retcode = bitstream_goto_offset(bitstr, offset_end);
        }
        else
        {
            int64_t rewind = current_pos - offset_end;

            if (rewind > 0)
            {
                if (rewind > (UINT_MAX/8))
                    retcode = rewind_bits(bitstr, (unsigned int)(rewind*8));
                else
                    retcode = bitstream_goto_offset(bitstr, offset_end);
            }
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Convert a videoTrack_t structure into a bitstreamMap_t.
 * \param *media: A pointer to a MediaFile_t structure, containing every informations available about the current media file.
 * \param *track: A pointer to the mp4 track structure we want to extract data from.
 * \param idr_only Set to true if we only want to extract IDR samples.
 *
 * - Use STSZ box content to get back all samples.
 * - Use STSS box content to get back IDR samples only.
 */
static bool convertTrack(MediaFile_t *media, Mp4_t *mp4, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "convertTrack()\n" CLR_RESET);
    bool retcode = SUCCESS;
    BitstreamMap_t *map = NULL;

    if (media == NULL || track == NULL)
    {
        TRACE_ERROR(MP4, "Cannot access audio or video tracks from the MP4 parser!\n");
        retcode = FAILURE;
    }

    // Select and init a bitstream map (A or V)
    ////////////////////////////////////////////////////////////////////////////

    if (retcode == SUCCESS)
    {
        if (track->handlerType == HANDLER_AUDIO)
        {
            retcode = init_bitstream_map(&media->tracks_audio[media->tracks_audio_count], track->stsz_sample_count);
            if (retcode == SUCCESS)
            {
                map = media->tracks_audio[media->tracks_audio_count];
                media->tracks_audio_count++;
            }
        }
        else if (track->handlerType == HANDLER_VIDEO)
        {
            retcode = init_bitstream_map(&media->tracks_video[media->tracks_video_count], track->stsz_sample_count + track->sps_count + track->pps_count);
            if (retcode == SUCCESS)
            {
                map = media->tracks_video[media->tracks_video_count];
                media->tracks_video_count++;
            }
        }
        else if (track->handlerType == HANDLER_SUBT ||
                 track->handlerType == HANDLER_SBTL ||
                 track->handlerType == HANDLER_TEXT)
        {
            retcode = init_bitstream_map(&media->tracks_subt[media->tracks_subtitles_count], track->stsz_sample_count);
            if (retcode == SUCCESS)
            {
                map = media->tracks_subt[media->tracks_subtitles_count];
                media->tracks_subtitles_count++;
            }
        }
        else
        {
            TRACE_WARNING(MP4, "Not sure we can build bitstream_map for other track types! (track #%u handlerType: %u)\n", track->id, track->handlerType);

            retcode = init_bitstream_map(&media->tracks_others[media->tracks_others_count], track->stsz_sample_count);
            if (retcode == SUCCESS)
            {
                map = media->tracks_others[media->tracks_others_count];
                media->tracks_others_count++;
            }
        }
    }

    // Build bitstream map
    ////////////////////////////////////////////////////////////////////////////

    if (retcode == SUCCESS && map)
    {
        unsigned int i = 0, j = 0;

        map->stream_fcc = track->fcc;
        map->stream_codec = track->codec;

        if (track->compressorname)
        {
            map->stream_encoder = malloc(sizeof(track->compressorname));
            strncpy(map->stream_encoder, track->compressorname, sizeof(track->compressorname));
        }
        if (track->name)
        {
            map->track_title = malloc(sizeof(track->name));
            strncpy(map->track_title, track->name, sizeof(track->name));
        }

        map->track_languagecode = malloc(4);
        strncpy(map->track_languagecode, track->language, 3);
        map->track_languagecode[3] = '\0';

        if (track->timescale)
        {
            map->duration_ms = ((double)track->duration / (double)track->timescale * 1000.0);
            map->creation_time = ((double)track->creation_time / (double)track->timescale * 1000.0);
            map->modification_time = ((double)track->modification_time / (double)track->timescale * 1000.0);
        }

        map->sample_alignment = true; // TODO not very true
        map->sample_count = track->stsz_sample_count + track->sps_count + track->pps_count;

        map->track_id = track->id;

        if (track->handlerType == HANDLER_AUDIO)
        {
            map->stream_type = stream_AUDIO;
            map->sampling_rate = track->sample_rate_hz;
            map->channel_count = track->channel_count;
            map->bit_per_sample = track->sample_size_bits;
        }
        else if (track->handlerType == HANDLER_VIDEO)
        {
            map->stream_type = stream_VIDEO;
            map->width = track->width;
            map->height = track->height;
            map->color_depth = track->color_depth;
            map->color_matrix = track->color_matrix;
            map->color_range = track->color_range;

            if (track->par_h && track->par_v)
            {
                map->pixel_aspect_ratio_h = track->par_h;
                map->pixel_aspect_ratio_v = track->par_v;
            }
            else
            {
                map->pixel_aspect_ratio_h = 1;
                map->pixel_aspect_ratio_v = 1;
            }

            map->frame_count_idr = track->stss_entry_count;

            // Framerate
            {
                uint32_t scalefactor = 1;
                map->framerate_num = track->timescale * scalefactor;

                if (track->stsz_sample_count == 0)
                    map->framerate_base = track->mediatime * scalefactor; // used for "progressive download" files
                else
                    map->framerate_base = ((double)track->duration / (double)track->stsz_sample_count * (double)scalefactor);

                if (map->framerate_base > 0.0)
                    map->framerate = map->framerate_num / map->framerate_base;

                TRACE_1(MP4, "framerate_num: %f  / framerate_base: %f\n",
                        map->framerate_num, map->framerate_base);
            }

            // Codec specific metadata
            if (track->codec == CODEC_H264 || track->codec == CODEC_H265)
            {
                // Set SPS
                for (i = 0; i < track->sps_count; i++)
                {
                    map->sample_type[i] = sample_VIDEO_PARAM;
                    map->sample_offset[i] = track->sps_sample_offset[i];
                    map->sample_size[i] = track->sps_sample_size[i];
                    map->sample_pts[i] = -1;
                    map->sample_dts[i] = -1;
                }
                // Set PPS
                for (i = 0; i < track->pps_count; i++)
                {
                    map->sample_type[i + track->sps_count] = sample_VIDEO_PARAM;
                    map->sample_offset[i + track->sps_count] = track->pps_sample_offset[i];
                    map->sample_size[i + track->sps_count] = track->pps_sample_size[i];
                    map->sample_pts[i + track->sps_count] = -1;
                    map->sample_dts[i + track->sps_count] = -1;
                }
            }
        }
        else if (track->handlerType == HANDLER_SUBT ||
                 track->handlerType == HANDLER_SBTL ||
                 track->handlerType == HANDLER_TEXT)
        {
            map->stream_type = stream_TEXT;
        }
        else if (track->handlerType == HANDLER_TMCD)
        {
            map->stream_type = stream_TMCD;
        }
        else if (track->handlerType == HANDLER_META)
        {
            map->stream_type = stream_META;
        }
        else if (track->handlerType == HANDLER_HINT)
        {
            map->stream_type = stream_HINT;
        }
        else
        {
            map->stream_type = stream_UNKNOWN;
        }

        // Set samples details into the bitstream map
        ////////////////////////////////////////////////////////////////

        // Bitrate mode
        uint32_t sample_size_cbr = 0;
        if (!track->stsz_entry_size)
        {
            // Assume constant sample size
            map->bitrate_mode = BITRATE_CBR;
            sample_size_cbr = track->stsz_sample_size;

            // PCM hack
            if (track->stsz_sample_size == 1 &&
                (track->codec == CODEC_LPCM || track->codec == CODEC_LogPCM ||
                 track->codec == CODEC_DPCM || track->codec == CODEC_ADPCM))
            {
                sample_size_cbr = track->channel_count * (track->sample_size_bits/8);
            }
        }

        // Set sample type & size
        for (i = 0; i < track->stsz_sample_count; i++)
        {
            unsigned sid = i + track->sps_count + track->pps_count; // Sample id

            // Set sample type
            if (track->handlerType == HANDLER_VIDEO)
            {
                map->sample_type[sid] = sample_VIDEO;

                for (j = 0; j < track->stss_entry_count; j++)
                {
                    if (i == (track->stss_sample_number[j] - 1))
                        map->sample_type[sid] = sample_VIDEO_SYNC;
                }
            }
            else if (track->handlerType == HANDLER_AUDIO)
            {
                map->sample_type[sid] = sample_AUDIO;
            }
            else if (track->handlerType == HANDLER_SUBT ||
                     track->handlerType == HANDLER_SBTL ||
                     track->handlerType == HANDLER_TEXT)
            {
                map->sample_type[sid] = sample_TEXT;
            }
            else
            {
                map->sample_type[sid] = sample_OTHER;
            }

            // Set sample size
            if (track->stsz_entry_size)
            {
                map->sample_size[sid] = track->stsz_entry_size[i];
            }
            else // Assume constant sample size
            {
                map->sample_size[sid] = sample_size_cbr;
            }

            // Contribute to stream size
            map->stream_size += map->sample_size[sid];
        }

        // Set sample decoding and presentation timecodes
        uint32_t k = 0;
        j = 0;
        int32_t _samples_pts_to_dts_shift = 0; // FIXME // from cslg

        if (track->ctts_sample_count) //if (_samples_pts_array)
        {
            // Compute DTS
            for (i = 0, k = track->sps_count + track->pps_count; i < track->stts_entry_count; i++)
            {
                j = 0;

                if (k == track->sps_count + track->pps_count)
                {
                    // Decoding time = 0 for the first DTS sample
                    map->sample_dts[k] = 0;

                    k++;
                    j = 1;
                }

                for (; j < track->stts_sample_count[i]; j++, k++)
                {
                    int64_t dts = map->sample_dts[k - 1];
                    dts = dts + track->stts_sample_delta[i];

                    map->sample_dts[k] = dts;
                }
            }

            // Then compute PTS
            for (i = 0, k = track->sps_count + track->pps_count; i < track->ctts_entry_count; i++)
            {
                for (j = 0; j < track->ctts_sample_count[i]; j++, k++)
                {
                    int64_t dts = map->sample_dts[k];
                    int64_t pts = dts + track->ctts_sample_offset[i] + _samples_pts_to_dts_shift;

                    // Assign pts
                    map->sample_pts[k] = pts;
                }
            }
        }
        else
        {
            // Compute DTS, then copy results into PTS
            for (i = 0, k = track->sps_count + track->pps_count; i < track->stts_entry_count; i++)
            {
                j = 0;

                if (k == track->sps_count + track->pps_count)
                {
                    // Decoding time = 0 for the first DTS sample
                    map->sample_dts[k] = map->sample_pts[k] = 0;

                    k++;
                    j = 1;
                }

                for (; j < track->stts_sample_count[i]; j++, k++)
                {
                    int64_t dts = map->sample_dts[k - 1];
                    int64_t pts = dts + track->stts_sample_delta[i];

                    map->sample_dts[k] = map->sample_pts[k] = pts;
                }
            }
        }

        // Set sample offset
        uint32_t index = track->sps_count + track->pps_count;
        uint32_t chunkOffset = 0;

        for (i = 0; (i < track->stsc_entry_count) && (chunkOffset < track->stco_entry_count); i++)
        {
            uint32_t n = 0, k = 0, l = 0;

            if ((i + 1) == track->stsc_entry_count)
            {
                if ((track->stsc_entry_count > 1) && (chunkOffset == 0))
                {
                    n = 1;
                }
                else
                {
                    n = track->stco_entry_count - chunkOffset;
                }
            }
            else
            {
                n = track->stsc_first_chunk[i + 1] - track->stsc_first_chunk[i];
            }

            for (k = 0; k < n; k++)
            {
                for (l = 0; l < track->stsc_samples_per_chunk[i]; l++)
                {
                    // Adjust DTS and PTS unit: from timescale to ns
                    {
                        if (map->sample_dts[index])
                        {
                            map->sample_dts[index] *= 1000000LL;
                            map->sample_dts[index] /= track->timescale;
                        }

                        //if (map->sample_pts[index])
                        {
                            //if (map->sample_pts[index] > av_pts_adjustment)
                            //    map->sample_pts[index] -= av_pts_adjustment; // FIXME // from edit list;

                            map->sample_pts[index] *= 1000000LL;
                            map->sample_pts[index] /= track->timescale;
                        }

                        //TRACE_2(MP4, "#%u > DTS: %lli  /  PTS: %lli\n", index, map->sample_dts[index], map->sample_pts[index]);
                    }

                    // FIXME // sample description index is not taken into account
                    if (l == 0)
                    {
                        map->sample_offset[index++] = track->stco_chunk_offset[chunkOffset];
                    }
                    else
                    {
                        map->sample_offset[index] = map->sample_offset[index - 1] + (int64_t)(map->sample_size[index - 1]);
                        index++;
                    }
                }

                // Increase chunk offset
                chunkOffset++;
            }
        }

#if ENABLE_DEBUG
        TRACE_INFO(MP4, BLD_GREEN ">> track content recap:\n" CLR_RESET);
        if (map->stream_type == stream_VIDEO)
        {
            TRACE_1(MP4, "Video Stream\n");
        }
        else if (map->stream_type == stream_AUDIO)
        {
            TRACE_1(MP4, "Audio Stream\n");
        }

        TRACE_1(MP4, "sample_count     : %u\n", map->sample_count);
        TRACE_1(MP4, "sample_count_idr : %u\n", map->frame_count_idr);
/*
        for (i = 0; i < map->sample_count; i++)
        {
            TRACE_2(MP4, "[%u] sample type   > %u\n", i, map->sample_type[i]);
            TRACE_1(MP4, "[%u] sample size   > %u\n", i, map->sample_size[i]);
            TRACE_1(MP4, "[%u] sample offset > %lli\n", i, map->sample_offset[i]);
            TRACE_2(MP4, "[%u] sample pts    > %lli\n", i, map->sample_pts[i]);
            TRACE_2(MP4, "[%u] sample dts    > %lli\n", i, map->sample_dts[i]);
        }
*/
#endif // ENABLE_DEBUG
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Free a videoTrack_t structure.
 * \param **track_ptr A pointer to the videoTrack_t structure we want to freed.
 */
static void freeTrack(Mp4Track_t **track_ptr)
{
    if (*track_ptr != NULL)
    {
        // SPS
        free((*track_ptr)->sps_sample_offset);
        free((*track_ptr)->sps_sample_size);

        // PPS
        free((*track_ptr)->pps_sample_offset);
        free((*track_ptr)->pps_sample_size);

        // stss
        free((*track_ptr)->stss_sample_number);

        // stss
        free((*track_ptr)->stts_sample_count);
        free((*track_ptr)->stts_sample_delta);

        // ctts
        free((*track_ptr)->ctts_sample_count);
        free((*track_ptr)->ctts_sample_offset);

        // stsc
        free((*track_ptr)->stsc_first_chunk);
        free((*track_ptr)->stsc_samples_per_chunk);
        free((*track_ptr)->stsc_sample_description_index);

        // stsz / stz2
        free((*track_ptr)->stsz_entry_size);

        // stco / co64
        free((*track_ptr)->stco_chunk_offset);

        // track
        free(*track_ptr);
        *track_ptr = NULL;
    }
}

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \brief Parse box header.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 4.2 Object Structure.
 *
 * bitstr pointer is not checked for performance reason.
 */
static int parse_box_header(Bitstream_t *bitstr, Mp4Box_t *box_header)
{
    TRACE_3(MP4, "parse_box_header()\n");
    int retcode = SUCCESS;

    if (box_header == NULL)
    {
        TRACE_ERROR(MP4, "Invalid Mp4Box_t structure!\n");
        retcode = FAILURE;
    }
    else
    {
        // Set box offset
        box_header->offset_start = bitstream_get_absolute_byte_offset(bitstr);

        // Read box size
        box_header->size = (int64_t)read_bits(bitstr, 32);

        // Read box type
        box_header->boxtype = read_bits(bitstr, 32);

        if (box_header->size == 0)
        {
            // the size is the remaining space in the file
            box_header->size = bitstr->bitstream_size - box_header->offset_start;
        }
        else if (box_header->size == 1)
        {
            // the size is actually a 64b field coded right after the box type
            box_header->size = (int64_t)read_bits_64(bitstr, 64);
        }

        // Set end offset
        box_header->offset_end = box_header->offset_start + box_header->size;

        if (box_header->boxtype == BOX_UUID)
        {
            //box_header->usertype = malloc(16);
            //if (box_header->usertype)
            {
                int i = 0;
                for (i = 0; i < 16; i++)
                {
                    box_header->usertype[i] = (uint8_t)read_bits(bitstr, 8);
                }
            }
        }

        // Init "FullBox" parameters
        box_header->version = 0;
        box_header->flags = 0;
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Print a box header content.
 */
static void print_box_header(Mp4Box_t *box_header)
{
#if ENABLE_DEBUG
    TRACE_2(MP4, "* start offset  : %lli\n", box_header->offset_start);
    TRACE_2(MP4, "* end offset    : %lli\n", box_header->offset_end);

    // Print Box header
    if (box_header->size == 1)
    {
        TRACE_2(MP4, "* box largesize : %lli\n", box_header->size);
    }
    else
    {
        TRACE_2(MP4, "* box size      : %lli\n", box_header->size);
    }

    TRACE_2(MP4, "* box type      : 0x%X\n", box_header->boxtype);
    if (box_header->boxtype == BOX_UUID)
    {
        TRACE_2(MP4, "* box usertype  : '%s'\n", box_header->usertype);
    }

    // Print FullBox header
    if (box_header->version != 0 || box_header->flags != 0)
    {
        TRACE_2(MP4, "* version       : %u\n", box_header->version);
        TRACE_2(MP4, "* flags         : 0x%X\n", box_header->flags);
    }
#endif // ENABLE_DEBUG
}

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \brief Padding bits box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.7.6 Padding Bits Box.
 */
static int parse_padb(Bitstream_t *bitstr, Mp4Box_t *box_header)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_padb()\n" CLR_RESET);
    int retcode = SUCCESS;

    unsigned int i;
    unsigned int sample_count = read_bits(bitstr, 32);

    for (i = 0; i < ((sample_count + 1)/2); i++)
    {
        const int reserved1 = read_bit(bitstr);
        int pad1 = read_bits(bitstr, 3);
        const int reserved2 = read_bit(bitstr);
        int pad2 = read_bits(bitstr, 3);
    }

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Unknown box, just parse header.
 *
 * When encountering an unknown box type, just print the header infos, the box
 * will be automatically skipped.
 */
static int parse_unknown_box(Bitstream_t *bitstr, Mp4Box_t *box_header)
{
#if ENABLE_DEBUG
    char fcc[5];
    TRACE_WARNING(MP4, BLD_GREEN "parse_unknown_box('%s' @ %lli; size is %u)\n" CLR_RESET,
                  getFccString_le(box_header->boxtype, fcc), box_header->offset_start,
                  box_header->offset_end - box_header->offset_start);

    // Print box header
    print_box_header(box_header);

    // Print box content
#endif // ENABLE_DEBUG

    return SUCCESS;
}

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \brief File Type Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 4.3 File Type Box.
 */
static int parse_ftyp(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_ftyp()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read brand identifier
    unsigned int major_brand = read_bits(bitstr, 32);

    // Read informative integer for the minor version of the major brand
    unsigned int minor_version = read_bits(bitstr, 32);

    // Read a list of brands, until the end of the box
    unsigned int compatible_brands[8] = {0};

    unsigned int i = 0;
    unsigned int nb_compatible_brands = (box_header->size - 16) / 4;

    if (nb_compatible_brands > 8)
    {
        TRACE_WARNING(MP4, "Too much compatible_brands! Consider handling more than 8.");
        nb_compatible_brands = 8;
    }

    for (i = 0; i < nb_compatible_brands; i++)
    {
        compatible_brands[i] = read_bits(bitstr, 32);
    }

#if ENABLE_DEBUG
    {
        char fcc[5];

        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "> major_brand   : 0x%08X\n", major_brand);
        TRACE_1(MP4, "> minor_version : %i\n", minor_version);
        for (i = 0; i < nb_compatible_brands; i++)
        {
            TRACE_1(MP4, "> compatible_brands[%i] : '%s' (0x%X)\n",
                    i, getFccString_le(compatible_brands[i], fcc), compatible_brands[i]);
        }
    }
#endif // ENABLE_DEBUG

    return retcode;
}
/* ************************************************************************** */

/*!
 * \brief Progressive Download Information Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.3.1 Progressive Download Information Box.
 */
static int parse_pdin(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_pdin()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Read rate/initial_delay
    int tbr = box_header->offset_end - bitstream_get_absolute_byte_offset(bitstr);

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "pdin contains %i pairs of values\n", tbr);
    }
#endif // ENABLE_DEBUG

    int i = 0;
    for (i = 0; i < tbr; i++)     // to end of box
    {
        unsigned int rate = read_bits(bitstr, 32);
        unsigned int initial_delay = read_bits(bitstr, 32);

        TRACE_1(MP4, "[i] > rate          : %u\n", i, rate);
        TRACE_1(MP4, "    > initial_delay : %u\n", initial_delay);
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Parse the container for metadata.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.2.1 Movie Box.
 */
static int parse_moov(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_moov()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print moov box header
    print_box_header(box_header);
    int64_t box_moov_end = box_header->offset_end;

    while (mp4->run == true &&
           retcode == SUCCESS &&
           bitstream_get_absolute_byte_offset(bitstr) < box_moov_end)
    {
        // Parse subbox header
        Mp4Box_t box_subheader;
        retcode = parse_box_header(bitstr, &box_subheader);

        // Then parse subbox content
        if (retcode == SUCCESS)
        {
            switch (box_subheader.boxtype)
            {
                case BOX_MVHD:
                    retcode = parse_mvhd(bitstr, &box_subheader, mp4);
                    break;
                case BOX_IODS:
                    retcode = parse_iods(bitstr, &box_subheader);
                    break;
                case BOX_TRAK:
                    retcode = parse_trak(bitstr, &box_subheader, mp4);
                    break;
                default:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
            }

            jumpy_mp4(bitstr, box_header, &box_subheader);
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Parse the Media Data Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.1.1 Media Data Box.
 *
 * This box contains the media data. In video tracks, this box would contain
 * video frames.
 * The parser doesn't really care for this box as long as we have already
 * indexed the A/V samples.
 */
static int parse_mdat(Bitstream_t *bitstr, Mp4Box_t *box_header)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_mdat()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print mdat box header
    print_box_header(box_header);

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Track Header Box - Fullbox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.3.2 Track Header Box.
 *
 * This box specifies the characteristics of a single track.
 * Exactly one Track Header Box is contained in a track.
 */
static int parse_tkhd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_tkhd()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Read box content
    if (box_header->version == 1)
    {
        track->creation_time = read_bits_64(bitstr, 64);
        track->modification_time = read_bits_64(bitstr, 64);
        track->id = read_bits(bitstr, 32);
        /*const unsigned int reserved =*/ read_bits(bitstr, 32);
        track->duration = read_bits_64(bitstr, 64);
    }
    else // if (box_header->version == 0)
    {
        track->creation_time = read_bits(bitstr, 32);
        track->modification_time = read_bits(bitstr, 32);
        track->id = read_bits(bitstr, 32);
        /*const unsigned int reserved =*/ read_bits(bitstr, 32);
        track->duration = read_bits(bitstr, 32);
    }

    unsigned int reserved[2] = {0};
    reserved[0] = read_bits(bitstr, 32);
    reserved[1] = read_bits(bitstr, 32);

    int layer = read_bits(bitstr, 16);
    int alternate_group = read_bits(bitstr, 16);
    int volume = read_bits(bitstr, 16);
    /*const unsigned int reserved =*/ read_bits(bitstr, 16);

    int i = 0, matrix[9] = {0};
    for (i = 0; i < 9; i++)
    {
        matrix[i] = read_bits(bitstr, 32);
    }

    unsigned int width = read_bits(bitstr, 32);
    unsigned int height = read_bits(bitstr, 32);

#if ENABLE_DEBUG
    {
        // Print tkhd box header
        print_box_header(box_header);

        // Print tkhd box content
        TRACE_1(MP4, "> creation_time     : %llu\n", track->creation_time);
        TRACE_1(MP4, "> modification_time : %llu\n", track->modification_time);
        TRACE_1(MP4, "> track_ID          : %u\n", track->id);
        TRACE_1(MP4, "> duration          : %llu\n", track->duration);
        TRACE_1(MP4, "> layer             : %i\n", layer);
        TRACE_1(MP4, "> alternate_group   : %i\n", alternate_group);
        TRACE_1(MP4, "> volume            : %i\n", volume);
        TRACE_1(MP4, "> matrix : [%u, %u, %u, %u, %u, %u, %u, %u, %u]\n",
                matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], matrix[6], matrix[7], matrix[8]);
        TRACE_1(MP4, "> width  : %u\n", width);
        TRACE_1(MP4, "> height : %u\n", height);
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Edit Box - Fullbox.
 *
 * From 'ISO/IEC 14496-12' specification:
 *
 *
 */
static int parse_edts(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_edst()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print trak box header
    print_box_header(box_header);

    while (retcode == SUCCESS &&
           bitstream_get_absolute_byte_offset(bitstr) < box_header->offset_end)
    {
        // Parse subbox header
        Mp4Box_t box_subheader;
        retcode = parse_box_header(bitstr, &box_subheader);

        // Then parse subbox content
        if (retcode == SUCCESS)
        {
            switch (box_subheader.boxtype)
            {
                case BOX_ELST:
                    retcode = parse_elst(bitstr, &box_subheader, track);
                    break;
                default:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
            }

            jumpy_mp4(bitstr, box_header, &box_subheader);
        }
    }

    return retcode;
}

/*!
 * \brief Edit List Box - Fullbox.
 *
 * From 'ISO/IEC 14496-12' specification:
 *
 */
static int parse_elst(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_elst()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Read box content
    if (box_header->version == 1)
    {

    }
    else // if (version == 0)
    {
        uint32_t entries = read_bits(bitstr, 32);

        unsigned i = 0;
        for (i = 0; i < entries; i++)
        {
            uint32_t segmentDuration = read_bits(bitstr, 32);
            track->mediatime = read_bits(bitstr, 32);
            uint32_t mediaRate = read_bits(bitstr, 32);

            // we only need one "mediaTime", used to compute framerate of "progressive download" files
            break;
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Parse the Movie Header Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.2.2 Movie Header Box.
 *
 * This box defines overall information which is media-independent, and relevant
 * to the entire presentation considered as a whole.
 */
static int parse_mvhd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_mvhd()\n" CLR_RESET);
    int retcode = SUCCESS;
    int i = 0;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Read box content
    if (box_header->version == 1)
    {
        mp4->creation_time = read_bits_64(bitstr, 64);
        mp4->modification_time = read_bits_64(bitstr, 64);
        mp4->timescale = read_bits(bitstr, 32);
        mp4->duration = read_bits_64(bitstr, 64);
    }
    else // if (version == 0)
    {
        mp4->creation_time = read_bits(bitstr, 32);
        mp4->modification_time = read_bits(bitstr, 32);
        mp4->timescale = read_bits(bitstr, 32);
        mp4->duration = read_bits(bitstr, 32);
    }

    uint32_t rate = read_bits(bitstr, 32);
    uint32_t volume = read_bits(bitstr, 16);
    /*unsigned int reserved =*/ read_bits(bitstr, 16);
    /*unsigned int reserved =*/ read_bits(bitstr, 32);
    /*unsigned int reserved =*/ read_bits(bitstr, 32);

    // Provides a transformation matrix for the video;
    int32_t matrix[9];
    for (i = 0; i < 9; i++)
    {
        matrix[i] = read_bits(bitstr, 32);
    }

    // ?
    int32_t predefined[6];
    for (i = 0; i < 6; i++)
    {
        predefined[i] = read_bits(bitstr, 32);
    }

    uint32_t next_track_ID = read_bits(bitstr, 32);

#if ENABLE_DEBUG
    {
        // Print mvhd box header
        print_box_header(box_header);

        // Print mvhd box content
        TRACE_1(MP4, "> creation_time     : %llu\n", mp4->creation_time);
        TRACE_1(MP4, "> modification_time : %llu\n", mp4->modification_time);
        TRACE_1(MP4, "> timescale  : %u\n", mp4->timescale);
        TRACE_1(MP4, "> duration   : %llu\n", mp4->duration);
        TRACE_1(MP4, "> rate       : %u\n", rate);
        TRACE_1(MP4, "> volume     : %llu\n", volume);
        for (i = 0; i < 9; i++)
        {
            TRACE_1(MP4, "> matrix[%i] : %i\n", i, matrix[i]);
        }
        TRACE_1(MP4, "> next track ID     : %u\n", next_track_ID);

    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Parse the object descriptor box.
 *
 * From 'ISO/IEC 14496-14' specification:
 * 5.1 object descriptor Box.
 */
static int parse_iods(Bitstream_t *bitstr, Mp4Box_t *box_header)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_iods()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Print trak box header
    print_box_header(box_header);

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Parse the container for individual track or stream.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.3.3 Track Reference Box.
 *
 * This box provides a reference from the containing track to another track in the
 * presentation.
 * Exactly one Track Reference Box can be contained within the Track Box.
 *
 * This box does not contain informations, only other boxes.
 */
static int parse_trak(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4_t *mp4)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_trak()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print trak box header
    print_box_header(box_header);

    // Init a track structure
    int track_id = mp4->tracks_count;
    mp4->tracks[track_id] = (Mp4Track_t*)calloc(1, sizeof(Mp4Track_t));

    if (mp4->tracks[track_id] == NULL)
    {
        TRACE_ERROR(MP4, "Unable to allocate a new mp4 track!\n");
        retcode = FAILURE;
    }
    else
    {
        mp4->tracks[track_id]->id = track_id;
        mp4->tracks_count++;
    }

    while (mp4->run == true &&
           retcode == SUCCESS &&
           bitstream_get_absolute_byte_offset(bitstr) < box_header->offset_end)
    {
        // Parse subbox header
        Mp4Box_t box_subheader;
        retcode = parse_box_header(bitstr, &box_subheader);

        // Then parse subbox content
        if (retcode == SUCCESS)
        {
            switch (box_subheader.boxtype)
            {
                case BOX_TKHD:
                    retcode = parse_tkhd(bitstr, &box_subheader, mp4->tracks[track_id]);
                    break;
                case BOX_EDTS:
                    retcode = parse_edts(bitstr, &box_subheader, mp4->tracks[track_id]);
                    break;
                case BOX_MDIA:
                    retcode = parse_mdia(bitstr, &box_subheader, mp4->tracks[track_id]);
                    break;
                default:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
            }

            jumpy_mp4(bitstr, box_header, &box_subheader);
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Media Header Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.4.2 Media Header Box.
 *
 * The media header box declares overall information that is media-independent,
 * and relevant to characteristics of the media in a track.
 */
static int parse_mdhd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_mdhd()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Read box content
    if (box_header->version == 1)
    {
        track->creation_time = read_bits_64(bitstr, 64);
        track->modification_time = read_bits_64(bitstr, 64);
        track->timescale = read_bits(bitstr, 32);
        track->duration = read_bits_64(bitstr, 64);
    }
    else // if (version == 0)
    {
        track->creation_time = read_bits(bitstr, 32);
        track->modification_time = read_bits(bitstr, 32);
        track->timescale = read_bits(bitstr, 32);
        track->duration = read_bits(bitstr, 32);
    }

    /*unsigned int pad =*/ read_bit(bitstr);

    // ISO-639-2/T language code
    // Each character is packed as the difference between its ASCII value and 0x60
    track->language[0] = (uint8_t)read_bits(bitstr, 5) + 96;
    track->language[1] = (uint8_t)read_bits(bitstr, 5) + 96;
    track->language[2] = (uint8_t)read_bits(bitstr, 5) + 96;

    /*unsigned int pre_defined =*/ read_bits(bitstr, 16);

#if ENABLE_DEBUG
    {
        // Print mdhd box header
        print_box_header(box_header);

        // Print mdhd box content
        TRACE_1(MP4, "> creation_time     : %llu\n", track->creation_time);
        TRACE_1(MP4, "> modification_time : %llu\n", track->modification_time);
        TRACE_1(MP4, "> timescale   : %u\n", track->timescale);
        TRACE_1(MP4, "> duration    : %llu\n", track->duration);
        TRACE_1(MP4, "> language[3] : '%c%c%c'\n",
                track->language[0], track->language[1], track->language[2]);
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Media Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.4.1 Media Box.
 *
 * The media declaration container contains all the objects that declare information
 * about the media data within a track.
 * This box does not contain informations, only other boxes.
 */
static int parse_mdia(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_mdia()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print mdia box header
    print_box_header(box_header);

    while (retcode == SUCCESS &&
           bitstream_get_absolute_byte_offset(bitstr) < box_header->offset_end)
    {
        // Parse subbox header
        Mp4Box_t box_subheader;
        retcode = parse_box_header(bitstr, &box_subheader);

        // Then parse subbox content
        if (retcode == SUCCESS)
        {
            switch (box_subheader.boxtype)
            {
                case BOX_MDHD:
                    retcode = parse_mdhd(bitstr, &box_subheader, track);
                    break;
                case BOX_HDLR:
                    retcode = parse_hdlr(bitstr, &box_subheader, track);
                    break;
                case BOX_MINF:
                    retcode = parse_minf(bitstr, &box_subheader, track);
                    break;
                default:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
            }

            jumpy_mp4(bitstr, box_header, &box_subheader);
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Handler Reference Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.4.3 Handler Reference Box.
 *
 * This box within a Media Box declares the process by which the media-data in the
 * track is presented, and thus, the nature of the media in a track.
 */
static int parse_hdlr(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_hdlr()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Read box content
    unsigned int pre_defined = read_bits(bitstr, 32);
    track->handlerType = read_bits(bitstr, 32);

    unsigned int reserved[3];
    reserved[0] = read_bits(bitstr, 32);
    reserved[1] = read_bits(bitstr, 32);
    reserved[2] = read_bits(bitstr, 32);

    int bytes_left = box_header->size - 32;
    if (bytes_left > 0)
    {
        // check if the bytes_left is also coded in the first byte (MOV style)
        // and make sure we store no more than 128 characters
        int namesize = next_bits(bitstr, 8);
        if (bytes_left == namesize + 1)
        {
            skip_bits(bitstr, 8);
            bytes_left == namesize;
        }
        if (bytes_left > 128) bytes_left = 128;

        int i = 0;
        for (i = 0; i < bytes_left; i++)
        {
            track->name[i] = read_bits(bitstr, 8);
        }
    }

#if ENABLE_DEBUG
    {
        // Print hdlr box header
        print_box_header(box_header);

        // Print hdlr box content
        char fcc[5];
        TRACE_1(MP4, "> pre_defined  : %u\n", pre_defined);
        TRACE_1(MP4, "> handler_type : 0x%X (%s)\n", track->handlerType,
                getFccString_le(track->handlerType, fcc));
        TRACE_1(MP4, "> name         : '%s'\n", track->name);

    }
#endif // ENABLE_DEBUG

    if (track->handlerType != HANDLER_AUDIO &&
        track->handlerType != HANDLER_VIDEO)
    {
        TRACE_1(MP4, "Not an audio or video track, ignoring\n");
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Media Information Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.4.4 Media Information Box.
 *
 * This box contains all the objects that declare characteristic information of
 * the media in the track.
 * This box does not contain informations, only other boxes.
 */
static int parse_minf(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_minf()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print minf box header
    print_box_header(box_header);

    // Subbox allocation
    Mp4Box_t box_subheader;

    while (retcode == SUCCESS &&
           bitstream_get_absolute_byte_offset(bitstr) < box_header->offset_end)
    {
        // Parse subbox header
        retcode = parse_box_header(bitstr, &box_subheader);

        // Then parse subbox content
        if (retcode == SUCCESS &&
            bitstream_get_absolute_byte_offset(bitstr) < box_header->offset_end)
        {
            switch (box_subheader.boxtype)
            {
                case BOX_DINF:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
                case BOX_STBL:
                    retcode = parse_stbl(bitstr, &box_subheader, track);
                    break;
                default:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
            }

            jumpy_mp4(bitstr, box_header, &box_subheader);
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Sample Table Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.5.1 Sample Table Box.
 *
 * Parse the sample table box, container for the time/space map.
 * This box does not contain informations, only other boxes.
 */
static int parse_stbl(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stbl()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Print stbl box header
    print_box_header(box_header);

    while (retcode == SUCCESS &&
           bitstream_get_absolute_byte_offset(bitstr) < box_header->offset_end)
    {
        // Parse subbox header
        Mp4Box_t box_subheader;
        retcode = parse_box_header(bitstr, &box_subheader);

        // Then parse subbox content
        if (retcode == SUCCESS)
        {
            switch (box_subheader.boxtype)
            {
                case BOX_STSD:
                    retcode = parse_stsd(bitstr, &box_subheader, track);
                    break;
                case BOX_STTS:
                    retcode = parse_stts(bitstr, &box_subheader, track);
                    break;
                case BOX_CTTS:
                    retcode = parse_ctts(bitstr, &box_subheader, track);
                    break;
                case BOX_STSS:
                    retcode = parse_stss(bitstr, &box_subheader, track);
                    break;
                case BOX_STSC:
                    retcode = parse_stsc(bitstr, &box_subheader, track);
                    break;
                case BOX_STSZ:
                    retcode = parse_stsz(bitstr, &box_subheader, track);
                    break;
                case BOX_STZ2:
                    retcode = parse_stsz(bitstr, &box_subheader, track);
                    break;
                case BOX_STCO:
                    retcode = parse_stco(bitstr, &box_subheader, track);
                    break;
                case BOX_CO64:
                    retcode = parse_stco(bitstr, &box_subheader, track);
                    break;
                default:
                    retcode = parse_unknown_box(bitstr, &box_subheader);
                    break;
            }

            jumpy_mp4(bitstr, box_header, &box_subheader);
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Sample Description Box.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.5.2 Sample Description Box.
 *
 * The SampleDescriptionBox contains information about codec types and some
 * initialization parameters needed to start decoding.
 * If an AVC box (AVCDecoderConfigurationRecord) is present, it also contains the
 * diferents SPS and PPS of the video.
 */
static int parse_stsd(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stsd()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Parse box content
    unsigned int reserved[6] = {0};
    int i = 0;
    for (i = 0; i < 6; i++)
    {
        reserved[i] = read_bits(bitstr, 8);
    }
    /*unsigned int data_reference_index =*/ read_bits(bitstr, 16);

    // Parse subbox header
    Mp4Box_t box_subheader;
    retcode = parse_box_header(bitstr, &box_subheader);

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    /*unsigned int entry_count =*/ read_bits(bitstr, 32);

    char fcc[5];
    track->fcc = box_subheader.boxtype; // save fourcc as backup

    // Then parse subbox content
    switch (track->handlerType)
    {
        case HANDLER_AUDIO:
        {
            // AudioSampleEntry
            // Box Types: ‘mp4a’

            if (box_subheader.boxtype == fcc_mp4a)
            {
                track->codec = CODEC_AAC;
                TRACE_1(MP4, "> Audio track is using AAC codec\n");
            }
            else if (box_subheader.boxtype == fcc_AC3 || box_subheader.boxtype == fcc_ac3)
            {
                track->codec = CODEC_AC3;
                TRACE_1(MP4, "> Audio track is using AC3 codec\n");
            }
            else if (box_subheader.boxtype == fcc_AC4 || box_subheader.boxtype == fcc_ac4)
            {
                track->codec = CODEC_AC4;
                TRACE_1(MP4, "> Audio track is using AC4 codec\n");
            }
            else if (box_subheader.boxtype == fcc_sowt)
            {
                track->codec = CODEC_LPCM;
                TRACE_1(MP4, "> Audio track is using PCM audio\n");
            }
            else
            {
                track->codec = CODEC_UNKNOWN;
                TRACE_WARNING(MP4, "> Unknown codec in audio track (%s)\n",
                              getFccString_le(box_subheader.boxtype, fcc));
            }

            /*const unsigned int reserved[0] =*/ read_bits(bitstr, 32);
            /*const unsigned int reserved[1] =*/ read_bits(bitstr, 32);

            track->channel_count = read_bits(bitstr, 16);
            track->sample_size_bits = read_bits(bitstr, 16);

            /*unsigned int pre_defined =*/ read_bits(bitstr, 16);
            /*const unsigned int(16) reserved =*/ read_bits(bitstr, 16);

            track->sample_rate_hz = read_bits(bitstr, 16);
        } break;

        case HANDLER_VIDEO:
        {
            // VisualSampleEntry
            // Box Types: 'avc1', 'm4ds', 'hev1', 'CFHD'

            if (box_subheader.boxtype == fcc_avc1)
            {
                track->codec = CODEC_H264;
                TRACE_1(MP4, "> Video track is using H.264 codec\n");
            }
            else if (box_subheader.boxtype == fcc_hvc1)
            {
                track->codec = CODEC_H265;
                TRACE_1(MP4, "> Video track is using H.265 codec\n");
            }
            else if (box_subheader.boxtype == fcc_mp4v)
            {
                track->codec = CODEC_MPEG4_ASP;
                TRACE_1(MP4, "> Video track is using XVID codec\n");
            }
            else if (box_subheader.boxtype == fcc_CFHD)
            {
                track->codec = CODEC_VC5;
                TRACE_1(MP4, "> Video track is using CineForm codec\n");
            }
            else
            {
                track->codec = CODEC_UNKNOWN;
                TRACE_WARNING(MP4, "> Unknown codec in video track (%s)\n",
                              getFccString_le(box_subheader.boxtype, fcc));
            }

            /*unsigned int pre_defined =*/ read_bits(bitstr, 16);
            /*const unsigned int reserved =*/ read_bits(bitstr, 16);

            unsigned int pre_defined[3] = {0};
            pre_defined[0] = read_bits(bitstr, 32);
            pre_defined[1] = read_bits(bitstr, 32);
            pre_defined[2] = read_bits(bitstr, 32);

            track->width = read_bits(bitstr, 16);
            track->height = read_bits(bitstr, 16);

            // 0x00480000; // 72 dpi
            unsigned int horizresolution = read_bits(bitstr, 32);
            unsigned int vertresolution = read_bits(bitstr, 32);

            /*const unsigned int reserved =*/ read_bits(bitstr, 32);

            unsigned int frame_count = read_bits(bitstr, 16);

            uint8_t compressorsize = (uint8_t)read_bits(bitstr, 8);
            for (i = 0; i < 31; i++)
            {
                track->compressorname[i] = (char)read_bits(bitstr, 8);
            }
            track->compressorname[compressorsize] = '\0';

            track->color_depth = read_bits(bitstr, 16);
            /*int pre_defined = */ read_bits(bitstr, 16);

#if ENABLE_DEBUG
            {
                // Print box header
                print_box_header(box_header);

                // Print VisualSampleEntry box header
                print_box_header(&box_subheader);

                // Print VisualSampleEntry box content
                TRACE_1(MP4, "> width  : %u\n", track->width);
                TRACE_1(MP4, "> height : %u\n", track->height);
                TRACE_1(MP4, "> horizresolution : 0x%X\n", horizresolution);
                TRACE_1(MP4, "> vertresolution  : 0x%X\n", vertresolution);
                TRACE_1(MP4, "> frame_count     : %u\n", frame_count);
                TRACE_1(MP4, "> compressor      : '%s'\n", track->compressorname);
                TRACE_1(MP4, "> color depth     : %u\n", track->color_depth);
            }
#endif // ENABLE_DEBUG

            while (retcode == SUCCESS &&
                   bitstream_get_absolute_byte_offset(bitstr) < box_subheader.offset_end)
            {
                // Parse subbox header
                Mp4Box_t box_subsubheader;
                retcode = parse_box_header(bitstr, &box_subsubheader);

                // Then parse subbox content
                ////////////////////////////////////////////////////////////////
                if (retcode == SUCCESS)
                {
                    switch (box_subsubheader.boxtype)
                    {
                        case BOX_AVCC:
                            retcode = parse_avcC(bitstr, &box_subsubheader, track);
                            break;
                        case BOX_BTRT:
                            retcode = parse_btrt(bitstr, &box_subsubheader, track);
                            break;
                        case BOX_CLAP:
                            retcode = parse_clap(bitstr, &box_subsubheader, track);
                            break;
                        case BOX_COLR:
                            retcode = parse_colr(bitstr, &box_subsubheader, track);
                            break;
                        case BOX_FIEL:
                            retcode = parse_fiel(bitstr, &box_subsubheader, track);
                            break;
                        case BOX_GAMA:
                            retcode = parse_gama(bitstr, &box_subsubheader, track);
                            break;
                        case BOX_PASP:
                            retcode = parse_pasp(bitstr, &box_subsubheader, track);
                            break;
                        default:
                            retcode = parse_unknown_box(bitstr, &box_subsubheader);
                            break;
                    }

                    jumpy_mp4(bitstr, &box_subheader, &box_subsubheader);
                }
            }
        } break;

        case HANDLER_TEXT:
        break;

        case HANDLER_META:
        break;

        case HANDLER_TMCD:
        break;

        case HANDLER_HINT:
        break;

        default:
            TRACE_1(MP4, "Unknown track type, skipped...\n");
        break;
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief AVCConfigurationBox.
 *
 * From 'ISO/IEC 14496-15' specification:
 * 5.2.4 Decoder configuration information.
 *
 * This subclause specifies the decoder configuration information for ISO/IEC
 * 14496-10 video content.
 * Contain AVCDecoderConfigurationRecord data structure (5.2.4.1.1 Syntax, 5.2.4.1.2 Semantics).
 */
static int parse_avcC(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_avcC()\n" CLR_RESET);
    int retcode = SUCCESS;

    // avcC box means H.264 codec
    track->codec = CODEC_H264;

    // Parse box content
    unsigned int i = 0;

    unsigned int configurationVersion = read_bits(bitstr, 8);
    unsigned int AVCProfileIndication = read_bits(bitstr, 8);
    unsigned int profile_compatibility = read_bits(bitstr, 8);
    unsigned int AVCLevelIndication = read_bits(bitstr, 8);
    /*int reserved =*/ read_bits(bitstr, 6);
    unsigned int lengthSizeMinusOne = read_bits(bitstr, 2);
    /*int reserved =*/ read_bits(bitstr, 3);

    // SPS
    track->sps_count = read_bits(bitstr, 5); // MAX_SPS = 32
    track->sps_sample_offset = (int64_t*)calloc(track->sps_count, sizeof(int64_t));
    track->sps_sample_size = (unsigned int*)calloc(track->sps_count, sizeof(unsigned int));
    for (i = 0; i < track->sps_count; i++)
    {
        track->sps_sample_size[i] = read_bits(bitstr, 16);
        track->sps_sample_offset[i] = bitstream_get_absolute_byte_offset(bitstr);

        skip_bits(bitstr, track->sps_sample_size[i] * 8); // sequenceParameterSetNALUnit
    }

    // PPS
    track->pps_count = read_bits(bitstr, 8); // MAX_PPS = 256
    track->pps_sample_offset = (int64_t*)calloc(track->pps_count, sizeof(int64_t));
    track->pps_sample_size = (unsigned int*)calloc(track->pps_count, sizeof(unsigned int));
    for (i = 0; i < track->pps_count; i++)
    {
       track->pps_sample_size[i] = read_bits(bitstr, 16);
       track->pps_sample_offset[i] = bitstream_get_absolute_byte_offset(bitstr);

       skip_bits(bitstr, track->pps_sample_size[i] * 8); // pictureParameterSetNALUnit
    }

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "> configurationVersion  : %u\n", configurationVersion);
        TRACE_1(MP4, "> AVCProfileIndication  : %u\n", AVCProfileIndication);
        TRACE_1(MP4, "> profile_compatibility : %u\n", profile_compatibility);
        TRACE_1(MP4, "> AVCLevelIndication    : %u\n", AVCLevelIndication);
        TRACE_1(MP4, "> lengthSizeMinusOne    : %u\n", lengthSizeMinusOne);

        TRACE_1(MP4, "> numOfSequenceParameterSets    = %u\n", track->sps_count);
        for (i = 0; i < track->sps_count; i++)
        {
            TRACE_1(MP4, "> sequenceParameterSetLength[%u] : %u\n", i, track->sps_sample_size[i]);
            TRACE_1(MP4, "> sequenceParameterSetOffset[%u] : %u\n", i, track->sps_sample_offset[i]);
        }

        TRACE_1(MP4, "> numOfPictureParameterSets     = %u\n", track->pps_count);
        for (i = 0; i < track->pps_count; i++)
        {
            TRACE_1(MP4, "> pictureParameterSetLength[%u]  : %u\n", i, track->pps_sample_size[i]);
            TRACE_1(MP4, "> pictureParameterSetOffset[%u]  : %u\n", i, track->pps_sample_offset[i]);
        }
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief BitRateBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.5.2 Sample Description Box
 * 8.5.2.2 Syntax
 * 8.5.2.3 Semantics
 */
static int parse_btrt(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_btrt()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Parse box content
    unsigned int bufferSizeDB = read_bits(bitstr, 32);
    track->bitrate_max = read_bits(bitstr, 32);
    track->bitrate_avg = read_bits(bitstr, 32);

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "> bufferSizeDB : %u\n", bufferSizeDB);
        TRACE_1(MP4, "> maxBitrate   : %u\n", track->bitrate_max);
        TRACE_1(MP4, "> avgBitrate   : %u\n", track->bitrate_avg);
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief CleanApertureBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.5.2 Sample Description Box
 * 8.5.2.2 Syntax
 * 8.5.2.3 Semantics
 */
static int parse_clap(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_clap()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Parse box content
    //unsigned int clap_size = read_bits(bitstr, 32);
    //unsigned int clap_type = read_bits(bitstr, 32);

    unsigned int cleanApertureWidthN = read_bits(bitstr, 32);
    unsigned int cleanApertureWidthD = read_bits(bitstr, 32);
    unsigned int cleanApertureHeightN = read_bits(bitstr, 32);
    unsigned int cleanApertureHeightD = read_bits(bitstr, 32);
    unsigned int horizOffN = read_bits(bitstr, 32);
    unsigned int horizOffD = read_bits(bitstr, 32);
    unsigned int vertOffN = read_bits(bitstr, 32);
    unsigned int vertOffD = read_bits(bitstr, 32);

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        //TRACE_1(MP4, "> clap_size   : %u\n", clap_size);
        //TRACE_1(MP4, "> clap_type   : %u\n", clap_type);

        TRACE_1(MP4, "> cleanApertureWidthN   : %u\n", cleanApertureWidthN);
        TRACE_1(MP4, "> cleanApertureWidthD   : %u\n", cleanApertureWidthD);
        TRACE_1(MP4, "> cleanApertureHeightN  : %u\n", cleanApertureHeightN);
        TRACE_1(MP4, "> cleanApertureHeightD  : %u\n", cleanApertureHeightD);

        TRACE_1(MP4, "> horizOffN  : %u\n", horizOffN);
        TRACE_1(MP4, "> horizOffD  : %u\n", horizOffD);
        TRACE_1(MP4, "> vertOffN   : %u\n", vertOffN);
        TRACE_1(MP4, "> vertOffD   : %u\n", vertOffD);
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief ColourInformationBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.5.2 Sample Description Box
 * 8.5.2.2 Syntax
 * 8.5.2.3 Semantics
 */
static int parse_colr(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_colr()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Parse box content
    unsigned int colour_type = read_bits(bitstr, 32);
    unsigned int colour_primaries;
    unsigned int transfer_characteristics;
    unsigned int matrix_coefficients;

    if (colour_type == 'nclc' ||
        colour_type == 'nclx') // "on-screen colours"
    {
        // https://developer.apple.com/library/mac/technotes/tn2227/_index.html

        colour_primaries = read_bits(bitstr, 16);
        transfer_characteristics = read_bits(bitstr, 16);
        matrix_coefficients = read_bits(bitstr, 16);
        /*unsigned int reserved =*/ //read_bits(bitstr, 7);
        track->color_range = read_bits(bitstr, 16);

        if (matrix_coefficients == 1)
        {
            track->color_matrix = CM_bt709;
        }
        else if (matrix_coefficients == 6)
        {
            track->color_matrix = CM_bt601;
        }
        else if (matrix_coefficients == 7)
        {
            track->color_matrix = CM_SMPTE240M;
        }
    }
    else if (colour_type == 'rICC')
    {
        // ICC_profile; // restricted ICC profile
    }
    else if (colour_type == 'prof')
    {
        // ICC_profile; // unrestricted ICC profile
    }

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        char fcc[5];
        TRACE_1(MP4, "> colour_type             : %u\n", getFccString_le(colour_type, fcc));
        if (colour_type == 'nclc' || colour_type == 'nclx')
        {
            TRACE_1(MP4, "> colour_primaries        : %u\n", colour_primaries);
            TRACE_1(MP4, "> transfer_characteristics: %u\n", transfer_characteristics);
            TRACE_1(MP4, "> matrix_coefficients     : %u\n", matrix_coefficients);
            TRACE_1(MP4, "> full_range_flag         : %u\n", track->color_range);
        }
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief FIEL box.
 */
static int parse_fiel(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_fiel()\n" CLR_RESET);
    int retcode = SUCCESS;

    // TODO

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief GAMA box.
 */
static int parse_gama(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_gama()\n" CLR_RESET);
    int retcode = SUCCESS;

    // TODO

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief PixelAspectRatioBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.5.2 Sample Description Box
 * 8.5.2.2 Syntax
 * 8.5.2.3 Semantics
 */
static int parse_pasp(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_pasp()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Parse box content
    //unsigned int pasp_size = read_bits(bitstr, 32);
    //unsigned int pasp_type = read_bits(bitstr, 32);

    track->par_h = read_bits(bitstr, 32);
    track->par_v = read_bits(bitstr, 32);

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        //TRACE_1(MP4, "> pasp_size : %u\n", pasp_size);
        //TRACE_1(MP4, "> pasp_type : %u\n", pasp_type);
        TRACE_1(MP4, "> hSpacing  : %u\n", track->par_h);
        TRACE_1(MP4, "> vSpacing  : %u\n", track->par_v);
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Decoding Time to Sample Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.6.1.2 Decoding Time to Sample Box.
 *
 * This box contains a compact version of a table that allows indexing from
 * decoding time to sample number.
 */
static int parse_stts(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stts()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Parse box content
    track->stts_entry_count = read_bits(bitstr, 32);
    track->stts_sample_count = (unsigned int*)calloc(track->stts_entry_count, sizeof(unsigned int));
    track->stts_sample_delta = (unsigned int*)calloc(track->stts_entry_count, sizeof(unsigned int));

    uint32_t i = 0;

    if (track->stts_sample_count == NULL || track->stts_sample_delta == NULL)
    {
        TRACE_ERROR(MP4, "Unable to alloc entry_table table!\n");
        retcode = FAILURE;
    }
    else
    {
        for (i = 0; i < track->stts_entry_count; i++)
        {
            track->stts_sample_count[i] = read_bits(bitstr, 32);
            track->stts_sample_delta[i] = read_bits(bitstr, 32);
        }
    }

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "> entry_count   : %u\n", track->stts_entry_count);
#if TRACE_1
        TRACE_1(MP4, "> sample_number : [");
        for (i = 0; i < track->stts_entry_count; i++)
        {
            printf("(%u / %u),", track->stts_sample_count[i], track->stts_sample_delta[i]);
        }
        printf("]\n");
#endif // TRACE_1
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Composition Time to Sample Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.6.1.3 Composition Time to Sample Box.
 *
 * This box provides the offset between decoding time and composition time.
 */
static int parse_ctts(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_ctts()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Parse box content
    track->ctts_entry_count = read_bits(bitstr, 32);
    track->ctts_sample_count = (uint32_t*)calloc(track->ctts_entry_count, sizeof(uint32_t));
    track->ctts_sample_offset = (int64_t*)calloc(track->ctts_entry_count, sizeof(int64_t));

    uint32_t i = 0;

    if (track->ctts_sample_count == NULL || track->ctts_sample_offset == NULL)
    {
        TRACE_ERROR(MP4, "Unable to alloc entry_table table!\n");
        retcode = FAILURE;
    }
    else
    {
        for (i = 0; i < track->ctts_entry_count; i++)
        {
            track->ctts_sample_count[i] = read_bits(bitstr, 32);

            if (box_header->version == 0)
                track->ctts_sample_offset[i] = (int64_t)read_bits(bitstr, 32); // read uint
            else if (box_header->version == 1)
                track->ctts_sample_offset[i] = (int64_t)read_bits(bitstr, 32); // read int
        }
    }

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "> entry_count   : %u\n", track->ctts_entry_count);
#if TRACE_1
        TRACE_1(MP4, "> sample_number : [");
        for (i = 0; i < track->ctts_entry_count; i++)
        {
            if (box_header->version == 0)
                printf("(%u / %u),", track->ctts_sample_count[i], track->ctts_sample_offset_u[i]);
            else
                printf("(%u / %i),", track->ctts_sample_count[i], track->ctts_sample_offset_i[i]);
        }
        printf("]\n");
#endif // TRACE_1
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Sync Sample Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.6.2 Sync Sample Box.
 *
 * This box provides a compact marking of the random access points within the stream.
 */
static int parse_stss(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stss()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Parse box content
    unsigned int i = 0;
    track->stss_entry_count = read_bits(bitstr, 32);
    track->stss_sample_number = (unsigned int*)calloc(track->stss_entry_count, sizeof(unsigned int));

    if (track->stss_sample_number == NULL)
    {
        TRACE_ERROR(MP4, "Unable to alloc entry_table table!\n");
        retcode = FAILURE;
    }
    else
    {
        for (i = 0; i < track->stss_entry_count; i++)
        {
            track->stss_sample_number[i] = read_bits(bitstr, 32);
        }

#if ENABLE_DEBUG
        {
            // Print box header
            print_box_header(box_header);

            // Print box content
            TRACE_1(MP4, "> entry_count   : %u\n", track->stss_entry_count);
#if TRACE_1
            TRACE_1(MP4, "> sample_number : [");
            for (i = 0; i < track->stss_entry_count; i++)
            {
                printf("%u, ", track->stss_sample_number[i]);
            }
            printf("]\n");
#endif // TRACE_1
        }
#endif // ENABLE_DEBUG
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Sample To Chunk Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.7.4 Sample To Chunk Box.
 *
 * Samples within the media data are grouped into chunks. Chunks can be of different
 * sizes, and the samples within a chunk can have different sizes. This table can
 * be used to find the chunk that contains a sample, its position, and the associated
 * sample description.
 */
static int parse_stsc(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stsc()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Parse box content
    unsigned int i = 0;
    track->stsc_entry_count = read_bits(bitstr, 32);
    track->stsc_first_chunk = (unsigned int*)calloc(track->stsc_entry_count, sizeof(unsigned int));
    track->stsc_samples_per_chunk = (unsigned int*)calloc(track->stsc_entry_count, sizeof(unsigned int));
    track->stsc_sample_description_index = (unsigned int*)calloc(track->stsc_entry_count, sizeof(unsigned int));

    if (track->stsc_first_chunk == NULL ||
        track->stsc_samples_per_chunk == NULL ||
        track->stsc_sample_description_index == NULL)
    {
        TRACE_ERROR(MP4, "Unable to alloc first_chunk, samples_per_chunk or sample_description_index tables!\n");
        retcode = FAILURE;
    }
    else
    {
        for (i = 0; i < track->stsc_entry_count; i++)
        {
            track->stsc_first_chunk[i] = read_bits(bitstr, 32);
            track->stsc_samples_per_chunk[i] = read_bits(bitstr, 32);
            track->stsc_sample_description_index[i] = read_bits(bitstr, 32);
        }

#if ENABLE_DEBUG
        {
            // Print box header
            print_box_header(box_header);
#if TRACE_1
            // Print box content
            TRACE_1(MP4, "> entry_count : %u\n", track->stsc_entry_count);

            TRACE_1(MP4, "> first_chunk : [");
            for (i = 0; i < track->stsc_entry_count; i++)
            {
                printf("%u, ", track->stsc_first_chunk[i]);
            }
            printf("]\n");

            TRACE_1(MP4, "> samples_per_chunk : [");
            for (i = 0; i < track->stsc_entry_count; i++)
            {
                printf("%u, ", track->stsc_samples_per_chunk[i]);
            }
            printf("]\n");

            TRACE_1(MP4, "> sample_description_index : [");
            for (i = 0; i < track->stsc_entry_count; i++)
            {
                printf("%u, ", track->stsc_sample_description_index[i]);
            }
            printf("]\n");
#endif // TRACE_1
        }
#endif // ENABLE_DEBUG
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Sample Size Boxes - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.7.3 Sample Size Box
 *
 * This box contains the sample count and a table giving the size in bytes of each
 * sample. This allows the media data itself to be unframed. The total number of
 * samples in the media is always indicated in the sample count.
 *
 * This box has two variants: STSZ and STZ2.
 * - This variant has a fixed size 32-bit field for representing the sample
 *   sizes; it permits defining a constant size for all samples in a track.
 * - The STZ2 variant permits smaller size fields, to save space when the sizes
 *   are varying but small.
 */
static int parse_stsz(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stsz()\n" CLR_RESET);
    int retcode = SUCCESS;
    unsigned int i = 0;
    int field_size = 32;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Parse box content
    if (box_header->boxtype == BOX_STSZ)
    {
        track->stsz_sample_size = read_bits(bitstr, 32);
        track->stsz_sample_count = read_bits(bitstr, 32);
    }
    else //if (box_header->type == BOX_STZ2)
    {
        /*unsigned int reserved =*/ read_bits(bitstr, 24);
        field_size = read_bits(bitstr, 8);
        track->stsz_sample_count = read_bits(bitstr, 32);
    }

    if (track->stsz_sample_size == 0)
    {
        track->stsz_entry_size = (unsigned int*)calloc(track->stsz_sample_count, sizeof(unsigned int));

        if (track->stsz_entry_size == NULL)
        {
             TRACE_ERROR(MP4, "Unable to alloc entry_size table!\n");
             retcode = FAILURE;
        }
        else
        {
            for (i = 0; i < track->stsz_sample_count; i++)
            {
                track->stsz_entry_size[i] = read_bits(bitstr, field_size);
            }
        }
    }

#if ENABLE_DEBUG
    {
        // Print box header
        print_box_header(box_header);

        // Print box content
        TRACE_1(MP4, "> sample_count : %u\n", track->stsz_sample_count);
        TRACE_1(MP4, "> sample_size  : %u\n", track->stsz_sample_size);
/*
        if (track->stsz_sample_size == 0)
        {
            TRACE_1(MP4, "> entry_size : [");
            for (i = 0; i < track->stsz_sample_count; i++)
            {
                printf("%u, ", track->stsz_entry_size[i]);
            }
            printf("]\n");
        }
*/
    }
#endif // ENABLE_DEBUG

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Chunk Offset Box - FullBox.
 *
 * From 'ISO/IEC 14496-12' specification:
 * 8.7.5 Chunk Offset Box.
 *
 * The chunk offset table gives the index of each chunk into the containing file.
 * There are two variants, permitting the use of 32-bit (STCO variant) or 64-bit
 * offsets (CO64 variant).
 */
static int parse_stco(Bitstream_t *bitstr, Mp4Box_t *box_header, Mp4Track_t *track)
{
    TRACE_INFO(MP4, BLD_GREEN "parse_stco()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Read FullBox attributs
    box_header->version = (uint8_t)read_bits(bitstr, 8);
    box_header->flags = read_bits(bitstr, 24);

    // Parse box content
    unsigned int i = 0;
    track->stco_entry_count = read_bits(bitstr, 32);
    track->stco_chunk_offset = (int64_t*)calloc(track->stco_entry_count, sizeof(int64_t));

    if (track->stco_chunk_offset == NULL)
    {
        TRACE_ERROR(MP4, "Unable to alloc chunk_offset table!\n");
        retcode = FAILURE;
    }
    else
    {
        if (box_header->boxtype == BOX_CO64)
        {
            for (i = 0; i < track->stco_entry_count; i++)
            {
                track->stco_chunk_offset[i] = (int64_t)read_bits_64(bitstr, 64);
            }
        }
        else //if (box_header->type == BOX_STCO)
        {
            for (i = 0; i < track->stco_entry_count; i++)
            {
                track->stco_chunk_offset[i] = (int64_t)read_bits(bitstr, 32);
            }
        }
#if ENABLE_DEBUG
        {
            // Print box header
            print_box_header(box_header);

            // Print box content
            TRACE_1(MP4, "> entry_count  : %u\n", track->stco_entry_count);
/*
            TRACE_1(MP4, "> chunk_offset : [");
            for (i = 0; i < track->stco_entry_count; i++)
            {
                printf("%lli, ", track->stco_chunk_offset[i]);
            }
            printf("]\n");
*/
        }
#endif // ENABLE_DEBUG
    }

    return retcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

int mp4_fileParse(MediaFile_t *media)
{
    TRACE_INFO(MP4, BLD_GREEN "mp4_fileParse()\n" CLR_RESET);
    int retcode = SUCCESS;

    // Init bitstream to parse container infos
    Bitstream_t *bitstr = init_bitstream(media, NULL);

    if (bitstr != NULL)
    {
        // Init an MP4 structure
        Mp4_t mp4;
        memset(&mp4, 0, sizeof(Mp4_t));

        // A convenient way to stop the parser
        mp4.run = true;

        while (mp4.run == true &&
               retcode == SUCCESS &&
               bitstream_get_absolute_byte_offset(bitstr) < media->file_size)
        {
            // Read box header
            Mp4Box_t box_header;
            retcode = parse_box_header(bitstr, &box_header);

            // Then parse box content
            if (retcode == SUCCESS)
            {
                switch (box_header.boxtype)
                {
                    case BOX_FTYP:
                        retcode = parse_ftyp(bitstr, &box_header, &mp4);
                        break;
                    case BOX_PDIN:
                        retcode = parse_pdin(bitstr, &box_header, &mp4);
                        break;
                    case BOX_UDTA:
                        retcode = parse_unknown_box(bitstr, &box_header);
                        break;
                    case BOX_SIDX:
                        retcode = parse_unknown_box(bitstr, &box_header);
                        break;
                    case BOX_MOOV:
                        retcode = parse_moov(bitstr, &box_header, &mp4);
                        break;
                    case BOX_MOOF:
                        retcode = parse_unknown_box(bitstr, &box_header);
                        break;
                    case BOX_MDAT:
                        retcode = parse_mdat(bitstr, &box_header);
                        break;
                    case BOX_FREE:
                        retcode = parse_unknown_box(bitstr, &box_header);
                        break;
                    case BOX_UUID:
                        retcode = parse_unknown_box(bitstr, &box_header);
                        break;
                    default:
                        retcode = parse_unknown_box(bitstr, &box_header);
                        break;
                }

                jumpy_mp4(bitstr, NULL, &box_header);
            }
        }

        // File metadatas
        media->duration = (double)mp4.duration / (double)mp4.timescale * 1000.0;
        media->creation_time = (double)mp4.creation_time ;
        media->modification_time = (double)mp4.modification_time ;

        // Tracks metadatas
        // Check if we have extracted tracks
        if (mp4.tracks_count == 0)
        {
            TRACE_WARNING(MP4, "No tracks extracted!");
            retcode = FAILURE;
        }
        else // Convert tracks
        {
            unsigned int i = 0;
            for (i = 0; i < mp4.tracks_count; i++)
            {
                convertTrack(media, &mp4, mp4.tracks[i]);

                // Free track structure
                freeTrack(&(mp4.tracks[i]));
            }

            if (media->tracks_video_count == 0 &&  media->tracks_audio_count == 0)
            {
                TRACE_WARNING(MP4, "No tracks extracted!");
                retcode = FAILURE;
            }
            else
            {
                retcode = SUCCESS;
            }
        }

        // Free bitstream
        free_bitstream(&bitstr);
    }
    else
    {
        retcode = FAILURE;
    }

    return retcode;
}

/* ************************************************************************** */
