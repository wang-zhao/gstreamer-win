/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 ObjectDescriptor sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


#include <internal/odf_dev.h>
#include <token.h>
#include <constants.h>
/*for import flags*/
#include <media_tools.h>

/* to complete...*/

u32 gf_odf_get_field_type(GF_Descriptor *desc, char *fieldName)
{
	switch (desc->tag) {
	case GF_ODF_IOD_TAG:
	case GF_ODF_OD_TAG:
		if (!_stricmp(fieldName, "esDescr")) return GF_ODF_FT_OD_LIST;
		else if (!_stricmp(fieldName, "ociDescr")) return GF_ODF_FT_OD_LIST;
		else if (!_stricmp(fieldName, "ipmpDescrPtr")) return GF_ODF_FT_OD_LIST;
		else if (!_stricmp(fieldName, "ipmpDescr")) return GF_ODF_FT_OD_LIST;
		else if (!_stricmp(fieldName, "extDescr")) return GF_ODF_FT_OD_LIST;
		else if (!_stricmp(fieldName, "toolListDescr")) return GF_ODF_FT_OD;
		return 0;
	case GF_ODF_DCD_TAG:
		if (!_stricmp(fieldName, "decSpecificInfo")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "profileLevelIndicationIndexDescr")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_ESD_TAG:
		if (!_stricmp(fieldName, "decConfigDescr")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "muxInfo")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "StreamSource")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "slConfigDescr")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "ipiPtr")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "qosDescr")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "regDescr")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "langDescr")) return GF_ODF_FT_OD;
		if (!_stricmp(fieldName, "ipIDS")) return GF_ODF_FT_OD_LIST;
		if (!_stricmp(fieldName, "ipmpDescrPtr")) return GF_ODF_FT_OD_LIST;
		if (!_stricmp(fieldName, "extDescr")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_TEXT_CFG_TAG:
		if (!_stricmp(fieldName, "SampleDescriptions")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_IPMP_TAG:
		if (!_stricmp(fieldName, "IPMPX_Data")) return GF_ODF_FT_IPMPX_LIST;
		return 0;
	case GF_ODF_IPMP_TL_TAG:
		if (!_stricmp(fieldName, "ipmpTool")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_IPMP_TOOL_TAG:
		if (!_stricmp(fieldName, "toolParamDesc")) return GF_ODF_FT_IPMPX;
		return 0;
	case GF_ODF_BIFS_CFG_TAG:
		if (!_stricmp(fieldName, "elementaryMask")) return GF_ODF_FT_OD_LIST;
		return 0;
	}
	return 0;
}

u32 gf_odf_get_tag_by_name(char *descName)
{
	if (!_stricmp(descName, "ObjectDescriptor")) return GF_ODF_OD_TAG;
	if (!_stricmp(descName, "InitialObjectDescriptor")) return GF_ODF_IOD_TAG;
	if (!_stricmp(descName, "ES_Descriptor")) return GF_ODF_ESD_TAG;
	if (!_stricmp(descName, "DecoderConfigDescriptor")) return GF_ODF_DCD_TAG;
	if (!_stricmp(descName, "DecoderSpecificInfo")) return GF_ODF_DSI_TAG;
	if (!_stricmp(descName, "DecoderSpecificInfoString")) return GF_ODF_DSI_TAG;
	if (!_stricmp(descName, "SLConfigDescriptor")) return GF_ODF_SLC_TAG;
	if (!_stricmp(descName, "ContentIdentification")) return GF_ODF_CI_TAG;
	if (!_stricmp(descName, "SuppContentIdentification")) return GF_ODF_SCI_TAG;
	if (!_stricmp(descName, "IPIPtr")) return GF_ODF_IPI_PTR_TAG;
	if (!_stricmp(descName, "IPMP_DescriptorPointer")) return GF_ODF_IPMP_PTR_TAG;
	if (!_stricmp(descName, "IPMP_Descriptor")) return GF_ODF_IPMP_TAG;
	if (!_stricmp(descName, "IPMP_ToolListDescriptor")) return GF_ODF_IPMP_TL_TAG;
	if (!_stricmp(descName, "IPMP_Tool")) return GF_ODF_IPMP_TOOL_TAG;
	if (!_stricmp(descName, "QoS")) return GF_ODF_QOS_TAG;
	if (!_stricmp(descName, "RegistrationDescriptor")) return GF_ODF_REG_TAG;
	if (!_stricmp(descName, "ExtensionPL")) return GF_ODF_EXT_PL_TAG;
	if (!_stricmp(descName, "PL_IndicationIndex")) return GF_ODF_PL_IDX_TAG;
	if (!_stricmp(descName, "ContentClassification")) return GF_ODF_CC_TAG;
	if (!_stricmp(descName, "KeyWordDescriptor")) return GF_ODF_KW_TAG;
	if (!_stricmp(descName, "RatingDescriptor")) return GF_ODF_RATING_TAG;
	if (!_stricmp(descName, "LanguageDescriptor")) return GF_ODF_LANG_TAG;
	if (!_stricmp(descName, "ShortTextualDescriptor")) return GF_ODF_SHORT_TEXT_TAG;
	if (!_stricmp(descName, "ExpandedTextualDescriptor")) return GF_ODF_TEXT_TAG;
	if (!_stricmp(descName, "ContentCreatorName")) return GF_ODF_CC_NAME_TAG;
	if (!_stricmp(descName, "ContentCreationDate")) return GF_ODF_CC_DATE_TAG;
	if (!_stricmp(descName, "OCI_CreatorName")) return GF_ODF_OCI_NAME_TAG;
	if (!_stricmp(descName, "OCI_CreationDate")) return GF_ODF_OCI_DATE_TAG;
	if (!_stricmp(descName, "SmpteCameraPosition")) return GF_ODF_SMPTE_TAG;
	if (!_stricmp(descName, "SegmentDescriptor")) return GF_ODF_SEGMENT_TAG;
	if (!_stricmp(descName, "MediaTimeDescriptor")) return GF_ODF_MEDIATIME_TAG;
	if (!_stricmp(descName, "MuxInfo")) return GF_ODF_MUXINFO_TAG;
	if (!_stricmp(descName, "StreamSource")) return GF_ODF_MUXINFO_TAG;
	if (!_stricmp(descName, "BIFSConfig") || !_stricmp(descName, "BIFSv2Config")) return GF_ODF_BIFS_CFG_TAG;
	if (!_stricmp(descName, "ElementaryMask")) return GF_ODF_ELEM_MASK_TAG;
	if (!_stricmp(descName, "TextConfig")) return GF_ODF_TEXT_CFG_TAG;
	if (!_stricmp(descName, "TextSampleDescriptor")) return GF_ODF_TX3G_TAG;
	if (!_stricmp(descName, "UIConfig")) return GF_ODF_UI_CFG_TAG;
	if (!_stricmp(descName, "ES_ID_Ref")) return GF_ODF_ESD_REF_TAG;
	if (!_stricmp(descName, "ES_ID_Inc")) return GF_ODF_ESD_INC_TAG;
	if (!_stricmp(descName, "AuxiliaryVideoData")) return GF_ODF_AUX_VIDEO_DATA;
	if (!_stricmp(descName, "DefaultDescriptor")) return GF_ODF_DSI_TAG;
	return 0;
}

#define GET_U8(field) { if (strstr(val, "0x")) { ret += sscanf(val, "%x", &d); if (ret) field = (u8) d; } else { ret += sscanf(val, "%d", &d); if (ret) field = (u8) d; }	}	
#define GET_U16(field) { if (strstr(val, "0x")) { ret += sscanf(val, "%x", &d); if (ret) field = (u16) d; } else { ret += sscanf(val, "%d", &d); if (ret) field = (u16) d; }	}	
#define GET_U32(field) { if (strstr(val, "0x")) { ret += sscanf(val, "%x", &d); if (ret) field = (u32) d; } else { ret += sscanf(val, "%d", &d); if (ret) field = (u32) d; }	}	
#define GET_S32(field) { if (strstr(val, "0x")) { ret += sscanf(val, "%x", &d); if (ret) field = (s32) d; } else { ret += sscanf(val, "%d", &d); if (ret) field = (s32) d; }	}	
#define GET_BOOL(field) { ret = 1; field = (!_stricmp(val, "true") || !_stricmp(val, "1")) ? 1 : 0; }

#define GET_DOUBLE(field) { Float v; ret = 1; sscanf(val, "%f", &v); field = (Double) v;}
#define GET_STRING(field) { ret = 1; field = _strdup(val); if (val[0] == '"') strcpy(field, val+1); if (field[strlen(field)-1] == '"') field[strlen(field)-1] = 0; }

void OD_ParseBinData(char *val, char **out_data, u32 *out_data_size)
{
	u32 i, c;
	char s[3];
	u32 len = strlen(val) / 3;
	if (*out_data) free(*out_data);
	*out_data_size = len;
	*out_data = (char*)malloc(sizeof(char) * len);
	s[2] = 0;
	for (i=0; i<len; i++) {
		s[0] = val[3*i+1];
		s[1] = val[3*i+2];
		sscanf(s, "%02X", &c);
		(*out_data)[i] = (unsigned char) c;
	}
}

void OD_ParseFileData(char *fileName, char **out_data, u32 *out_data_size)
{
	FILE *f;
	u32 size;
	if (*out_data) free(*out_data);
	*out_data = NULL;
	*out_data_size = 0;
	f = fopen(fileName, "rb");
	if (!f) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ODF Parse] cannot open data file %s - skipping\n", fileName));
		return;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*out_data_size = size;
	*out_data = (char*)malloc(sizeof(char) * size);
	fread(*out_data, sizeof(char) * size, 1, f);
	fclose(f);
}

