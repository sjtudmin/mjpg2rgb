
//#define USER_MODE
//#define CACULATE_TIME

extern "C" {
#include <wdm.h>
}

#ifdef USER_MODE
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <memory.h>
#ifdef CACULATE_TIME
#include <windows.h>
#endif
#else
#endif
//#include "hardware.h"

#include "djpg_common.h"

DJPG_DATA_PARAM mjpg_decoder;
const int JPGD_FAILED = -1;
const int JPGD_DONE = 1;
const int JPGD_OKAY = 0;


#define UBITS(num) (unsigned int)((jpg_decoder->bit_buf)>>(32-num))
#define jpgmin(a,b) (a>b)?b:a

#define DUMPBITS(num)						\
do {										\
    jpg_decoder->bit_buf <<= (num);			\
    jpg_decoder->bits += (num);				\
} while (0)

//jpg_decoder->bit_buf |= ((jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset] << 8) | jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset+1]) << (jpg_decoder->bits);
//	if(jpg_decoder->buffer_offset >= jpg_decoder->buffer_len)return 1;	

#define GETWORD()				\
do {								\
	jpg_decoder->bit_buf |= (jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset])<< (jpg_decoder->bits+8);	\
	if((jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset] == 0xff)&&(jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset+1] == 0x00))	\
	{	\
		jpg_decoder->buffer_offset += 2;						\
	}	\
	else	\
	{		\
		jpg_decoder->buffer_offset += 1;						\
	}		\
} while (0)

#define NEEDBITS()					\
do {								\
    if(jpg_decoder->bits > 0)		\
	{								\
		GETWORD ();					\
		jpg_decoder->bits -= 8;		\
		GETWORD ();					\
		jpg_decoder->bits -= 8;		\
    }								\
} while (0)


const int extend_test[16] =   /* entry n is 2**(n-1) */
  { 0, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000 };

const int extend_offset[16] = /* entry n is (-1 << n) + 1 */
  { 0, ((-1)<<1) + 1, ((-1)<<2) + 1, ((-1)<<3) + 1, ((-1)<<4) + 1,
    ((-1)<<5) + 1, ((-1)<<6) + 1, ((-1)<<7) + 1, ((-1)<<8) + 1,
    ((-1)<<9) + 1, ((-1)<<10) + 1, ((-1)<<11) + 1, ((-1)<<12) + 1,
    ((-1)<<13) + 1, ((-1)<<14) + 1, ((-1)<<15) + 1 };

// used by huff_extend()
const int extend_mask[] =
{
  0,
  (1<<0), (1<<1), (1<<2), (1<<3),
  (1<<4), (1<<5), (1<<6), (1<<7),
  (1<<8), (1<<9), (1<<10), (1<<11),
  (1<<12), (1<<13), (1<<14), (1<<15),
  (1<<16),
};

#define HUFF_EXTEND_TBL(x,s) ((x) < extend_test[s] ? (x) + extend_offset[s] : (x))
#define HUFF_EXTEND(x,s) HUFF_EXTEND_TBL(x,s)

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


//The routine provide a simple interface to be called by any other application
int djpg_decoder(unsigned char *JPG_input_frame_buffer, unsigned char *JPG_output_buffer,int input_frame_len)
{
	int retcode =0;

	djpg_init(&mjpg_decoder,JPG_input_frame_buffer,JPG_output_buffer,input_frame_len);

	retcode = djpeg_marker_parser(&mjpg_decoder);

	return retcode;
}

void djpg_init(DJPG_DATA_PARAM *jpg_decoder,unsigned char *JPG_input_frame_buffer, unsigned char *JPG_output_buffer,int input_frame_len)
{
	#ifdef USER_MODE
	memset(jpg_decoder,0x00,sizeof(DJPG_DATA_PARAM));
	#else
    RtlZeroMemory(jpg_decoder, sizeof(DJPG_DATA_PARAM));
	#endif

	jpg_decoder->djpg_buffer = JPG_input_frame_buffer;
	jpg_decoder->djpg_out_buffer = JPG_output_buffer;
	jpg_decoder->buffer_len = input_frame_len;

	#ifdef SUPPORT_MMX
	jpg_decoder->use_mmx_idct = 1;
	#endif

	return;
}

