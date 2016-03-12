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


static int ZAG[64] =
{
  0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63,
};
//------------------------------------------------------------------------------
const int AAN_SCALE_BITS = 14;
const int IFAST_SCALE_BITS = 2; /* fractional bits in scale factors */
static int aan_scales[64] =
{
  16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
  22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
  21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
  19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
  16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
  12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
   8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
   4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
};



#define INPUT_BYTE(jpg_decoder) jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset++]
#define INPUT_2BYTE(jpg_decoder) (jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset]<<8)|(jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset+1])
#define INCREASE2() jpg_decoder->buffer_offset = jpg_decoder->buffer_offset+2

int djpeg_marker_parser(DJPG_DATA_PARAM *jpg_decoder)
{
	unsigned char current_byte;
	int retcode = 0;

	for(;;)
	{
		if(jpg_decoder->buffer_offset >= jpg_decoder->buffer_len)return 1;

		current_byte = INPUT_BYTE(jpg_decoder);
		if((jpg_decoder->stream_last_byte) != 0xff)
		{
			jpg_decoder->stream_last_byte = current_byte;
			continue;
		}

		switch(current_byte)
		{
			case M_SOI:
				jpg_decoder->saw_soi = 1;
				break;

			case M_SOF2:  //progressive dct
				jpg_decoder->progressive_flag = 1;
			case M_SOF0:  //baseline DCT 
			case M_SOF1:  // extended sequential DCT
				retcode = jpeg_read_sof_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;

			//Following is not supported 
			case M_SOF3:  //loseless sequential
			case M_SOF5:
			case M_SOF6:
			case M_SOF7:
			case M_SOF9:
			case M_SOF10:
			case M_SOF11:
			case M_SOF13:
			case M_SOF14:
			case M_SOF15:
				return 1;

			case M_APP0:
				retcode = jpeg_read_app0_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;
			case M_APP1:
			case M_APP2:
			case M_APP3:
			case M_APP4:
			case M_APP5:
			case M_APP6:
			case M_APP7:
			case M_APP8:
			case M_APP9:
			case M_APP10:
			case M_APP11:
			case M_APP12:
			case M_APP13:
			case M_APP14:
			case M_APP15:
				retcode = jpeg_read_default_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;

			case M_DQT:
				retcode = jpeg_read_dqt_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;

			case M_DHT:
				retcode = jpeg_read_dht_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;

			case M_DRI:
				retcode = jpeg_read_dri_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;

			case M_SOS:
				retcode = jpeg_read_sos_marker(jpg_decoder);
				if(retcode ==1)return 1;
				retcode = djpg_decoder_scan(jpg_decoder);
				return retcode;

			case M_JPG:
			case M_RST0:    /* no parameters */
			case M_RST1:
			case M_RST2:
			case M_RST3:
			case M_RST4:
			case M_RST5:
			case M_RST6:
			case M_RST7:
			case M_TEM:
				break;

			default:
				retcode = jpeg_read_default_marker(jpg_decoder);
				if(retcode ==1)return 1;
				break;
		}
		jpg_decoder->stream_last_byte = current_byte;
	}

	return 0;
}

int jpeg_read_sof_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int i;
	int length;
	int hv_samp;

	if(jpg_decoder->saw_soi ==0)return 0;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;

	length = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if((jpg_decoder->buffer_offset+length-2)>jpg_decoder->buffer_len)return 1;

	if(INPUT_BYTE(jpg_decoder) != 8)return 1; //we only support 8 bits

	jpg_decoder->image_y_size = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if((jpg_decoder->image_y_size < 1) || (jpg_decoder->image_y_size > JPGD_MAX_HEIGHT))
	return 1;

	jpg_decoder->image_x_size = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if((jpg_decoder->image_x_size < 1) || (jpg_decoder->image_x_size > JPGD_MAX_WIDTH))
    return 1;

	jpg_decoder->comps_in_frame = INPUT_BYTE(jpg_decoder);

	if(jpg_decoder->comps_in_frame > JPGD_MAXCOMPONENTS)
    return 1;

	if(length != (jpg_decoder->comps_in_frame * 3 + 8))
    return 1;

	for (i = 0; i < jpg_decoder->comps_in_frame; i++)
	{
		jpg_decoder->djpg_comp[i].comp_ident = INPUT_BYTE(jpg_decoder);
		hv_samp = INPUT_BYTE(jpg_decoder);
		jpg_decoder->djpg_comp[i].comp_h_samp = ((hv_samp&0xf0)>>4);
		jpg_decoder->djpg_comp[i].comp_v_samp = (hv_samp&0x0f);
		jpg_decoder->djpg_comp[i].comp_quant  = INPUT_BYTE(jpg_decoder);;
	}
	