void OD_ParseBin128(char *val, bin128 *data)
{
	if (!_strnicmp(val, "0x", 2)) val+=2;

	if (strlen(val)<16) {
		GF_BitStream *bs;
		u32 int_val = atoi(val);
		bs = gf_bs_new((char*) (*data), 16, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 0, 32);
		gf_bs_write_int(bs, 0, 32);
		gf_bs_write_int(bs, 0, 32);
		gf_bs_write_int(bs, int_val, 32);
		gf_bs_del(bs);
	} else {
		u32 i, b;
		char szB[3];
		szB[2] = 0;
		for (i=0; i<16; i++) {
			szB[0] = val[2*i];
			szB[1] = val[2*i+1];
			sscanf(szB, "%x", &b);
			((char *)data)[i] = (u8) b;
		}
	}
}


GF_Err gf_odf_set_field(GF_Descriptor *desc, char *fieldName, char *val)
{
	Bool OD_ParseUIConfig(char *val, char **out_data, u32 *out_data_size);
	u32 d, ret = 0;

	if (!_stricmp(val, "auto")) return GF_OK;
	else if (!_stricmp(val, "unspecified")) return GF_OK;

	switch (desc->tag) {
	case GF_ODF_IOD_TAG:
	{
		GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *)desc;
		if (!_stricmp(fieldName, "objectDescriptorID") || !_stricmp(fieldName, "binaryID")) ret += sscanf(val, "%hd", &iod->objectDescriptorID);
		else if (!_stricmp(fieldName, "URLString")) {
			iod->URLString = _strdup(val);
			ret = 1;
		}
		else if (!_stricmp(fieldName, "includeInlineProfileLevelFlag")) {
			GET_BOOL(iod->inlineProfileFlag)
			if (!ret) { iod->inlineProfileFlag = 0; ret = 1; }
		}
		else if (!_stricmp(fieldName, "ODProfileLevelIndication")) {
			GET_U8(iod->OD_profileAndLevel)
			if (!ret) { iod->OD_profileAndLevel = 0xFE; ret = 1; }
		}
		else if (!_stricmp(fieldName, "sceneProfileLevelIndication")) {
			GET_U8(iod->scene_profileAndLevel)
			if (!ret) { iod->scene_profileAndLevel = 0xFE; ret = 1; }
		}
		else if (!_stricmp(fieldName, "audioProfileLevelIndication")) {
			GET_U8(iod->audio_profileAndLevel)
			if (!ret) { iod->audio_profileAndLevel = 0xFE; ret = 1; }
		}
		else if (!_stricmp(fieldName, "visualProfileLevelIndication")) {
			GET_U8(iod->visual_profileAndLevel)
			if (!ret) { iod->visual_profileAndLevel = 0xFE; ret = 1; }
		}
		else if (!_stricmp(fieldName, "graphicsProfileLevelIndication")) {
			GET_U8(iod->graphics_profileAndLevel)
			if (!ret) { iod->graphics_profileAndLevel = 0xFE; ret = 1; }
		}
	}
		break;
	case GF_ODF_OD_TAG:
	{
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) desc;
		if (!_stricmp(fieldName, "objectDescriptorID") || !_stricmp(fieldName, "binaryID")) ret += sscanf(val, "%hd", &od->objectDescriptorID);
		else if (!_stricmp(fieldName, "URLString")) {
			od->URLString = _strdup(val);
			ret = 1;
		}
	}
		break;
	case GF_ODF_DCD_TAG:
	{
		GF_DecoderConfig *dcd = (GF_DecoderConfig *)desc;
		if (!_stricmp(fieldName, "objectTypeIndication")) {
			GET_U8(dcd->objectTypeIndication)
			/*XMT may use string*/
			if (!ret) {
				if (!_stricmp(val, "MPEG4Systems1")) { dcd->objectTypeIndication = 0x01; ret = 1; }
				else if (!_stricmp(val, "MPEG4Systems2")) { dcd->objectTypeIndication = 0x02; ret = 1; }
				else if (!_stricmp(val, "MPEG4Visual")) { dcd->objectTypeIndication = 0x20; ret = 1; }
				else if (!_stricmp(val, "MPEG4Audio")) { dcd->objectTypeIndication = 0x40; ret = 1; }
				else if (!_stricmp(val, "MPEG2VisualSimple")) { dcd->objectTypeIndication = 0x60; ret = 1; }
				else if (!_stricmp(val, "MPEG2VisualMain")) { dcd->objectTypeIndication = 0x61; ret = 1; }
				else if (!_stricmp(val, "MPEG2VisualSNR")) { dcd->objectTypeIndication = 0x62; ret = 1; }
				else if (!_stricmp(val, "MPEG2VisualSpatial")) { dcd->objectTypeIndication = 0x63; ret = 1; }
				else if (!_stricmp(val, "MPEG2VisualHigh")) { dcd->objectTypeIndication = 0x64; ret = 1; }
				else if (!_stricmp(val, "MPEG2Visual422")) { dcd->objectTypeIndication = 0x65; ret = 1; }
				else if (!_stricmp(val, "MPEG2AudioMain")) { dcd->objectTypeIndication = 0x66; ret = 1; }
				else if (!_stricmp(val, "MPEG2AudioLowComplexity")) { dcd->objectTypeIndication = 0x67; ret = 1; }
				else if (!_stricmp(val, "MPEG2AudioScaleableSamplingRate")) { dcd->objectTypeIndication = 0x68; ret = 1; }
				else if (!_stricmp(val, "MPEG2AudioPart3")) { dcd->objectTypeIndication = 0x69; ret = 1; }
				else if (!_stricmp(val, "MPEG1Visual")) { dcd->objectTypeIndication = 0x6A; ret = 1; }
				else if (!_stricmp(val, "MPEG1Audio")) { dcd->objectTypeIndication = 0x6B; ret = 1; }
				else if (!_stricmp(val, "JPEG")) { dcd->objectTypeIndication = 0x6C; ret = 1; }
				else if (!_stricmp(val, "PNG")) { dcd->objectTypeIndication = 0x6D; ret = 1; }
			}
		}
		else if (!_stricmp(fieldName, "streamType")) {
			GET_U8(dcd->streamType)
			/*XMT may use string*/
			if (!ret) {
				if (!_stricmp(val, "ObjectDescriptor")) { dcd->streamType = GF_STREAM_OD; ret = 1; } 
				else if (!_stricmp(val, "ClockReference")) { dcd->streamType = GF_STREAM_OCR; ret = 1; }
				else if (!_stricmp(val, "SceneDescription")) { dcd->streamType = GF_STREAM_SCENE; ret = 1; }
				else if (!_stricmp(val, "Visual")) { dcd->streamType = GF_STREAM_VISUAL; ret = 1; }
				else if (!_stricmp(val, "Audio")) { dcd->streamType = GF_STREAM_AUDIO; ret = 1; }
				else if (!_stricmp(val, "MPEG7")) { dcd->streamType = GF_STREAM_MPEG7; ret = 1; }
				else if (!_stricmp(val, "IPMP")) { dcd->streamType = GF_STREAM_IPMP; ret = 1; }
				else if (!_stricmp(val, "OCI")) { dcd->streamType = GF_STREAM_OCI; ret = 1; }
				else if (!_stricmp(val, "MPEGJ")) { dcd->streamType = GF_STREAM_MPEGJ; ret = 1; }
			}
		}
		else if (!_stricmp(fieldName, "upStream")) GET_BOOL(dcd->upstream)
		else if (!_stricmp(fieldName, "bufferSizeDB")) ret += sscanf(val, "%d", &dcd->bufferSizeDB);
		else if (!_stricmp(fieldName, "maxBitRate")) ret += sscanf(val, "%d", &dcd->maxBitrate);
		else if (!_stricmp(fieldName, "avgBitRate")) ret += sscanf(val, "%d", &dcd->avgBitrate);
	}
		break;
	case GF_ODF_ESD_TAG:
	{
		GF_ESD *esd = (GF_ESD *)desc;
		if (!_stricmp(fieldName, "ES_ID") || !_stricmp(fieldName, "binaryID")) {
			ret += sscanf(val, "%hd", &esd->ESID);
		}
		else if (!_stricmp(fieldName, "streamPriority")) GET_U8(esd->streamPriority)
		else if (!_stricmp(fieldName, "dependsOn_ES_ID") || !_stricmp(fieldName, "dependsOnESID")) ret += sscanf(val, "%hd", &esd->dependsOnESID);
		else if (!_stricmp(fieldName, "OCR_ES_ID")) ret += sscanf(val, "%hd", &esd->OCRESID);
		else if (!_stricmp(fieldName, "URLstring")) {
			esd->URLString = _strdup(val);
			ret = 1;
		}
		/*ignore*/
		else if (!_stricmp(fieldName, "streamDependenceFlag")
			|| !_stricmp(fieldName, "URL_Flag")
			|| !_stricmp(fieldName, "OCRstreamFlag")
			) 
			ret = 1;
	}
		break;
	case GF_ODF_SLC_TAG:
	{
		u32 ts;
		GF_SLConfig *slc = (GF_SLConfig*)desc;
		if (!_stricmp(fieldName, "predefined")) GET_U8(slc->predefined)
		else if (!_stricmp(fieldName, "useAccessUnitStartFlag")) GET_BOOL(slc->useAccessUnitStartFlag)
		else if (!_stricmp(fieldName, "useAccessUnitEndFlag")) GET_BOOL(slc->useAccessUnitEndFlag)
		else if (!_stricmp(fieldName, "useRandomAccessPointFlag")) GET_BOOL(slc->useRandomAccessPointFlag)
		else if (!_stricmp(fieldName, "hasRandomAccessUnitsOnlyFlag") || !_stricmp(fieldName, "useRandomAccessUnitsOnlyFlag")) GET_BOOL(slc->hasRandomAccessUnitsOnlyFlag)
		else if (!_stricmp(fieldName, "usePaddingFlag")) GET_BOOL(slc->usePaddingFlag)
		else if (!_stricmp(fieldName, "useTimeStampsFlag")) GET_BOOL(slc->useTimestampsFlag)
		else if (!_stricmp(fieldName, "useIdleFlag")) GET_BOOL(slc->useIdleFlag)
		else if (!_stricmp(fieldName, "timeStampResolution")) ret += sscanf(val, "%d", &slc->timestampResolution);
		else if (!_stricmp(fieldName, "OCRResolution")) ret += sscanf(val, "%d", &slc->OCRResolution);
		else if (!_stricmp(fieldName, "timeStampLength")) GET_U8(slc->timestampLength)
		else if (!_stricmp(fieldName, "OCRLength")) GET_U8(slc->OCRLength)
		else if (!_stricmp(fieldName, "AU_Length")) GET_U8(slc->AULength)
		else if (!_stricmp(fieldName, "instantBitrateLength")) GET_U8(slc->instantBitrateLength)
		else if (!_stricmp(fieldName, "degradationPriorityLength")) GET_U8(slc->degradationPriorityLength)
		else if (!_stricmp(fieldName, "AU_seqNumLength")) GET_U8(slc->AUSeqNumLength)
		else if (!_stricmp(fieldName, "packetSeqNumLength")) GET_U8(slc->packetSeqNumLength)
		else if (!_stricmp(fieldName, "timeScale")) ret += sscanf(val, "%d", &slc->timeScale);
		else if (!_stricmp(fieldName, "accessUnitDuration")) ret += sscanf(val, "%hd", &slc->AUDuration);
		else if (!_stricmp(fieldName, "compositionUnitDuration")) ret += sscanf(val, "%hd", &slc->CUDuration);
		else if (!_stricmp(fieldName, "startDecodingTimeStamp")) {
			ret += sscanf(val, "%d", &ts);
			slc->startDTS = ts;
		}
		else if (!_stricmp(fieldName, "startCompositionTimeStamp")) {
			ret += sscanf(val, "%d", &ts);
			slc->startCTS = ts;
		}
		else if (!_stricmp(fieldName, "durationFlag")) ret = 1;
	}	
		break;
	case GF_ODF_ELEM_MASK_TAG:
	{
		GF_ElementaryMask* em = (GF_ElementaryMask*)desc;
		if (!_stricmp(fieldName, "atNode")) {
			GET_U32(em->node_id);
			if (!ret || !em->node_id) em->node_name = _strdup(val);
			ret = 1;
		}
		else if (!_stricmp(fieldName, "numDynFields")) ret = 1;
	}
		break;
	case GF_ODF_BIFS_CFG_TAG:
	{
		s32 notused;
		GF_BIFSConfig *bcd = (GF_BIFSConfig*)desc;
		if (!_stricmp(val, "auto")) return GF_OK;
		if (!_stricmp(fieldName, "nodeIDbits")) ret += sscanf(val, "%hd", &bcd->nodeIDbits);
		else if (!_stricmp(fieldName, "nodeIDbits")) ret += sscanf(val, "%hd", &bcd->nodeIDbits);
		else if (!_stricmp(fieldName, "routeIDbits")) ret += sscanf(val, "%hd", &bcd->routeIDbits);
		else if (!_stricmp(fieldName, "protoIDbits")) ret += sscanf(val, "%hd", &bcd->protoIDbits);
		else if (!_stricmp(fieldName, "isCommandStream")) { /*GET_BOOL(bcd->isCommandStream)*/ ret = 1; }
		else if (!_stricmp(fieldName, "pixelMetric") || !_stricmp(fieldName, "pixelMetrics")) GET_BOOL(bcd->pixelMetrics)
		else if (!_stricmp(fieldName, "pixelWidth")) ret += sscanf(val, "%hd", &bcd->pixelWidth);
		else if (!_stricmp(fieldName, "pixelHeight")) ret += sscanf(val, "%hd", &bcd->pixelHeight);
		else if (!_stricmp(fieldName, "use3DMeshCoding")) GET_BOOL(notused)
		else if (!_stricmp(fieldName, "usePredictiveMFField")) GET_BOOL(notused)
		else if (!_stricmp(fieldName, "randomAccess")) GET_BOOL(bcd->randomAccess)
		else if (!_stricmp(fieldName, "useNames")) GET_BOOL(bcd->useNames)
	}
		break;
	case GF_ODF_MUXINFO_TAG:
	{
		GF_MuxInfo *mi = (GF_MuxInfo *)desc;
		if (!_stricmp(fieldName, "fileName") || !_stricmp(fieldName, "url")) GET_STRING(mi->file_name)
		else if (!_stricmp(fieldName, "streamFormat")) GET_STRING(mi->streamFormat)
		else if (!_stricmp(fieldName, "GroupID")) ret += sscanf(val, "%d", &mi->GroupID);
		else if (!_stricmp(fieldName, "startTime")) ret += sscanf(val, "%d", &mi->startTime);
		else if (!_stricmp(fieldName, "duration")) ret += sscanf(val, "%d", &mi->duration);
		else if (!_stricmp(fieldName, "compactSize"))
		{ ret = 1; if (!_stricmp(val, "true") || !_stricmp(val, "1")) mi->import_flags |= GF_IMPORT_USE_COMPACT_SIZE; }
		else if (!_stricmp(fieldName, "useDataReference"))
		{ ret = 1; if (!_stricmp(val, "true") || !_stricmp(val, "1")) mi->import_flags |= GF_IMPORT_USE_DATAREF; }
		else if (!_stricmp(fieldName, "noFrameDrop"))
		{ ret = 1; if (!_stricmp(val, "true") || !_stricmp(val, "1")) mi->import_flags |= GF_IMPORT_NO_FRAME_DROP; }
		else if (!_stricmp(fieldName, "SBR_Type")) { 
			ret = 1; 
			if (!_stricmp(val, "implicit") || !_stricmp(val, "1")) mi->import_flags |= GF_IMPORT_SBR_IMPLICIT;
			else if (!_stricmp(val, "explicit") || !_stricmp(val, "2")) mi->import_flags |= GF_IMPORT_SBR_EXPLICIT;
		}

		else if (!_stricmp(fieldName, "textNode")) GET_STRING(mi->textNode)
		else if (!_stricmp(fieldName, "fontNode")) GET_STRING(mi->fontNode)
		else if (!_stricmp(fieldName, "frameRate")) { ret = 1; mi->frame_rate = atof(val); }
	}
		break;
	case GF_ODF_DSI_TAG:
	{
		GF_DefaultDescriptor *dsi = (GF_DefaultDescriptor*)desc;
		if (!_stricmp(fieldName, "info")) {
			/*only parse true hexa strings*/
			if (val[0] == '%') {
				OD_ParseBinData(val, &dsi->data, &dsi->dataLength);
				ret = 1;
			} else if (!_strnicmp(val, "file:", 5)) {
				OD_ParseFileData(val+5, &dsi->data, &dsi->dataLength);
				ret = 1;
			} else if (!strlen(val)) ret = 1;
		}
		if (!_stricmp(fieldName, "src")) {
			u32 len = strlen("data:application/octet-string,");
			if (_strnicmp(val, "data:application/octet-string,", len)) break;
			val += len;
			/*only parse true hexa strings*/
			if (val[0] == '%') {
				OD_ParseBinData(val, &dsi->data, &dsi->dataLength);
				ret = 1;
			} else if (!_strnicmp(val, "file:", 5)) {
				OD_ParseFileData(val+5, &dsi->data, &dsi->dataLength);
				ret = 1;
			}
		}
	}
		break;
	case GF_ODF_SEGMENT_TAG:
	{
		GF_Segment *sd = (GF_Segment*)desc;
		if (!_stricmp(fieldName, "start") || !_stricmp(fieldName, "startTime")) GET_DOUBLE(sd->startTime)
		else if (!_stricmp(fieldName, "duration")) GET_DOUBLE(sd->Duration)
		else if (!_stricmp(fieldName, "name") || !_stricmp(fieldName, "segmentName")) GET_STRING(sd->SegmentName)
	}
		break;
	case GF_ODF_UI_CFG_TAG:
	{
		GF_UIConfig *uic = (GF_UIConfig*)desc;
		if (!_stricmp(fieldName, "deviceName")) GET_STRING(uic->deviceName)
		else if (!_stricmp(fieldName, "termChar")) GET_U8(uic->termChar)
		else if (!_stricmp(fieldName, "delChar")) GET_U8(uic->delChar)
		else if (!_stricmp(fieldName, "uiData")) {
			/*only parse true hexa strings*/
			if (val[0] == '%') {
				OD_ParseBinData(val, &uic->ui_data, &uic->ui_data_length);
				ret = 1;
			} else if (!_strnicmp(val, "file:", 5)) {
				OD_ParseFileData(val+5, &uic->ui_data, &uic->ui_data_length);
				ret = 1;
			} else {
				ret = OD_ParseUIConfig(val, &uic->ui_data, &uic->ui_data_length);
			}
		}
	}
		break;
	case GF_ODF_ESD_INC_TAG:
	{
		GF_ES_ID_Inc *inc = (GF_ES_ID_Inc *)desc;
		if (!_stricmp(fieldName, "trackID")) ret += sscanf(val, "%d", &inc->trackID);
	}
		break;
	case GF_ODF_ESD_REF_TAG:
	{
		GF_ES_ID_Ref *inc = (GF_ES_ID_Ref *)desc;
		if (!_stricmp(fieldName, "trackID")) ret += sscanf(val, "%hd", &inc->trackRef);
	}
		break;
	case GF_ODF_TEXT_CFG_TAG:
	{
		GF_TextConfig *txt = (GF_TextConfig*)desc;
		if (!_stricmp(fieldName, "3GPPBaseFormat")) GET_U8(txt->Base3GPPFormat)
		else if (!_stricmp(fieldName, "MPEGExtendedFormat")) GET_U8(txt->MPEGExtendedFormat)
		else if (!_stricmp(fieldName, "profileLevel")) GET_U8(txt->profileLevel)
		else if (!_stricmp(fieldName, "durationClock") || !_stricmp(fieldName, "timescale") ) GET_U32(txt->timescale)
		else if (!_stricmp(fieldName, "layer")) GET_U32(txt->layer)
		else if (!_stricmp(fieldName, "text_width")) GET_U32(txt->text_width)
		else if (!_stricmp(fieldName, "text_height")) GET_U32(txt->text_height)
		else if (!_stricmp(fieldName, "video_width")) {
			GET_U32(txt->video_width)
			txt->has_vid_info = 1;
		}
		else if (!_stricmp(fieldName, "video_height")) {
			GET_U32(txt->video_height)
			txt->has_vid_info = 1;
		}
		else if (!_stricmp(fieldName, "horizontal_offset")) {
			GET_S32(txt->horiz_offset)
			txt->has_vid_info = 1;
		}
		else if (!_stricmp(fieldName, "vertical_offset")) {
			GET_S32(txt->vert_offset)
			txt->has_vid_info = 1;
		}
	}
		break;
	case GF_ODF_TX3G_TAG:
	{
		GF_TextSampleDescriptor *sd = (GF_TextSampleDescriptor*)desc;
		if (!_stricmp(fieldName, "displayFlags")) GET_U32(sd->displayFlags)
		else if (!_stricmp(fieldName, "horiz_justif")) GET_S32(sd->horiz_justif)
		else if (!_stricmp(fieldName, "vert_justif")) GET_S32(sd->vert_justif)
		else if (!_stricmp(fieldName, "back_color")) GET_S32(sd->back_color)
		else if (!_stricmp(fieldName, "top")) GET_S32(sd->default_pos.top)
		else if (!_stricmp(fieldName, "bottom")) GET_S32(sd->default_pos.bottom)
		else if (!_stricmp(fieldName, "left")) GET_S32(sd->default_pos.left)
		else if (!_stricmp(fieldName, "right")) GET_S32(sd->default_pos.right)
		else if (!_stricmp(fieldName, "style_font_ID")) GET_S32(sd->default_style.fontID)
		else if (!_stricmp(fieldName, "style_font_size")) GET_S32(sd->default_style.font_size)
		else if (!_stricmp(fieldName, "style_text_color")) GET_U32(sd->default_style.text_color)
		else if (!_stricmp(fieldName, "style_flags")) {
			char szStyles[1024];
			strcpy(szStyles, val);
			_strlwr(szStyles);
			if (strstr(szStyles, "bold")) sd->default_style.style_flags |= GF_TXT_STYLE_BOLD;
			if (strstr(szStyles, "italic")) sd->default_style.style_flags |= GF_TXT_STYLE_ITALIC;
			if (strstr(szStyles, "underlined")) sd->default_style.style_flags |= GF_TXT_STYLE_UNDERLINED;
			ret = 1;
		}
		else if (!_stricmp(fieldName, "fontID") || !_stricmp(fieldName, "fontName")) {
			/*check if we need a new entry*/
			if (!sd->font_count) {
				sd->fonts = (GF_FontRecord*)malloc(sizeof(GF_FontRecord));
				sd->font_count = 1;
				sd->fonts[0].fontID = 0;
				sd->fonts[0].fontName = NULL;
			} else {
				Bool realloc_fonts = 0;
				if (!_stricmp(fieldName, "fontID") && sd->fonts[sd->font_count-1].fontID) realloc_fonts = 1;
				else if (!_stricmp(fieldName, "fontName") && sd->fonts[sd->font_count-1].fontName) realloc_fonts = 1;
				if (realloc_fonts) {
					sd->font_count += 1;
					sd->fonts = (GF_FontRecord*)realloc(sd->fonts, sizeof(GF_FontRecord)*sd->font_count);
					sd->fonts[sd->font_count-1].fontID = 0;
					sd->fonts[sd->font_count-1].fontName = NULL;
				}
			}
			if (!_stricmp(fieldName, "fontID")) GET_U32(sd->fonts[sd->font_count-1].fontID)
			if (!_stricmp(fieldName, "fontName")) GET_STRING(sd->fonts[sd->font_count-1].fontName)
			ret = 1;
		}
	}
		break;
	case GF_ODF_IPMP_TAG:
	{
		GF_IPMP_Descriptor *ipmp = (GF_IPMP_Descriptor*)desc;
		if (!_stricmp(fieldName, "IPMP_DescriptorID")) GET_U8(ipmp->IPMP_DescriptorID)
		else if (!_stricmp(fieldName, "IPMPS_Type")) GET_U16(ipmp->IPMPS_Type)
		else if (!_stricmp(fieldName, "IPMP_DescriptorIDEx")) GET_U16(ipmp->IPMP_DescriptorIDEx)
		else if (!_stricmp(fieldName, "IPMP_ToolID")) { ret = 1; OD_ParseBin128(val, &ipmp->IPMP_ToolID); }
		else if (!_stricmp(fieldName, "controlPointCode")) GET_U8(ipmp->control_point)
		else if (!_stricmp(fieldName, "sequenceCode")) GET_U8(ipmp->cp_sequence_code)
	}
		break;
	case GF_ODF_IPMP_PTR_TAG:
	{
		GF_IPMPPtr *ipmpd = (GF_IPMPPtr*)desc;
		if (!_stricmp(fieldName, "IPMP_DescriptorID")) GET_U8(ipmpd->IPMP_DescriptorID)
		else if (!_stricmp(fieldName, "IPMP_DescriptorIDEx"))  ret += sscanf(val, "%hd", &ipmpd->IPMP_DescriptorIDEx);
		else if (!_stricmp(fieldName, "IPMP_ES_ID"))  ret += sscanf(val, "%hd", &ipmpd->IPMP_ES_ID);
	}
		break;
	case GF_ODF_LANG_TAG:
		{
			GF_Language *ld = (GF_Language *)desc;
			if (!_stricmp(fieldName, "languageCode")) {
				u32 li, l = strlen(val);
				ld->langCode = 0;
				for (li = 0; li < l; li++) {
					/* Warning: sensitive to big endian, little endian */
					ld->langCode |= (val[li] << (l-li-1)*8);
				}
				ret++;
			}
		}
		break;
	case GF_ODF_AUX_VIDEO_DATA:
	{
		GF_AuxVideoDescriptor *avd = (GF_AuxVideoDescriptor *)desc;
		if (!_stricmp(fieldName, "aux_video_type"))  GET_U8(avd->aux_video_type)
		if (!_stricmp(fieldName, "position_offset_h"))  GET_U8(avd->aux_video_type)
		if (!_stricmp(fieldName, "position_offset_v"))  GET_U8(avd->aux_video_type)
		if (!_stricmp(fieldName, "knear"))  GET_U8(avd->aux_video_type)
		if (!_stricmp(fieldName, "kfar"))  GET_U8(avd->aux_video_type)
		if (!_stricmp(fieldName, "parallax_zero"))  GET_U16(avd->aux_video_type)
		if (!_stricmp(fieldName, "parallax_scale"))  GET_U16(avd->aux_video_type)
		if (!_stricmp(fieldName, "dref"))  GET_U16(avd->aux_video_type)
		if (!_stricmp(fieldName, "wref"))  GET_U16(avd->aux_video_type)
	}
		break;
	case GF_ODF_IPMP_TOOL_TAG:
	{
		GF_IPMP_Tool *it = (GF_IPMP_Tool*)desc;
		if (!_stricmp(fieldName, "IPMP_ToolID")) { ret = 1; OD_ParseBin128(val, &it->IPMP_ToolID); }
		else if (!_stricmp(fieldName, "ToolURL"))  GET_STRING(it->tool_url)
	}
		break;
	}

	return ret ? GF_OK : GF_BAD_PARAM;
}