int djpg_decoder_scan(DJPG_DATA_PARAM *jpg_decoder)
{
	int retcode;

	retcode = djpg_init_frame(jpg_decoder);

	if(retcode ==1)return 1;

	if (jpg_decoder->progressive_flag)
	retcode = djpg_init_progressive(jpg_decoder);
	else
    retcode = djpg_init_sequential(jpg_decoder);

	if(retcode ==1)return 1;

	retcode = djpg_init_bitstream(jpg_decoder);
	if(retcode ==1)return 1;
	retcode = djpg_decoder_picture(jpg_decoder);

	return retcode;
}

#ifdef CREATE_BMP_FILE
FILE *p_bmp_handle = NULL;
#endif

int djpg_decoder_picture(DJPG_DATA_PARAM *jpg_decoder)
{
	int mcu_row;
	int mcu_col;
	int mcu_block;
	signed short p[64];
	int component_id;
	signed short *q;
	unsigned char *p_out_buf;
	int r, s;
	int prev_num_set;
	HUFF_TABLES_TAG *Ph;
	int n;
	int kt;
	unsigned char *block_buf[JPGD_MAXBLOCKSPERMCU];
	int i;
	unsigned char *dst_mcu_row;
	unsigned char *dst_buf;
	int dst_row_offset;
	int dst_col_offset;
	#ifdef SUPPORT_MMX
	unsigned char * outptr[8];
	#endif

	#ifdef CACULATE_TIME
	unsigned int t_start,t_end;
	#endif

	#ifdef CREATE_BMP_FILE
	p_bmp_handle = NULL;
	p_bmp_handle = djpg_create_bmp(jpg_decoder,"E:\\TestJpg\\myjpg\\idct\\rgb_24_1.bmp");
	#endif

	#ifdef CACULATE_TIME
	#ifdef GETTICKCOUNT
	t_start = GetTickCount();
	#else
	LARGE_INTEGER resolution,start,end;
	float duration;
	QueryPerformanceFrequency(&resolution);
	QueryPerformanceCounter(&start);
	#endif
	#endif

	for(i = 0; i < JPGD_MAXBLOCKSPERMCU; ++i)
	{
		block_buf[i] = jpg_decoder->mcu_block_buf[i];
	}

	dst_mcu_row = jpg_decoder->djpg_out_buffer;
	dst_row_offset = jpg_decoder->image_x_size*BYTES_ONE_PIXEL*jpg_decoder->max_mcu_y_size;
	dst_col_offset = jpg_decoder->max_mcu_x_size*BYTES_ONE_PIXEL;
	for(mcu_col = 0; mcu_col < jpg_decoder->mcus_per_col; mcu_col++)
	{
		//dst_buf = jpg_decoder->djpg_out_buffer + jpg_decoder->image_x_size*3*jpg_decoder->max_mcu_y_size*mcu_col;

		dst_buf = dst_mcu_row;
		//row_block = 0;
		for(mcu_row = 0; mcu_row < jpg_decoder->mcus_per_row; mcu_row++)
		{
			if ((jpg_decoder->restart_interval) && (jpg_decoder->restarts_left == 0))
			djpg_process_restart(jpg_decoder);

			for(mcu_block = 0; mcu_block < jpg_decoder->blocks_per_mcu; mcu_block++)
			{
				component_id = jpg_decoder->mcu_org[mcu_block];
				p_out_buf = block_buf[mcu_block];

				q = jpg_decoder->quant_tbl_para[jpg_decoder->djpg_comp[component_id].comp_quant].quant;

				if ((s = djpg_huff_decode(jpg_decoder,&jpg_decoder->h[jpg_decoder->djpg_comp[component_id].comp_dc_tab])) != 0)
				{
					NEEDBITS();
					r = (int)UBITS(s);
					DUMPBITS(s);
					s = HUFF_EXTEND(r, s);
				}

				jpg_decoder->last_dc_val[component_id] = (s += jpg_decoder->last_dc_val[component_id]);

				#ifdef SUPPORT_MMX
				if (jpg_decoder->use_mmx_idct)
				p[0] = (short)s;
				else
				#endif
				p[0] = s * q[0];

				prev_num_set = 64; //jpg_decoder->block_max_zag_set[row_block];

				Ph = &jpg_decoder->h[jpg_decoder->djpg_comp[component_id].comp_ac_tab];

				for (int k = 1; k < 64; k++)
				{
					s = djpg_huff_decode(jpg_decoder,Ph);

					r = s >> 4;
					s &= 15;

					if (s)
					{
						if (r)
						{
							//if ((k + r) > 63)return 1;
							if ((k + r) > 63)break;

							if (k < prev_num_set)
							{
								n = jpgmin(r, prev_num_set - k);
								kt = k;
								while (n--)
								p[ZAG[kt++]] = 0;
							}
							k += r;
						}

						NEEDBITS();
						r = (int)UBITS(s);
						DUMPBITS(s);
						s = HUFF_EXTEND(r, s);

						#ifdef SUPPORT_MMX
						if (jpg_decoder->use_mmx_idct)
						p[ZAG[k]] = (short)s;
						else
						#endif
						p[ZAG[k]] = s * q[k];
					}
					else
					{
						if (r == 15)
						{
							//if ((k + 15) > 63)return 1;
							if ((k + 15) > 63)break;

							if (k < prev_num_set)
							{
								n = jpgmin(16, prev_num_set - k);		//bugfix Dec. 19, 2001 - was 15!
								kt = k;
								while (n--)
								p[ZAG[kt++]] = 0;
							}

							k += 15;
						}
						else
						{
							break;
						}
					}
				}

				if (k < prev_num_set)
				{
					kt = k;
					while (kt < prev_num_set)
					p[ZAG[kt++]] = 0;
				}

				
				#ifdef SUPPORT_MMX
				outptr[0] = p_out_buf-8;
				outptr[1] = p_out_buf;
				outptr[2] = p_out_buf+8*1;
				outptr[3] = p_out_buf+8*2;
				outptr[4] = p_out_buf+8*3;
				outptr[5] = p_out_buf+8*4;
				outptr[6] = p_out_buf+8*5;
				outptr[7] = p_out_buf+8*6;

				if(jpg_decoder->use_mmx_idct)
				jpeg_idct_ifast(p, q, outptr, 8);
				else
				#endif
				djpg_idct(p,p_out_buf);

			}

			djpg_format_convert(jpg_decoder,block_buf,dst_buf,mcu_row);
			dst_buf = dst_buf + dst_col_offset; 
			jpg_decoder->restarts_left--;
		}
		dst_mcu_row = dst_mcu_row + dst_row_offset;
	}

	#ifdef CREATE_BMP_FILE
	if(p_bmp_handle)fclose(p_bmp_handle);
	#endif

	#ifdef CACULATE_TIME
#ifdef OPTIMIZE_INTRIN
	_m_empty();
#endif
	//t_end = GetTickCount();
	//printf("Consume time = %d ms\n",(t_end-t_start));
	#ifdef GETTICKCOUNT
	t_end = GetTickCount();
	printf("Consume time = %d ms\n",(t_end-t_start));
	#else
    QueryPerformanceCounter(&end);
    duration = (float)(end.QuadPart - start.QuadPart) / (float)resolution.QuadPart;
	printf("Consume time = %5f ms\n",duration/*(t_end-t_start)*/);
	#endif
	#endif

	#ifdef PRINT_BITMAP_FILE
	djpg_print_rgb(jpg_decoder);
	#endif
	return 0;
}