/*	jpg_decoder->djpg_comp[0].comp_h_samp = 2;
	jpg_decoder->djpg_comp[0].comp_v_samp = 2;
	jpg_decoder->djpg_comp[1].comp_h_samp = 1;
	jpg_decoder->djpg_comp[1].comp_v_samp = 1;
	jpg_decoder->djpg_comp[2].comp_h_samp = 1;
	jpg_decoder->djpg_comp[2].comp_v_samp = 1;
*/

	return 0;
}


int jpeg_read_app0_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int length;
	unsigned char b[APP0_DATA_LEN];
	int i;

	if(jpg_decoder->saw_soi ==0)return 0;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	length = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();
	length -= 2;
	if((jpg_decoder->buffer_offset+length)>jpg_decoder->buffer_len)return 1;

	if (length != APP0_DATA_LEN)return 1;

	for (i = 0; i < length; i++)
	{
		b[i] = INPUT_BYTE(jpg_decoder);
	}

	if((b[0] == 0x4A) && (b[1] == 0x46) && (b[2] == 0x49) && 
      (b[3] == 0x46) && (b[4] == 0))
	{
		jpg_decoder->saw_JFIF_marker = 1;
		jpg_decoder->djpg_jfif.JFIF_major_version = (int)b[5];
		jpg_decoder->djpg_jfif.JFIF_minor_version = (int)b[6];
		jpg_decoder->djpg_jfif.density_unit = (int)b[7];
		jpg_decoder->djpg_jfif.X_density = (int)((b[8]<< 8)|(b[9]));
		jpg_decoder->djpg_jfif.Y_density = (int)((b[10]<< 8)|(b[11]));
	}

	return 0;
}

int jpeg_read_default_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int length;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	length = (int)INPUT_2BYTE(jpg_decoder);
	if((jpg_decoder->buffer_offset+length)>jpg_decoder->buffer_len)return 1;

	jpg_decoder->buffer_offset += length;

	return 0;
}

int jpeg_read_dqt_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int n, i, prec;
	int length;
	int temp;

	if(jpg_decoder->saw_soi ==0)return 0;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	length = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if(length < 2)return 1;
	length -= 2;
	if((jpg_decoder->buffer_offset+length)>jpg_decoder->buffer_len)return 1;

	while (length)
	{
		n = (int)INPUT_BYTE(jpg_decoder);
		prec = (n >> 4);
		n = (n & 0x0F);

		if (n >= JPGD_MAXQUANTTABLES)
		return 1;

		// read quantization entries, in zag order
		for (i = 0; i < 64; i++)
		{
			if(prec)
			{
				temp = (int)INPUT_2BYTE(jpg_decoder);
				INCREASE2();
			}
			else
			{
				temp = (int)INPUT_BYTE(jpg_decoder);
			}

			#ifdef SUPPORT_MMX
			if (jpg_decoder->use_mmx_idct)
			jpg_decoder->quant_tbl_para[n].quant[ZAG[i]] = (signed short)((temp * aan_scales[ZAG[i]] + (1 << (AAN_SCALE_BITS - IFAST_SCALE_BITS - 1))) >> (AAN_SCALE_BITS - IFAST_SCALE_BITS));
			else
			#endif
			jpg_decoder->quant_tbl_para[n].quant[i] = (signed short)temp;
		}

		i = 64 + 1;

		if (prec)
		i += 64;

		if (length < i)
		return 1;

		length -= i;
	}

	return 0;
}

