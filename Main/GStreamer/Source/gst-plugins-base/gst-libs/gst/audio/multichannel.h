/* GStreamer Multichannel-Audio helper functions
 * (c) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_AUDIO_MULTICHANNEL_H__
#define __GST_AUDIO_MULTICHANNEL_H__

#include <gst/audio/audio.h>
#include <gst/audio/audio-enumtypes.h>
#include <gst/audio/audio-channels.h>

G_BEGIN_DECLS

/* Retrieves or sets the positions from/to a GstStructure. Only
 * works with fixed caps, caller should check for that! Caller
 * g_free()s result of the getter. */
GstAudioChannelPosition *
        gst_audio_get_channel_positions (GstStructure *str);
void    gst_audio_set_channel_positions (GstStructure *str,
                                         const GstAudioChannelPosition *pos);

/* Sets a (non-fixed) list of possible audio channel positions
 * on a structure (this requires the "channels" property to
 * be fixed!) or on a caps (here, the "channels" property may be
 * unfixed and the caps may even contain multiple structures). */
void    gst_audio_set_structure_channel_positions_list
                                        (GstStructure *str,
                                         const GstAudioChannelPosition *pos,
                                         gint          num_positions);
void    gst_audio_set_caps_channel_positions_list
                                        (GstCaps      *caps,
                                         const GstAudioChannelPosition *pos,
                                         gint          num_positions);

/* Custom fixate function. Elements that implement some sort of
 * channel conversion algorithm should use this function for
 * fixating on GstAudioChannelPosition properties. It will take
 * care of equal channel positioning (left/right). Caller g_free()s
 * the return value. The input properties may be (and are supposed
 * to be) unfixed. */
GstAudioChannelPosition *
        gst_audio_fixate_channel_positions (GstStructure *str);

gboolean gst_audio_check_channel_positions (const GstAudioChannelPosition * pos, guint channels);

G_END_DECLS

#endif /* __GST_AUDIO_MULTICHANNEL_H__ */
