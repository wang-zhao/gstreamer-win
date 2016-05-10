/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowfakesink.cpp:
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

#include "gstdshowfakesink.h"

CDshowFakeSink::CDshowFakeSink():
    m_hres(S_OK),
    m_callback(NULL),
    m_data(NULL),
    CBaseRenderer(CLSID_DshowFakeSink, _T("DshowFakeSink"), NULL, &m_hres) {
}

STDMETHODIMP CDshowFakeSink::gst_set_media_type(AM_MEDIA_TYPE* pmt) {
    m_MediaType.Set(*pmt);
    return S_OK;
}

STDMETHODIMP
CDshowFakeSink::gst_set_buffer_callback(push_buffer_func push,
                                        gpointer data) {
    m_callback = push;
    m_data = data;
    return S_OK;
}

HRESULT CDshowFakeSink::WaitForRenderTime() {
    return S_OK;
}

HRESULT CDshowFakeSink::CheckMediaType(const CMediaType* pmt) {
    if (pmt != NULL) {
        if (*pmt == m_MediaType) {
            return S_OK;
        }
    }

    return S_FALSE;
}

HRESULT CDshowFakeSink::DoRenderSample(IMediaSample* pMediaSample) {
    if (pMediaSample && m_callback) {
        guint8* pBuffer = NULL;
        pMediaSample->GetPointer(&pBuffer);

        guint size = pMediaSample->GetActualDataLength();

        GstClockTimeDiff lStart = 0;
        GstClockTimeDiff lStop = 0;
        pMediaSample->GetTime(&lStart, &lStop);

        GstClockTime duration = (lStop - lStart) * 100;
        m_callback(pBuffer, size, m_data, duration);
    }

    return S_OK;
}