Bool OD_ParseUIConfig(char *val, char **out_data, u32 *out_data_size)
{
	GF_BitStream *bs;
	if (!_strnicmp(val, "HTK:", 4)) {
		char szItem[100];
		s32 pos, bs_start, bs_cur;
		Bool has_word;
		u32 nb_phonems, nbWords = 0;
		bs_start = 0;
		nb_phonems = 0;
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*we'll write the nb of words later on*/
		gf_bs_write_int(bs, 0, 8);
		has_word = 0;
		/*parse all words*/
		val += 4;
		while (1) {
			pos = gf_token_get(val, 0, " ;", szItem, 100);
			if (pos>0) val += pos;
			if (!has_word) {
				has_word = 1;
				nbWords++;
				nb_phonems = 0;
				bs_start = (u32) gf_bs_get_position(bs);
				/*nb phonems*/
				gf_bs_write_int(bs, 0, 8);
				gf_bs_write_data(bs, szItem, strlen(szItem));
				gf_bs_write_int(bs, 0, 8);
				continue;
			}
			if (pos>0) {
		
				nb_phonems ++;
				/*would be nicer with a phone book & use indexes*/
				if (!_stricmp(szItem, "vcl")) {
					gf_bs_write_data(bs, "vc", 2);
				} else {
					gf_bs_write_data(bs, szItem, 2);
				}

				while (val[0] && (val[0]==' ')) val += 1;
			}

			if ((pos<0) || !val[0] || val[0]==';') {
				if (has_word) {
					has_word = 0;
					bs_cur = (u32) gf_bs_get_position(bs);
					gf_bs_seek(bs, bs_start);
					gf_bs_write_int(bs, nb_phonems, 8);
					gf_bs_seek(bs, bs_cur);
				}
				if ((pos<0) || !val[0]) break;
				val += 1;
				while (val[0] && (val[0]==' ')) val += 1;
			}
		}
		if (nbWords) {
			bs_cur = (u32) gf_bs_get_position(bs);
			gf_bs_seek(bs, 0);
			gf_bs_write_int(bs, nbWords, 8);
			gf_bs_seek(bs, bs_cur);
			gf_bs_get_content(bs, out_data, out_data_size);
		}
		gf_bs_del(bs);
		return 1;
	}
	return 0;
}