int djpg_format_convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dst_buf, int mcu_row)
{
	int dst_width = 0;

	switch (jpg_decoder->scan_type)
	{
		case JPGD_YH2V2:
			dst_width = jpg_decoder->image_x_size *BYTES_ONE_PIXEL;
			djpg_H2V2Convert(jpg_decoder,yuv_buf,dst_buf,dst_width,mcu_row);
			break;

		case JPGD_YH2V1:
			dst_width = jpg_decoder->image_x_size *BYTES_ONE_PIXEL;
			djpg_H2V1Convert(jpg_decoder,yuv_buf,dst_buf,dst_width,mcu_row);
			break;
    
		case JPGD_YH1V2:
			dst_width = jpg_decoder->image_x_size *BYTES_ONE_PIXEL;
			djpg_H1V2Convert(jpg_decoder,yuv_buf,dst_buf,dst_width,mcu_row);
			break;

		case JPGD_YH1V1:
			dst_width = jpg_decoder->image_x_size *BYTES_ONE_PIXEL;
			djpg_H1V1Convert(jpg_decoder,yuv_buf,dst_buf,dst_width,mcu_row);
			break;

		case JPGD_GRAYSCALE:
			dst_width = jpg_decoder->image_x_size;
			djpg_GrayConvert(jpg_decoder,yuv_buf,dst_buf,dst_width,mcu_row);
			break;
	}

	return 0;
}


