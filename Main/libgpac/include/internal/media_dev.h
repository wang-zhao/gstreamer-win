/*
*			GPAC - Multimedia Framework C SDK
*
*			Copyright (c) Jean Le Feuvre 2000-2005 
*					All rights reserved
*
*  This file is part of GPAC / Media Tools sub-project
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


#ifndef _GF_MEDIA_DEV_H_
#define _GF_MEDIA_DEV_H_

#include <media_tools.h>

#ifdef __cplusplus
extern "C"{
#endif

  GF_Err gf_import_message(GF_MediaImporter *import, GF_Err e, char *format, ...);

  /*returns 0 if not a start code, or size of start code (3 or 4 bytes). If start code, bitstream
  is positionned AFTER start code*/
  u32 AVC_IsStartCode(GF_BitStream *bs);
  /*returns size of chunk between current and next startcode (excluding startcode sizes), 0 if no more startcodes (eos)*/
  u32 AVC_NextStartCode(GF_BitStream *bs);
  /*returns NAL unit type - bitstream must be sync'ed!!*/
  u8 AVC_NALUType(GF_BitStream *bs);
  /*slice NALU*/
  Bool AVC_NALUIsSlice(u8 type);
  Bool AVC_ChangeTimeScale(u8* buf, u32* buflen,s32 tick, s32 tms );


  typedef struct
  {
    s32 profile_idc;
    s32 level_idc;
    s32 prof_compat;
    s32 log2_max_frame_num;
    u32 chroma_format_idc;
    u32 poc_type, poc_cycle_length;
    s32 log2_max_poc_lsb;
    s32 delta_pic_order_always_zero_flag;
    s32 offset_for_non_ref_pic, offset_for_top_to_bottom_field;
    Bool frame_mbs_only_flag;

    s16 offset_for_ref_frame[256];

    s32 timing_info_present_flag;
    u32 num_units_in_tick;
    u32 time_scale;
    s32 fixed_frame_rate_flag;

    s32 video_format;

    Bool residual_colour_transform_flag;
    u32 bit_depth_luma_minus8;
    u32 bit_depth_chroma_minus8;
    Bool qpprime_y_zero_transform_bypass_flag;
    Bool seq_scaling_matrix_present_flag;

    u32 ref_frame_count;
    Bool gaps_in_frame_num_allowed_flag;

    Bool mb_adaptive_frame_field_flag;
    Bool direct_8x8_inference_flag;
    Bool crop; 
    u32 cl;
    u32 cr;
    u32 ct;
    u32 cb;

    u32 vui_flag_pos;

    s32 aspect_ratio_idc;

    Bool vui_parameters_present_flag;
    Bool aspect_ratio_info_present_flag;

    Bool overscan_info_present_flag ;
    u32 overscan_appropriate_flag ;

    Bool video_signal_type_present_flag;
    Bool video_full_range_flag;
    Bool colour_description_present_flag;

    u32 colour_primaries; 
    u32 transfer_characteristics; 
    u32 matrix_coefficients;

    Bool chroma_location_info_present_flag;

    u32 chroma_sample_location_type_top_field;
    u32 chroma_sample_location_type_bottom_field ;



    //vui
    s32 nal_hrd_parameters_present_flag;
    u32 cpb_cnt_minus1;
    s32	bit_rate_scale;
    s32	cpb_size_scale;

    s32 bit_rate_value_minus1[32];
    s32 cpb_size_value_minus1[32];
    s32 cbr_flag[32];

    s32 initial_cpb_removal_delay_length_minus1;
    s32 cpb_removal_delay_length_minus1;
    s32 dpb_output_delay_length_minus1;
    s32 time_offset_length;

    u32 width, height;
    u32 par_num, par_den;
    /*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 sent*/
    u32 status;
  } AVC_SPS;

  typedef struct 
  {
    s32 sps_id;
    s32 pic_order_present;      /* pic_order_present_flag*/
    s32 redundant_pic_cnt_present; /* redundant_pic_cnt_present_flag */
    int slice_group_count;      /* num_slice_groups_minus1 + 1*/
    /*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 sent*/
    u32 status;

    Bool cabac;
    u32 mb_slice_group_map_type;
    u32 ref_count[2];
    Bool weighted_pred;
    s32 weighted_bipred_idc;
    s32 init_qp;
    s32 init_qs;
    s32 chroma_qp_index_offset;
    Bool deblocking_filter_parameters_present;
    Bool constrained_intra_pred ;

  } AVC_PPS;

  typedef struct
  {
    u8 nal_ref_idc, nal_unit_type, field_pic_flag, bottom_field_flag;
    u32 frame_num, idr_pic_id, poc_lsb, slice_type;
    s32 delta_poc_bottom;
    s32 delta_poc[2];
    s32 redundant_pic_cnt;

    s32 poc;
    u32 poc_msb, poc_msb_prev, poc_lsb_prev, frame_num_prev;
    s32 frame_num_offset, frame_num_offset_prev;

    s32 poc_start;
    s32 poc_base;

    AVC_SPS *sps;
    AVC_PPS *pps;
  } AVCSliceInfo;


  typedef struct 
  {
    u32 frame_cnt;
    u8 exact_match_flag;
    u8 broken_link_flag;
    u8 changing_slice_group_idc;
    u8 valid;
  } AVCSeiRecoveryPoint;


  typedef struct 
  {
    AVCSeiRecoveryPoint recovery_point;
    /*to be eventually completed by other sei*/

  } AVCSei;

  typedef struct
  {
    AVC_SPS sps[32];
    AVC_PPS pps[255];

    AVCSliceInfo s_info;
    AVCSei sei;
  } AVCState;


  /*return sps ID or -1 if error*/
  s32 AVC_ReadSeqInfo(GF_BitStream *bs, AVCState *avc, u32 *vui_flag_pos);
  /*return pps ID or -1 if error*/
  s32 AVC_ReadPictParamSet(GF_BitStream *bs, AVCState *avc);
  /*is slice a RAP*/
  Bool AVC_SliceIsIDR(AVCState *avc);
  /*parses NALU, updates avc state and returns:
  1 if NALU part of new frame
  0 if NALU part of prev frame
  -1 if bitstream error
  */
  s32 AVC_ParseNALU(GF_BitStream *bs, u32 nal_hdr, AVCState *avc);
  /*remove SEI messages not allowed in MP4*/
  u32 AVC_ReformatSEI_NALU(char *buffer, u32 nal_size, AVCState *avc);

  GF_Err AVC_ChangePAR(GF_AVCConfig *avcc, s32 ar_n, s32 ar_d);


#ifdef __cplusplus
};
#endif

typedef struct
{
  u8 rate_idx;
  u8 pck_size;
} QCPRateTable;


#endif		/*_GF_MEDIA_DEV_H_*/

