/*!
 * COPYRIGHT (C) 2014 Emeric Grange - All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \file      utils.h
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2014
 */

#ifndef UTILS_H
#define UTILS_H
/* ************************************************************************** */

#include "bitstream_map_struct.h"
#include <QString>

/* ************************************************************************** */

/*!
 * \brief Get a duration string from a duration in milliseconds.
 * \param duration: Duration in milliseconds.
 * \return Duration QString.
 */
QString getDurationString(const unsigned duration);

/*!
 * \brief Get a size string from a size in bytes.
 * \param size: File size in bytes.
 * \return File size QString.
 *
 * Print size in (G/M/K)iB, (G/M/K)B and bytes.
 */
QString getSizeString(const int64_t size);

/*!
 * \brief Compute a track size string from a track structure.
 * \param track: Track structure.
 * \param file_size: File size in bytes.
 * \param detailed: More precise sizes.
 * \return Track size QString.
 */
QString getTrackSizeString(const BitstreamMap_t *track, const int64_t file_size, const bool detailed = false);

/*!
 * \brief Get an aspect ratio string from a video definition.
 * \param x: Video width in pixel.
 * \param y: Video height in pixel.
 * \param detailed: Also print the name of the format.
 * \return QString with the aspect ratio.
 *
 * More infos: http://en.wikipedia.org/wiki/Aspect_ratio_(image)
 */
QString getAspectRatioString(const unsigned x, const unsigned y, bool detailed = false);
QString getAspectRatioString(const double ar, bool detailed = false);

/*!
 * \brief Get a bitrate string from a bitrate in bytes.
 * \param bitrate: Bitrate in bits.
 * \return Bitrate QString with the bitrate per second.
 *
 * Depending on the bitrate, the unit will be:
 * - Kb/s with 0 digits after the decimal point (if less than 10 Mb/s)
 * - Mb/s with 2 digits after the decimal point (if less than 100 Mb/s)
 * - Mb/s with 0 digits after the decimal point (if more than 100 Mb/s)
 */
QString getBitrateString(const unsigned bitrate);

QString getBitrateModeString(const unsigned bitrate_mode);

QString getFramerateModeString(const unsigned framerate_mode);

/* ************************************************************************** */
#endif // UTILS_H