int djpg_huff_decode(DJPG_DATA_PARAM *jpg_decoder,HUFF_TABLES_TAG *Ph)
{
	int symbol;
	unsigned int first_8bits;
	int index;

	// Check first 8-bits: do we have a complete symbol?
	NEEDBITS();
	first_8bits = UBITS(8);
	
	if ((symbol = Ph->look_up[first_8bits]) < 0)
	{
		// Decode more bits, use a tree traversal to find symbol.
		DUMPBITS(8);

		do
		{
			NEEDBITS();
			index = ~symbol + (1 - UBITS(1));
			symbol = Ph->tree[index];
			//symbol = Ph->tree[~symbol + (1 - UBITS(1))];
			DUMPBITS(1);

		} while (symbol < 0);
	}
	else
	{
		DUMPBITS(Ph->code_size[symbol]);
	}

	return symbol;
}


int djpg_process_restart(DJPG_DATA_PARAM *jpg_decoder)
{
	int i, c;


	// Align to a byte boundry
	jpg_decoder->buffer_offset -= (16-jpg_decoder->bits)/8;

	// Let's scan a little bit to find the marker, but not _too_ far.
	// 1536 is a "fudge factor" that determines how much to scan.
	for (i = 1536; i > 0; i--)
	{
		if (jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset++] == 0xFF)
		break;
	}

	if (i == 0)return 1;

	for ( ; i > 0; i--)
	{
		if ((c= (int)jpg_decoder->djpg_buffer[jpg_decoder->buffer_offset++]) != 0xFF)
		break;
	}

	if (i == 0)return 1;
    
	// Is it the expected marker? If not, something bad happened.
	if (c != (jpg_decoder->next_restart_num + M_RST0))
    return 1;

	// Reset each component's DC prediction values.

	#ifdef USER_MODE
	memset(jpg_decoder->last_dc_val, 0, jpg_decoder->comps_in_frame * sizeof(unsigned int));
	#else
    RtlZeroMemory(jpg_decoder->last_dc_val,jpg_decoder->comps_in_frame * sizeof(unsigned int));
	#endif

	jpg_decoder->eob_run = 0;

	jpg_decoder->restarts_left = jpg_decoder->restart_interval;

	jpg_decoder->next_restart_num = (jpg_decoder->next_restart_num + 1) & 7;

	// Get the bit buffer going again...
	djpg_init_bitstream(jpg_decoder);

	return 0;
}

NTSTATUS
DllUnload(
          void
)
{
    return (STATUS_SUCCESS);
}


extern "C"
ULONG
DriverEntry(
    PVOID Context1,
    PVOID Context2
    )
{
    // this function is not used
    return STATUS_SUCCESS;
}