int jpeg_read_dht_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int i, index, count;
	int length;

	if(jpg_decoder->saw_soi ==0)return 0;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	length = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if (length < 2)return 1;

	length -= 2;
	if((jpg_decoder->buffer_offset+length)>jpg_decoder->buffer_len)return 1;
	while (length)
	{
		index = (int)INPUT_BYTE(jpg_decoder);
		index = (index & 0x0F) + ((index & 0x10) >> 4) * (JPGD_MAXHUFFTABLES >> 1);

		if (index >= JPGD_MAXHUFFTABLES)return 1;

		jpg_decoder->huff_tbl_para[index].huff_num[0] = 0;
		jpg_decoder->huff_index_flag[index] = 1;
		count = 0;

		for(i = 1; i <= 16; i++)
		{
			jpg_decoder->huff_tbl_para[index].huff_num[i] = (int)INPUT_BYTE(jpg_decoder);
			count += jpg_decoder->huff_tbl_para[index].huff_num[i];
		}

		if (count > 255)return 1;
 
		for (i = 0; i < count; i++)
		jpg_decoder->huff_tbl_para[index].huff_val[i] = (int)INPUT_BYTE(jpg_decoder);

		i = 1 + 16 + count;

		if (length < i)return 1;

		length -= i;
	}


	return 0;

}

int jpeg_read_dri_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int length;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	length = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if (length != 4)
	return 1;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	jpg_decoder->restart_interval = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	return 0;
}

int jpeg_read_sos_marker(DJPG_DATA_PARAM *jpg_decoder)
{
	int length;
	int i, ci, n, c, cc;
	int success_value;

	if((jpg_decoder->buffer_offset+2)>jpg_decoder->buffer_len)return 1;
	length = (int)INPUT_2BYTE(jpg_decoder);
	INCREASE2();

	if((jpg_decoder->buffer_offset+1)>jpg_decoder->buffer_len)return 1;
	n = (int)INPUT_BYTE(jpg_decoder);

	jpg_decoder->comps_in_scan = n;

	length -= 3;
	if((jpg_decoder->buffer_offset+length)>jpg_decoder->buffer_len)return 1;

	if ( (length != (n * 2 + 3)) || (n < 1) || (n > JPGD_MAXCOMPSINSCAN) )
	return 1;

	for (i = 0; i < n; i++)
	{
		cc = (int)INPUT_BYTE(jpg_decoder);
		c = (int)INPUT_BYTE(jpg_decoder);
		length -= 2;

		for (ci = 0; ci < jpg_decoder->comps_in_frame; ci++)
		{
			if (cc == jpg_decoder->djpg_comp[ci].comp_ident)break;
		}

		if (ci >= jpg_decoder->comps_in_frame)
		return 1;

		jpg_decoder->comp_list[i]    = ci;
		jpg_decoder->djpg_comp[ci].comp_dc_tab = (c >> 4) & 15;
		jpg_decoder->djpg_comp[ci].comp_ac_tab = (c & 15) + (JPGD_MAXHUFFTABLES >> 1);
	}

	jpg_decoder->spectral_start  = (int)INPUT_BYTE(jpg_decoder);
	jpg_decoder->spectral_end    = (int)INPUT_BYTE(jpg_decoder);
	success_value = (int)INPUT_BYTE(jpg_decoder);
	jpg_decoder->successive_high = ((success_value>>4)&15);
	jpg_decoder->successive_low  = (success_value&15);

	if (!jpg_decoder->progressive_flag)
	{
		jpg_decoder->spectral_start = 0;
		jpg_decoder->spectral_end = 63;
	}

	length -= 3;

	jpg_decoder->buffer_offset += length;

	return 0;
}
