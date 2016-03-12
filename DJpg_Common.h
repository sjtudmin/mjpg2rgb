#ifndef AMD64
#define SUPPORT_MMX
#define OPTIMIZE_INTRIN
#endif
//#define CREATE_BMP_FILE
//#define PRINT_BITMAP_FILE

#define JPGD_MAXHUFFTABLES   8
#define JPGD_MAXCOMPONENTS   4
#define JPGD_MAXCOMPSINSCAN  4
#define JPGD_MAXBLOCKSPERMCU 10
#define JPGD_MAXBLOCKS    100
#define JPGD_MAXBLOCKSPERROW 1024
#define JPGD_MAXQUANTTABLES  4
#define APP0_DATA_LEN	14
#define JPGD_GRAYSCALE 0
#define JPGD_YH1V1     1
#define JPGD_YH2V1     2
#define JPGD_YH1V2     3
#define JPGD_YH2V2     4

#define JPGD_MAX_HEIGHT 8192
#define JPGD_MAX_WIDTH  8192
#define MAX_HUFF_NUM 17
#define MAX_HUFF_VAL 256
#define MAX_QUANT_NUM 64

//#define RGB_32
#define BYTES_ONE_PIXEL 3
#define BITS_ONE_PIXEL  24

#include <mmintrin.h>
#include "DJpg.h"

typedef struct HUFF_TABLES_TAG
{
	unsigned int look_up[256];
	unsigned char code_size[256];
	unsigned int tree[512];
}HUFF_TABLES_TAG, *PHUFF_TABLES_TAG;

typedef struct HUFF_TABLE_PARAM
{
	int huff_num[MAX_HUFF_NUM];
	int huff_val[MAX_HUFF_VAL];
}HUFF_TABLE_PARAM;

typedef struct QUANT_TABLE_PARAM
{
	signed short quant[MAX_QUANT_NUM];
}QUANT_TABLE_PARAM;

typedef struct DJPG_COMPONENT
{
	int comp_ident;      /* component's ID */
	int comp_h_samp;     /* component's horizontal sampling factor */
	int comp_v_samp;     /* component's vertical sampling factor */
	int comp_quant;      /* component's quantization table selector */

	int comp_h_blocks;
	int comp_v_blocks;

	int comp_dc_tab;     /* component's DC Huffman coding table selector */
	int comp_ac_tab;     /* component's AC Huffman coding table selector */

}DJPG_COMPONENT;

typedef struct DJPG_JFIF
{
	int saw_JFIF_marker;
	int	JFIF_major_version;
	int	JFIF_minor_version;
	int	density_unit;
	int	X_density;
	int	Y_density;
}DJPG_JFIF;

typedef struct DJPG_DATA_PARAM
{

	unsigned char *djpg_buffer;
	unsigned char *djpg_out_buffer;
	int buffer_len;
	int buffer_offset;
	unsigned char stream_last_byte;

	unsigned int bit_buf;
	int bits;
	
	#ifdef SUPPORT_MMX
	int use_mmx_idct;
	#endif

	int error_code;
	int ready_flag;

	int image_x_size;
	int image_y_size;
	int progressive_flag;

	int saw_soi;

	int saw_JFIF_marker;
	DJPG_JFIF djpg_jfif;

	int comps_in_frame;                 /* # of components in frame */
	DJPG_COMPONENT djpg_comp[JPGD_MAXCOMPONENTS];

	QUANT_TABLE_PARAM quant_tbl_para[JPGD_MAXQUANTTABLES];

	HUFF_TABLE_PARAM huff_tbl_para[JPGD_MAXHUFFTABLES];
	int huff_index_flag[JPGD_MAXHUFFTABLES];

	int comps_in_scan;                  /* # of components in scan */
	int comp_list[JPGD_MAXCOMPSINSCAN];      /* components in this scan */

	int spectral_start;                 /* spectral selection start */
	int spectral_end;                   /* spectral selection end   */
	int successive_low;                 /* successive approximation low */
	int successive_high;                /* successive approximation high */

	int scan_type;
	int max_mcu_x_size;                 /* MCU's max. X size in pixels */
	int max_mcu_y_size;                 /* MCU's max. Y size in pixels */

	int max_mcus_per_row;
	int max_blocks_per_mcu;
	int max_mcus_per_col;
	int blocks_per_mcu;
	int max_blocks_per_row;
	int mcus_per_row;
	int mcus_per_col;
	int dest_bytes_per_pixel;
	int dest_bytes_per_scan_line;
	int real_dest_bytes_per_scan_line;

	signed short block_seg[256][64];
	int block_max_zag_set[256];
	int total_lines_left;
	int mcu_lines_left;
	int crr[256];
	int cbb[256];
	int crg[256];
	int cbg[256];
#ifdef OPTIMIZE_INTRIN
	__m64 crrval ;
	__m64 cbbval ;
	__m64 crgval ;
	__m64 cbgval ;
	__m64 zero   ;
	__m64 m512  ;
#endif
	int mcu_org[JPGD_MAXBLOCKSPERMCU];
	HUFF_TABLES_TAG h[JPGD_MAXHUFFTABLES];
	PHUFF_TABLES_TAG dc_huff_seg[JPGD_MAXBLOCKSPERMCU];
	PHUFF_TABLES_TAG ac_huff_seg[JPGD_MAXBLOCKSPERMCU];

	unsigned int *component[JPGD_MAXBLOCKSPERMCU];   /* points into the lastdcvals table */
	unsigned int last_dc_val[JPGD_MAXCOMPONENTS];

	unsigned char mcu_block_buf[JPGD_MAXBLOCKSPERMCU][64];

	int eob_run;

	int restart_interval;
	int restarts_left;
	int next_restart_num;


}DJPG_DATA_PARAM;

