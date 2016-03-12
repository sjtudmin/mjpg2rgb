//#define USER_MODE

#ifdef USER_MODE
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <memory.h>
#else
#include <wdm.h>
#endif

#include "djpg_common.h"


int djpg_init_frame(DJPG_DATA_PARAM *jpg_decoder)
{
	int i;

	if(jpg_decoder->comps_in_frame == 1)
	{
		jpg_decoder->scan_type = JPGD_GRAYSCALE;

		jpg_decoder->max_blocks_per_mcu = 1;
		jpg_decoder->max_mcu_x_size     = 8;
		jpg_decoder->max_mcu_y_size     = 8;
	}
	else if(jpg_decoder->comps_in_frame == 3)
	{
		if ( ((jpg_decoder->djpg_comp[1].comp_h_samp != 1) || (jpg_decoder->djpg_comp[1].comp_v_samp != 1)) ||
         ((jpg_decoder->djpg_comp[2].comp_h_samp != 1) || (jpg_decoder->djpg_comp[2].comp_v_samp != 1)) )
		return 1;

		if ((jpg_decoder->djpg_comp[0].comp_h_samp == 1) && (jpg_decoder->djpg_comp[0].comp_v_samp == 1))
		{//for video, it is 4:4:4
			jpg_decoder->scan_type          = JPGD_YH1V1;
			jpg_decoder->max_blocks_per_mcu = 3;
			jpg_decoder->max_mcu_x_size     = 8;
			jpg_decoder->max_mcu_y_size     = 8;
		}
		else if ((jpg_decoder->djpg_comp[0].comp_h_samp == 2) && (jpg_decoder->djpg_comp[0].comp_v_samp == 1))
		{
			jpg_decoder->scan_type          = JPGD_YH2V1;
			jpg_decoder->max_blocks_per_mcu = 4;
			jpg_decoder->max_mcu_x_size     = 16;
			jpg_decoder->max_mcu_y_size     = 8;
		}
		else if ((jpg_decoder->djpg_comp[0].comp_h_samp == 1) && (jpg_decoder->djpg_comp[0].comp_v_samp == 2))
		{
			jpg_decoder->scan_type          = JPGD_YH1V2;
			jpg_decoder->max_blocks_per_mcu = 4;
			jpg_decoder->max_mcu_x_size     = 8;
			jpg_decoder->max_mcu_y_size     = 16;
		}
		else if ((jpg_decoder->djpg_comp[0].comp_h_samp == 2) && (jpg_decoder->djpg_comp[0].comp_v_samp == 2))
		{//4:2:0
			jpg_decoder->scan_type          = JPGD_YH2V2;
			jpg_decoder->max_blocks_per_mcu = 6;

			jpg_decoder->max_mcu_x_size     = 16;
			jpg_decoder->max_mcu_y_size     = 16;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}

	jpg_decoder->max_mcus_per_row = (jpg_decoder->image_x_size + (jpg_decoder->max_mcu_x_size - 1)) / jpg_decoder->max_mcu_x_size;
	jpg_decoder->max_mcus_per_col = (jpg_decoder->image_y_size + (jpg_decoder->max_mcu_y_size - 1)) / jpg_decoder-> max_mcu_y_size;

	/* these values are for the *destination* pixels: after conversion */

	if (jpg_decoder->scan_type == JPGD_GRAYSCALE)
	jpg_decoder->dest_bytes_per_pixel = 1;
	else
    jpg_decoder->dest_bytes_per_pixel = 3;

	jpg_decoder->dest_bytes_per_scan_line = ((jpg_decoder->image_x_size + 15) & 0xFFF0) * jpg_decoder->dest_bytes_per_pixel;

	jpg_decoder->real_dest_bytes_per_scan_line = (jpg_decoder->image_x_size * jpg_decoder->dest_bytes_per_pixel);

	// Initialize two scan line buffers.
	// FIXME: Only the V2 sampling factors need two buffers.
	jpg_decoder->max_blocks_per_row = jpg_decoder->max_mcus_per_row * jpg_decoder->max_blocks_per_mcu;

	// Should never happen
	if (jpg_decoder->max_blocks_per_row > JPGD_MAXBLOCKSPERROW)
    return 1;

	// Allocate the coefficient buffer, enough for one row's worth of MCU's
	//q = (uchar *)alloc(max_blocks_per_row * 64 * sizeof(BLOCK_TYPE) + 8);

	// Align to 8-byte boundry, for MMX code
	//q = (uchar *)(((uint)q + 7) & ~7);

	// The block_seg[] array's name dates back to the
	// 16-bit assembler implementation. "seg" stood for "segment".

	for (i = 0; i < jpg_decoder->max_blocks_per_row; i++)
    jpg_decoder->block_max_zag_set[i] = 64;

	//Psample_buf = (uchar *)(((uint)alloc(max_blocks_per_row * 64 + 8) + 7) & ~7);

	jpg_decoder->total_lines_left = jpg_decoder->image_y_size;

	jpg_decoder->mcu_lines_left = 0;

	djpg_create_look_ups(jpg_decoder);

	return 0;
}


#define SCALEBITS 16
#define ONE_HALF ((long) 1 << (SCALEBITS-1))
#define FIX(x) ((long) ((x) * (1L<<SCALEBITS) + 0.5))
#define CRR_VAL 0x59ba //45941
#define CBB_VAL 0x7168 //58065
#define CRG_VAL -11700 //-23401
#define CBG_VAL -5638 //-11277
//------------------------------------------------------------------------------
// Create a few tables that allow us to quickly convert YCbCr to RGB.
int djpg_create_look_ups(DJPG_DATA_PARAM *jpg_decoder)
{
	int i, k;

	for (i = 0; i <= 255; i++)
	{
		//k = (i * 2) - 255;
		//k = (i * 2) - 256; // Dec. 28 2001- change so table[128] == 0
		k = (i * 4) - 512; 

#if 0
		jpg_decoder->crr[i] = ( FIX(1.40200/2)  * k + ONE_HALF) >> SCALEBITS;
		jpg_decoder->cbb[i] = ( FIX(1.77200/2)  * k + ONE_HALF) >> SCALEBITS;

		jpg_decoder->crg[i] = (-FIX(0.71414/2)) * k;
		jpg_decoder->cbg[i] = (-FIX(0.34414/2)) * k + ONE_HALF;
#endif

#if 1
		//jpg_decoder->crr[i] = ( CRR_VAL  * k + ONE_HALF) >> SCALEBITS;
		//jpg_decoder->cbb[i] = ( CBB_VAL  * k + ONE_HALF) >> SCALEBITS;

		//jpg_decoder->crg[i] = CRG_VAL * k;
		//jpg_decoder->cbg[i] = CBG_VAL * k + ONE_HALF;
		jpg_decoder->crr[i] = ( CRR_VAL  * k ) >> SCALEBITS;
		jpg_decoder->cbb[i] = ( CBB_VAL  * k ) >> SCALEBITS;

		jpg_decoder->crg[i] = (CRG_VAL * k) >> SCALEBITS;
		jpg_decoder->cbg[i] = (CBG_VAL * k ) >> SCALEBITS;
#endif
	}
#ifdef OPTIMIZE_INTRIN
	jpg_decoder->crrval = _m_from_int(0x59ba59ba);
	jpg_decoder->crrval = _m_punpcklwd(jpg_decoder->crrval,jpg_decoder->crrval);
	jpg_decoder-> cbbval = _m_from_int(0x71687168);
    jpg_decoder->cbbval = _m_punpcklwd(jpg_decoder->cbbval,jpg_decoder->cbbval);
	jpg_decoder->crgval = _m_from_int(0xd24cd24c);
	jpg_decoder->crgval = _m_punpcklwd(jpg_decoder->crgval,jpg_decoder->crgval);
	jpg_decoder->cbgval = _m_from_int(0xe9fae9fa);
	jpg_decoder->cbgval = _m_punpcklwd(jpg_decoder->cbgval,jpg_decoder->cbgval);
	jpg_decoder->zero   = _m_from_int(0);
	jpg_decoder->m512   = _m_from_int(0x2000200);
	jpg_decoder->m512 = _m_punpcklwd(jpg_decoder->m512,jpg_decoder->m512);

	_m_empty();

#endif
	return 0;
}

int djpg_init_scan(DJPG_DATA_PARAM *jpg_decoder)
{
	djpg_calc_mcu_block_order(jpg_decoder);

	djpg_check_huff_tables(jpg_decoder);

	#ifdef USER_MODE
	memset(jpg_decoder->last_dc_val, 0, jpg_decoder->comps_in_frame * sizeof(unsigned int));
	#else
    RtlZeroMemory(jpg_decoder->last_dc_val,jpg_decoder->comps_in_frame * sizeof(unsigned int));
	#endif

	jpg_decoder->eob_run = 0;

	if (jpg_decoder->restart_interval)
	{
		jpg_decoder->restarts_left = jpg_decoder->restart_interval;
		jpg_decoder->next_restart_num = 0;
	}


	return 0;
}

#define HUFFMANFILE "E:\\TestJpg\\myjpg\\huff_table.dat"
int djpg_make_huff_table(DJPG_DATA_PARAM *jpg_decoder,int index,HUFF_TABLES_TAG *hs)
{
	int p, i, l, si;
	int huffsize[257];
	int huffcode[257];
	int code;
	int subtree;
	int code_size;
	int lastp;
	int nextfreeentry;
	int currententry;
#ifdef OUTPUT_HUFFMAN
	FILE *m_file_huff = NULL;
	char huff_file_name[128];
	sprintf(huff_file_name,"E:\\TestJpg\\myjpg\\huff_table%d.dat",index);
#endif
	p = 0;

	for (l = 1; l <= 16; l++)
	{
		for (i = 1; i <= jpg_decoder->huff_tbl_para[index].huff_num[l]; i++)
		huffsize[p++] = l;
	}

	huffsize[p] = 0;

	lastp = p;

	code = 0;
	si = huffsize[0];
	p = 0;

	while (huffsize[p])
	{
		while (huffsize[p] == si)
		{
			huffcode[p++] = code;
			code++;
		}

		code <<= 1;
		si++;
	}

	//we will replace this later
	#ifdef USER_MODE
	memset(hs->look_up, 0, sizeof(hs->look_up));
	memset(hs->tree, 0, sizeof(hs->tree));
	memset(hs->code_size, 0, sizeof(hs->code_size));
	#else
    RtlZeroMemory(hs->look_up,sizeof(hs->look_up));
	RtlZeroMemory(hs->tree, sizeof(hs->tree));
	RtlZeroMemory(hs->code_size, sizeof(hs->code_size));
	#endif

	nextfreeentry = -1;

	p = 0;

	while (p < lastp)
	{
		i = jpg_decoder->huff_tbl_para[index].huff_val[p];
		code = huffcode[p];
		code_size = huffsize[p];

		hs->code_size[i] = (unsigned char)code_size;

		if(code_size <= 8)
		{
			code <<= (8 - code_size);

			for (l = 1 << (8 - code_size); l > 0; l--)
			{
				hs->look_up[code] = i;
				code++;
			}
		}
		else
		{
			subtree = (code >> (code_size - 8)) & 0xFF;

			currententry = hs->look_up[subtree];

			if (currententry == 0)
			{
				hs->look_up[subtree] = currententry = nextfreeentry;

				nextfreeentry -= 2;
			}

			code <<= (16 - (code_size - 8));

			for (l = code_size; l > 9; l--)
			{
				if ((code & 0x8000) == 0)
				currententry--;

				if (hs->tree[-currententry - 1] == 0)
				{
					hs->tree[-currententry - 1] = nextfreeentry;

					currententry = nextfreeentry;

					nextfreeentry -= 2;
				}
				else
				currententry = hs->tree[-currententry - 1];

				code <<= 1;
			}

			if ((code & 0x8000) == 0)
			currententry--;

			hs->tree[-currententry - 1] = i;
		}

		p++;
	}
#ifdef OUTPUT_HUFFMAN
	m_file_huff = fopen(huff_file_name,"w");
	if(m_file_huff)
	{
		for(i = 0; i < 256; ++i)
		{
			fprintf(m_file_huff," #%d look_up  = %x code_size = %x \n",i,hs->look_up[i],hs->code_size[i]);
		}
		for(i = 0; i < 512; ++i)
		{
			fprintf(m_file_huff," #%d tree = %x \n",i,hs->tree[i]);
		}
		fclose(m_file_huff);
	}
#endif
	return 0;
}



int djpg_check_huff_tables(DJPG_DATA_PARAM *jpg_decoder)
{
	int i;

	for (i = 0; i < JPGD_MAXHUFFTABLES; i++)
	{

		if (jpg_decoder->huff_index_flag[i] == 1)
		{
			djpg_make_huff_table(jpg_decoder,i,&jpg_decoder->h[i]);
		}
    }

	for (i = 0; i < jpg_decoder->blocks_per_mcu; i++)
	{
		jpg_decoder->dc_huff_seg[i] = &jpg_decoder->h[jpg_decoder->djpg_comp[jpg_decoder->mcu_org[i]].comp_dc_tab];
		jpg_decoder->ac_huff_seg[i] = &jpg_decoder->h[jpg_decoder->djpg_comp[jpg_decoder->mcu_org[i]].comp_ac_tab];
		jpg_decoder->component[i]   = &jpg_decoder->last_dc_val[jpg_decoder->mcu_org[i]];
	}

	return 0;
}

int djpg_calc_mcu_block_order(DJPG_DATA_PARAM *jpg_decoder)
{
	int component_num;
	int component_id;
	int max_h_samp = 0;
	int max_v_samp = 0;

	for (component_id = 0; component_id < jpg_decoder->comps_in_frame; component_id++)
	{
		if (jpg_decoder->djpg_comp[component_id].comp_h_samp > max_h_samp)
		max_h_samp = jpg_decoder->djpg_comp[component_id].comp_h_samp;

		if (jpg_decoder->djpg_comp[component_id].comp_v_samp > max_v_samp)
		max_v_samp = jpg_decoder->djpg_comp[component_id].comp_v_samp;
	}

	for (component_id = 0; component_id < jpg_decoder->comps_in_frame; component_id++)
	{
		jpg_decoder->djpg_comp[component_id].comp_h_blocks = ((((jpg_decoder->image_x_size * jpg_decoder->djpg_comp[component_id].comp_h_samp) + (max_h_samp - 1)) / max_h_samp) + 7) / 8;
		jpg_decoder->djpg_comp[component_id].comp_v_blocks = ((((jpg_decoder->image_y_size * jpg_decoder->djpg_comp[component_id].comp_v_samp) + (max_v_samp - 1)) / max_v_samp) + 7) / 8;
	}

	if (jpg_decoder->comps_in_scan == 1)
	{
		jpg_decoder->mcus_per_row = jpg_decoder->djpg_comp[jpg_decoder->comp_list[0]].comp_h_blocks;
		jpg_decoder->mcus_per_col = jpg_decoder->djpg_comp[jpg_decoder->comp_list[0]].comp_v_blocks;
	}
	else
	{
		jpg_decoder->mcus_per_row = (((jpg_decoder->image_x_size + 7) / 8) + (max_h_samp - 1)) / max_h_samp;
		jpg_decoder->mcus_per_col = (((jpg_decoder->image_y_size + 7) / 8) + (max_v_samp - 1)) / max_v_samp;
	}

	if (jpg_decoder->comps_in_scan == 1)
	{
		jpg_decoder->mcu_org[0] = jpg_decoder->comp_list[0];

		jpg_decoder->blocks_per_mcu = 1;
	}
	else
	{
		jpg_decoder->blocks_per_mcu = 0;

		for (component_num = 0; component_num < jpg_decoder->comps_in_scan; component_num++)
		{
			int num_blocks;

			component_id = jpg_decoder->comp_list[component_num];

			num_blocks = jpg_decoder->djpg_comp[component_id].comp_h_samp * jpg_decoder->djpg_comp[component_id].comp_v_samp;

			while (num_blocks--)
			jpg_decoder->mcu_org[jpg_decoder->blocks_per_mcu++] = component_id;
		}
	}

	return 0;
}

int djpg_init_progressive(DJPG_DATA_PARAM *jpg_decoder)
{

	int i;

	if (jpg_decoder->comps_in_frame == 4)
    return 1;

	for ( ; ; )
	{
		int dc_only_scan, refinement_scan;
//		Pdecode_block_func decode_block_func;

		if (!djpg_init_scan(jpg_decoder))
		break;

		dc_only_scan    = (jpg_decoder->spectral_start == 0);
		refinement_scan = (jpg_decoder->successive_high != 0);

		if ((jpg_decoder->spectral_start > jpg_decoder->spectral_end) || (jpg_decoder->spectral_end > 63))
		return 1;

		if (dc_only_scan)
		{
			if (jpg_decoder->spectral_end)
			return 1;
		}
		else if (jpg_decoder->comps_in_scan != 1)  /* AC scans can only contain one component */
		return 1;

		if ((refinement_scan) && (jpg_decoder->successive_low != jpg_decoder->successive_high - 1))
		return 1;
#if 0
		if (dc_only_scan)
		{
			if (refinement_scan)
			decode_block_func = progressive_block_decoder::decode_block_dc_refine;
			else
			decode_block_func = progressive_block_decoder::decode_block_dc_first;
		}
		else
		{
			if (refinement_scan)
			decode_block_func = progressive_block_decoder::decode_block_ac_refine;
			else
			decode_block_func = progressive_block_decoder::decode_block_ac_first;
		}

		decode_scan(decode_block_func);

		//get_bits_2(bits_left & 7);

		bits_left = 16;
		//bit_buf = 0;
		get_bits_1(16);
		get_bits_1(16);
#endif
	}

	jpg_decoder->comps_in_scan = jpg_decoder->comps_in_frame;

	for (i = 0; i < jpg_decoder->comps_in_frame; i++)
    jpg_decoder->comp_list[i] = i;

	djpg_calc_mcu_block_order(jpg_decoder);


	return 0;
}

int djpg_init_sequential(DJPG_DATA_PARAM *jpg_decoder)
{
	if(djpg_init_scan(jpg_decoder)!=0)
    return 1;


	return 0;
}

int djpg_init_bitstream(DJPG_DATA_PARAM *jpg_decoder)
{
	unsigned char *start = &jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset];
    
	jpg_decoder->bit_buf =
		(start[0] << 24) | (start[1] << 16) | (start[2] << 8) | start[3];

	jpg_decoder->buffer_offset += 4;
	jpg_decoder->bits = -16;

	return 0;
}