typedef enum
{
  M_SOF0  = 0xC0,
  M_SOF1  = 0xC1,
  M_SOF2  = 0xC2,
  M_SOF3  = 0xC3,

  M_SOF5  = 0xC5,
  M_SOF6  = 0xC6,
  M_SOF7  = 0xC7,

  M_JPG   = 0xC8,
  M_SOF9  = 0xC9,
  M_SOF10 = 0xCA,
  M_SOF11 = 0xCB,

  M_SOF13 = 0xCD,
  M_SOF14 = 0xCE,
  M_SOF15 = 0xCF,

  M_DHT   = 0xC4,

  M_DAC   = 0xCC,

  M_RST0  = 0xD0,
  M_RST1  = 0xD1,
  M_RST2  = 0xD2,
  M_RST3  = 0xD3,
  M_RST4  = 0xD4,
  M_RST5  = 0xD5,
  M_RST6  = 0xD6,
  M_RST7  = 0xD7,

  M_SOI   = 0xD8,
  M_EOI   = 0xD9,
  M_SOS   = 0xDA,
  M_DQT   = 0xDB,
  M_DNL   = 0xDC,
  M_DRI   = 0xDD,
  M_DHP   = 0xDE,
  M_EXP   = 0xDF,

  M_APP0  = 0xE0,
  M_APP1  = 0xE1,
  M_APP2  = 0xE2,
  M_APP3  = 0xE3,
  M_APP4  = 0xE4,
  M_APP5  = 0xE5,
  M_APP6  = 0xE6,
  M_APP7  = 0xE7,
  M_APP8  = 0xE8,
  M_APP9  = 0xE9,
  M_APP10 = 0xEA,
  M_APP11 = 0xEB,
  M_APP12 = 0xEC,
  M_APP13 = 0xED,
  M_APP14 = 0xEE,
  M_APP15 = 0xEF,

  M_JPG0  = 0xF0,
  M_JPG13 = 0xFD,
  M_COM   = 0xFE,

  M_TEM   = 0x01,

  M_ERROR = 0x100
} JPEG_MARKER;


//int djpg_decoder(unsigned char *JPG_input_frame_buffer, unsigned char *JPG_output_buffer,int input_frame_len);
void djpg_init(DJPG_DATA_PARAM *jpg_decoder,unsigned char *JPG_input_frame_buffer, unsigned char *JPG_output_buffer,int input_frame_len);
int jpeg_read_sof_marker(DJPG_DATA_PARAM *jpg_decoder);
int jpeg_read_app0_marker(DJPG_DATA_PARAM *jpg_decoder);
int jpeg_read_default_marker(DJPG_DATA_PARAM *jpg_decoder);
int jpeg_read_dqt_marker(DJPG_DATA_PARAM *jpg_decoder);
int jpeg_read_dht_marker(DJPG_DATA_PARAM *jpg_decoder);
int jpeg_read_dri_marker(DJPG_DATA_PARAM *jpg_decoder);
int jpeg_read_sos_marker(DJPG_DATA_PARAM *jpg_decoder);
int djpg_init_frame(DJPG_DATA_PARAM *jpg_decoder);
int djpg_create_look_ups(DJPG_DATA_PARAM *jpg_decoder);
int djpg_init_progressive(DJPG_DATA_PARAM *jpg_decoder);
int djpg_init_sequential(DJPG_DATA_PARAM *jpg_decoder);
int djpg_calc_mcu_block_order(DJPG_DATA_PARAM *jpg_decoder);
int djpg_make_huff_table(DJPG_DATA_PARAM *jpg_decoder,int index,HUFF_TABLES_TAG *hs);
int djpg_check_huff_tables(DJPG_DATA_PARAM *jpg_decoder);
int djpg_init_scan(DJPG_DATA_PARAM *jpg_decoder);
int djpg_huff_decode(DJPG_DATA_PARAM *jpg_decoder,HUFF_TABLES_TAG *Ph);
int djpg_init_bitstream(DJPG_DATA_PARAM *jpg_decoder);
int djpg_process_restart(DJPG_DATA_PARAM *jpg_decoder);
void djpg_idct(signed short *data, unsigned char *Pdst_ptr);
#ifdef SUPPORT_MMX
void jpeg_idct_ifast (signed short *inptr,signed short *quantptr,unsigned char **outptr, int output_col);
void jpeg_idct_ifast_deinit(void);
#endif
int djpg_decoder_picture(DJPG_DATA_PARAM *jpg_decoder);

int djpg_H2V2Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[],unsigned char *dest_buf,int dst_width,int mcu_row);
int djpg_H2V1Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[],unsigned char *dest_buf,int dst_width,int mcu_row);
int djpg_H1V2Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[],unsigned char *dest_buf,int dst_width,int mcu_row);
int djpg_H1V1Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[],unsigned char *dest_buf,int dst_width,int mcu_row);
int djpg_GrayConvert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[],unsigned char *dest_buf,int dst_width,int mcu_row);
int djpg_format_convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dst_buf,int mcu_row);
int djpg_decoder_scan(DJPG_DATA_PARAM *jpg_decoder);
int djpeg_marker_parser(DJPG_DATA_PARAM *jpg_decoder);
#ifdef PRINT_BITMAP_FILE
int djpg_print_rgb(DJPG_DATA_PARAM *jpg_decoder);
#endif

#ifdef CREATE_BMP_FILE
FILE *djpg_create_bmp(DJPG_DATA_PARAM *jpg_decoder,char *p_file_name);
#endif
