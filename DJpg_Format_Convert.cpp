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

#ifdef CREATE_BMP_FILE
unsigned char mcu_row_buf[81920];
extern FILE *p_bmp_handle;
#endif

#define djpg_decoder_clamp(i) (i & 0xFFFFFF00)?(unsigned char)(((~i) >> 31) & 0xFF):(unsigned char)i
#if 0
inline unsigned char djpg_decoder_clamp(int i)
{
	if (i & 0xFFFFFF00)
	i = (((~i) >> 31) & 0xFF);

	return (i);
}
#endif


int djpg_H2V2Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dest_buf,int dst_width,int mcu_row)
{//YUV 4:2:0
	int l;
	int j;
	int k;
	int cb;
	int cr;
	int rc;
	int gc;
	int bc;
	int yy;
	unsigned char *y_buf;
	unsigned char *cb_buf;
	unsigned char *cr_buf;
	unsigned char *block_start_d0;
	unsigned char *d0;
	unsigned char *d1;
	int cb_cr_index;
	int b_cb_cr_index;
	#ifdef CREATE_BMP_FILE
	unsigned char *data_buf;
	#endif

	cb_buf = yuv_buf[4];
	cr_buf = yuv_buf[5];
	#ifdef CREATE_BMP_FILE
	data_buf = mcu_row_buf + mcu_row * jpg_decoder->max_mcu_x_size*BYTES_ONE_PIXEL; 
	#endif

	unsigned char tmp[8];

	for (l = 0; l < 4; l++)
	{
		y_buf = yuv_buf[l];

		#ifdef CREATE_BMP_FILE
		if(l == 0)block_start_d0 = data_buf;
		if(l == 1)block_start_d0 = data_buf + BITS_ONE_PIXEL;
		if(l == 2)block_start_d0 = data_buf + (dst_width<<3);
		if(l == 3)block_start_d0 = data_buf + (dst_width<<3) + BITS_ONE_PIXEL;

		#else
		if(l == 0)block_start_d0 = dest_buf;
		if(l == 1)block_start_d0 = dest_buf + BITS_ONE_PIXEL;
		if(l == 2)block_start_d0 = dest_buf + (dst_width<<3);
		if(l == 3)block_start_d0 = dest_buf + (dst_width<<3) + BITS_ONE_PIXEL;
		#endif
		
		if(l == 0)b_cb_cr_index = 0;
		if(l == 1)b_cb_cr_index = 4;
		if(l == 2)b_cb_cr_index = 32;
		if(l == 3)b_cb_cr_index = 36;

		for (k = 0; k < 8; k += 2)
		{
			d0 = block_start_d0 + dst_width*k;
			d1 = d0 + dst_width;

			cb_cr_index = b_cb_cr_index +(k << 2);
#ifndef OPTIMIZE_INTRIN
			for (j = 0; j < 8; j += 2)
			{
				cb = cb_buf[cb_cr_index];
				cr = cr_buf[cb_cr_index];

				rc = jpg_decoder->crr[cr];
				gc = jpg_decoder->crg[cr] + jpg_decoder->cbg[cb];
				bc = jpg_decoder->cbb[cb];

				yy = y_buf[j+(k<<3)];
				d0[2] = djpg_decoder_clamp((yy+rc));
				d0[1] = djpg_decoder_clamp((yy+gc));
				d0[0] = djpg_decoder_clamp((yy+bc));

				yy = y_buf[j+(k<<3)+1];
#ifndef RGB_32
				d0[5] = djpg_decoder_clamp((yy+rc));
				d0[4] = djpg_decoder_clamp((yy+gc));
				d0[3] = djpg_decoder_clamp((yy+bc));
				d0 += 6;
#else
				d0[6] = djpg_decoder_clamp(yy+rc);
				d0[5] = djpg_decoder_clamp(yy+gc);
				d0[4] = djpg_decoder_clamp(yy+bc);
				d0 += 8;
#endif
				yy = y_buf[j+(k<<3)+8];
				d1[2] = djpg_decoder_clamp((yy+rc));
				d1[1] = djpg_decoder_clamp((yy+gc));
				d1[0] = djpg_decoder_clamp((yy+bc));

				yy = y_buf[j+(k<<3)+8+1];
#ifndef RGB_32
				d1[5] = djpg_decoder_clamp((yy+rc));
				d1[4] = djpg_decoder_clamp((yy+gc));
				d1[3] = djpg_decoder_clamp((yy+bc));
				d1 += 6;
#else
				d1[6] = djpg_decoder_clamp(yy+rc);
				d1[5] = djpg_decoder_clamp(yy+gc);
				d1[4] = djpg_decoder_clamp(yy+bc);
				d1 += 8;
#endif
				cb_cr_index++;
			}
#else
		__m64 cb30,bc30,bc10,bc32;
			cb30 = _m_from_int(*(int*)(&cb_buf[cb_cr_index]));
			cb30 = _m_punpcklbw(cb30,jpg_decoder->zero);
			cb30 = _m_psllwi(cb30,2);
			cb30 = _m_psubsw(cb30,jpg_decoder->m512);
			bc30 = _m_pmulhw(cb30,jpg_decoder->cbbval);
			bc10 = _m_punpcklwd(bc30,bc30);
			bc32 = _m_punpckhwd(bc30,bc30);

			__m64 cr30,rc30,rc10,rc32;
			cr30 = _m_from_int(*(int*)(&cr_buf[cb_cr_index]));
			cr30 = _m_punpcklbw(cr30,jpg_decoder->zero);
			cr30 = _m_psllwi(cr30,2);
			cr30 = _m_psubsw(cr30,jpg_decoder->m512);
			rc30 = _m_pmulhw(cr30,jpg_decoder->crrval);
			rc10 = _m_punpcklwd(rc30,rc30);
			rc32 = _m_punpckhwd(rc30,rc30);

			__m64 crg30,cbg30,gc30,gc10,gc32;
			crg30 = _m_pmulhw(cr30,jpg_decoder->crgval);
			cbg30 = _m_pmulhw(cb30,jpg_decoder->cbgval);
			gc30 = _m_paddsw(crg30,cbg30);
			gc10 = _m_punpcklwd(gc30,gc30);
			gc32 = _m_punpckhwd(gc30,gc30);

			__m64 y70 = *(__m64*)(y_buf + (k << 3));
			__m64 y30 = _m_punpcklbw(y70,jpg_decoder->zero);
			__m64 y74 = _m_punpcklbw(_m_psrlqi(y70,32),jpg_decoder->zero);
			__m64 r30 = _m_paddsw(y30,bc10);
			__m64 r74 = _m_paddsw(y74,bc32);
			__m64 g30 = _m_paddsw(y30,gc10);
			__m64 g74 = _m_paddsw(y74,gc32);
			__m64 b30 = _m_paddsw(y30,rc10);
			__m64 b74 = _m_paddsw(y74,rc32);

			__m64 r70 = _m_packuswb(r30,r74);
			__m64 g70 = _m_packuswb(g30,g74);
			__m64 b70 = _m_packuswb(b30,b74);

			__m64 m0_c,m1_c,m2_c;
			__m64 m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			      m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			__m64 m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			      m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
				  m1_c = _m_psllqi(m1_c,8);
	        __m64 m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			      m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
				  m2_c = _m_psllqi(m2_c,16);


				  __m64 result = _m_por(_m_por(m0_c,m1_c),m2_c);
			__m64 temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			__m64 templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)d0=_m_por(temph,templ);
			//*(__m64*)d0= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);

			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d0+6)=_m_por(temph,templ);

			//*(__m64*)(d0 + 8) = _m_por(_m_por(m0_c,m1_c),m2_c);

			r70 = _m_psrlqi(r70,32);
			g70 = _m_psrlqi(g70,32);
			b70 = _m_psrlqi(b70,32);

			m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
	        m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d0+12)=_m_por(temph,templ);

			//*(__m64*)(d0 + 16)= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			//*(__m64*)(d0+18)=_m_por(temph,templ);
			*(__m64*)tmp=_m_por(temph,templ);
			for(int x = 0; x < 6; x++)
				*(d0++ + 18) = tmp[x];
			{
				__m64 y70 = *(__m64*)(y_buf + (k << 3) + 8);
			__m64 y30 = _m_punpcklbw(y70,jpg_decoder->zero);
			__m64 y74 = _m_punpcklbw(_m_psrlqi(y70,32),jpg_decoder->zero);
			__m64 r30 = _m_paddsw(y30,bc10);
			__m64 r74 = _m_paddsw(y74,bc32);
			__m64 g30 = _m_paddsw(y30,gc10);
			__m64 g74 = _m_paddsw(y74,gc32);
			__m64 b30 = _m_paddsw(y30,rc10);
			__m64 b74 = _m_paddsw(y74,rc32);

			__m64 r70 = _m_packuswb(r30,r74);
			__m64 g70 = _m_packuswb(g30,g74);
			__m64 b70 = _m_packuswb(b30,b74);

			__m64 m0_c,m1_c,m2_c;
			__m64 m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			      m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			__m64 m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			      m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
				  m1_c = _m_psllqi(m1_c,8);
	        __m64 m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			      m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
				  m2_c = _m_psllqi(m2_c,16);


				  __m64 result = _m_por(_m_por(m0_c,m1_c),m2_c);
			__m64 temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			__m64 templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)d1=_m_por(temph,templ);

			//*(__m64*)d0= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);

			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d1+6)=_m_por(temph,templ);

			//*(__m64*)(d0 + 8) = _m_por(_m_por(m0_c,m1_c),m2_c);

			r70 = _m_psrlqi(r70,32);
			g70 = _m_psrlqi(g70,32);
			b70 = _m_psrlqi(b70,32);

			m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
	        m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d1+12)=_m_por(temph,templ);

			//*(__m64*)(d0 + 16)= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			//*(__m64*)(d1+18)=_m_por(temph,templ);
			*(__m64*)tmp=_m_por(temph,templ);
			for(int x = 0; x < 6; x++)
				*(d1++ + 18) = tmp[x];
			}
#endif

		}
	}

	#ifdef CREATE_BMP_FILE
	if(mcu_row == (jpg_decoder->mcus_per_row-1))
	{
		data_buf = mcu_row_buf;
		for(l = 0; l < jpg_decoder->max_mcu_y_size; ++l)
		{
			if(p_bmp_handle)fwrite(data_buf, dst_width, 1, p_bmp_handle);
			data_buf += dst_width;
		}
	}
	#endif
#ifdef  OPTIMIZE_INTRIN
	_m_empty();
#endif
	return 0;
}


int djpg_H2V1Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dest_buf,int dst_width,int mcu_row)
{//YUV 4:2:2
	int l;
	int j;
	int k;
	int cb;
	int cr;
    int rc;
    int gc;
    int bc;
	int cb_cr_index;
	int yy;

	unsigned char *y_buf;
	unsigned char *cb_buf;
	unsigned char *cr_buf;
	unsigned char *d0;
	unsigned char *block_start_d0;
	int b_cb_cr_index;
	#ifdef CREATE_BMP_FILE
	unsigned char *data_buf;
	data_buf = mcu_row_buf + mcu_row * jpg_decoder->max_mcu_x_size*BYTES_ONE_PIXEL; 
	#endif

	unsigned char tmp[32];
	cb_buf = yuv_buf[2];
	cr_buf = yuv_buf[3];

#ifdef OPTIMIZE_INTRIN1111
	__m64 crrval = _m_from_int(0x59ba59ba);
	crrval = _m_punpcklwd(crrval,crrval);
	__m64 cbbval = _m_from_int(0x71687168);
    cbbval = _m_punpcklwd(cbbval,cbbval);
	__m64 crgval = _m_from_int(0xd24cd24c);
	crgval = _m_punpcklwd(crgval,crgval);
	__m64 cbgval = _m_from_int(0xe9fae9fa);
	cbgval = _m_punpcklwd(cbgval,cbgval);
	__m64 zero   = _m_from_int(0);
	__m64 m512   = _m_from_int(0x2000200);
	m512 = _m_punpcklwd(m512,m512);
	__m64 m256   = _m_from_int(0x1000100);
	m256 = _m_punpcklwd(m256,m256);
#endif

	for(l = 0; l < 2; l++)
	{
		y_buf = yuv_buf[l];
		#ifdef CREATE_BMP_FILE
		block_start_d0 = data_buf + l*BITS_ONE_PIXEL; 
		#else
		block_start_d0 = dest_buf + l*BITS_ONE_PIXEL; 
		#endif
		b_cb_cr_index = (l<<2);

		for(k = 0; k < 8; ++k)
		{
			d0 = block_start_d0 + k*dst_width;
			cb_cr_index = b_cb_cr_index + (k<<3);
#ifndef OPTIMIZE_INTRIN
			for(j = 0; j < 4; j++)
			{
				cb = cb_buf[cb_cr_index];
				cr = cr_buf[cb_cr_index];

				rc = jpg_decoder->crr[cr];
				//gc = ((jpg_decoder->crg[cr] + jpg_decoder->cbg[cb]) >> 16);
				gc = jpg_decoder->crg[cr] + jpg_decoder->cbg[cb];
				bc = jpg_decoder->cbb[cb];

				yy = y_buf[j<<1];
				d0[2] = djpg_decoder_clamp((yy+rc));
				d0[1] = djpg_decoder_clamp((yy+gc));
				d0[0] = djpg_decoder_clamp((yy+bc));

				yy = y_buf[(j<<1)+1];
#ifndef RGB_32
				d0[5] = djpg_decoder_clamp((yy+rc));
				d0[4] = djpg_decoder_clamp((yy+gc));
				d0[3] = djpg_decoder_clamp((yy+bc));
				d0 += 6;
#else
				d0[6] = djpg_decoder_clamp((yy+rc));
				d0[5] = djpg_decoder_clamp((yy+gc));
				d0[4] = djpg_decoder_clamp((yy+bc));
				d0 += 8;
#endif
				cb_cr_index ++;
			}
#else
			//cb_cr_index = b_cb_cr_index + (k<<3);
			__m64 cb30,bc30,bc10,bc32;
			cb30 = _m_from_int(*(int*)(&cb_buf[cb_cr_index]));
			cb30 = _m_punpcklbw(cb30,jpg_decoder->zero);
			cb30 = _m_psllwi(cb30,2);
			cb30 = _m_psubsw(cb30,jpg_decoder->m512);
			bc30 = _m_pmulhw(cb30,jpg_decoder->cbbval);
			bc10 = _m_punpcklwd(bc30,bc30);
			bc32 = _m_punpckhwd(bc30,bc30);

			__m64 cr30,rc30,rc10,rc32;
			cr30 = _m_from_int(*(int*)(&cr_buf[cb_cr_index]));
			cr30 = _m_punpcklbw(cr30,jpg_decoder->zero);
			cr30 = _m_psllwi(cr30,2);
			cr30 = _m_psubsw(cr30,jpg_decoder->m512);
			rc30 = _m_pmulhw(cr30,jpg_decoder->crrval);
			rc10 = _m_punpcklwd(rc30,rc30);
			rc32 = _m_punpckhwd(rc30,rc30);

			__m64 crg30,cbg30,gc30,gc10,gc32;
			crg30 = _m_pmulhw(cr30,jpg_decoder->crgval);
			cbg30 = _m_pmulhw(cb30,jpg_decoder->cbgval);
			gc30 = _m_paddsw(crg30,cbg30);
			gc10 = _m_punpcklwd(gc30,gc30);
			gc32 = _m_punpckhwd(gc30,gc30);

			__m64 y70 = *(__m64*)y_buf;
			__m64 y30 = _m_punpcklbw(y70,jpg_decoder->zero);
			__m64 y74 = _m_punpcklbw(_m_psrlqi(y70,32),jpg_decoder->zero);
			__m64 r30 = _m_paddsw(y30,bc10);
			__m64 r74 = _m_paddsw(y74,bc32);
			__m64 g30 = _m_paddsw(y30,gc10);
			__m64 g74 = _m_paddsw(y74,gc32);
			__m64 b30 = _m_paddsw(y30,rc10);
			__m64 b74 = _m_paddsw(y74,rc32);

			__m64 r70 = _m_packuswb(r30,r74);
			__m64 g70 = _m_packuswb(g30,g74);
			__m64 b70 = _m_packuswb(b30,b74);

			__m64 m0_c,m1_c,m2_c;
			__m64 m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			      m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			__m64 m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			      m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
				  m1_c = _m_psllqi(m1_c,8);
	        __m64 m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			      m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
				  m2_c = _m_psllqi(m2_c,16);


	        __m64 result = _m_por(_m_por(m0_c,m1_c),m2_c);
			__m64 temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			__m64 templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)d0=_m_por(temph,templ);
			

			//*(__m64*)d0= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);

			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d0+6)=_m_por(temph,templ);

			//*(__m64*)(d0 + 8) = _m_por(_m_por(m0_c,m1_c),m2_c);

			r70 = _m_psrlqi(r70,32);
			g70 = _m_psrlqi(g70,32);
			b70 = _m_psrlqi(b70,32);

			m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
	        m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d0+12)=_m_por(temph,templ);

			//*(__m64*)(d0 + 16)= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			//*(__m64*)(d0+18)=_m_por(temph,templ);
			*(__m64*)tmp=_m_por(temph,templ);
			for(int x = 0; x < 6; x++)
				*(d0++ + 18) = tmp[x];

			//*(__m64*)(d0 + 24) = _m_por(_m_por(m0_c,m1_c),m2_c);
#endif
			y_buf += 8;
		}
	}

	#ifdef CREATE_BMP_FILE
	if(mcu_row == (jpg_decoder->mcus_per_row-1))
	{
		data_buf = mcu_row_buf;
		for(l = 0; l < jpg_decoder->max_mcu_y_size; ++l)
		{
			if(p_bmp_handle)fwrite(data_buf, dst_width, 1, p_bmp_handle);
			data_buf += dst_width;
		}
	}
	#endif
#ifdef  OPTIMIZE_INTRIN
	_m_empty();
#endif

	return 0;
}



int djpg_H1V2Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dest_buf,int dst_width,int mcu_row)
{//YUV 4:2:2
	int i;
	int j;
	int k;
	int cb;
	int cr;
	int yy;
	int rc;
	int gc;
	int bc;

	unsigned char *y_buf;
	unsigned char *cb_buf;
	unsigned char *cr_buf;
	unsigned char *d0;
	unsigned char *d1;
	unsigned char *block_start_d0;
	int b_cb_cr_index;
	int m_cb_cr_index;

	#ifdef CREATE_BMP_FILE
	unsigned char *data_buf;
	data_buf = mcu_row_buf + mcu_row * jpg_decoder->max_mcu_x_size*BYTES_ONE_PIXEL; 
	#endif


	unsigned char tmp[32];
	//unsigned char tmp2[32];
	cb_buf = yuv_buf[2];
	cr_buf = yuv_buf[3];

	for (i = 0; i<2; ++i)
	{
		y_buf = yuv_buf[i];
		#ifdef CREATE_BMP_FILE
		block_start_d0  = data_buf + (i*dst_width*8);
		#else
		block_start_d0  = dest_buf + (i*dst_width*8);
		#endif
		b_cb_cr_index = (i==0)?0:32;
		
		for(k = 0; k < 8; k +=2)
		{
			d0 = block_start_d0 + k*dst_width;
			d1 = d0 + dst_width;

			m_cb_cr_index = b_cb_cr_index +(k<<2);
#ifndef OPTIMIZE_INTRIN
			for (j = 0; j < 8; j++)
			{
				cb = cb_buf[m_cb_cr_index];
				cr = cr_buf[m_cb_cr_index];

				rc = jpg_decoder->crr[cr];
				gc = jpg_decoder->crg[cr] + jpg_decoder->cbg[cb];
				bc = jpg_decoder->cbb[cb];

				yy = y_buf[j+(k<<3)];
				d0[2] = djpg_decoder_clamp(yy+rc);
				d0[1] = djpg_decoder_clamp(yy+gc);
				d0[0] = djpg_decoder_clamp(yy+bc);

				yy = y_buf[8+j+(k<<3)];
				d1[2] = djpg_decoder_clamp(yy+rc);
				d1[1] = djpg_decoder_clamp(yy+gc);
				d1[0] = djpg_decoder_clamp(yy+bc);
#ifndef RGB_32
				d0 += 3;
				d1 += 3;
#else
				d0 += 4;
				d1 += 4;
#endif
				m_cb_cr_index ++;
			}
#else
			//m_cb_cr_index = b_cb_cr_index +(k<<2);
			__m64 cb70,cb74,cb30,bc30,bc74;
			cb70 = *(__m64*)(&cb_buf[m_cb_cr_index]);
			cb30 = _m_punpcklbw(cb70,jpg_decoder->zero);
			cb30 = _m_psllwi(cb30,2);
			cb74 = _m_punpckhbw(cb70,jpg_decoder->zero);
			cb74 = _m_psllwi(cb74,2);

			cb30 = _m_psubsw(cb30,jpg_decoder->m512);
			bc30 = _m_pmulhw(cb30,jpg_decoder->cbbval);
			cb74 = _m_psubsw(cb74,jpg_decoder->m512);
			bc74 = _m_pmulhw(cb74,jpg_decoder->cbbval);

			__m64 cr70,cr74,cr30,rc30,rc74;
			cr70 = *(__m64*)(&cr_buf[m_cb_cr_index]);
			cr30 = _m_punpcklbw(cr70,jpg_decoder->zero);
			cr30 = _m_psllwi(cr30,2);
			cr74 = _m_punpckhbw(cr70,jpg_decoder->zero);
			cr74 = _m_psllwi(cr74,2);
			cr30 = _m_psubsw(cr30,jpg_decoder->m512);
			rc30 = _m_pmulhw(cr30,jpg_decoder->crrval);
			cr74 = _m_psubsw(cr74,jpg_decoder->m512);
			rc74 = _m_pmulhw(cr74,jpg_decoder->crrval);

			__m64 crg74,crg30,cbg74,cbg30,gc74,gc30;
			crg30 = _m_pmulhw(cr30,jpg_decoder->crgval);
			crg74 = _m_pmulhw(cr74,jpg_decoder->crgval);

			cbg30 = _m_pmulhw(cb30,jpg_decoder->cbgval);
			cbg74 = _m_pmulhw(cb74,jpg_decoder->cbgval);

			gc30 = _m_paddsw(crg30,cbg30);
			gc74 = _m_paddsw(crg74,cbg74);

			__m64 y70 = *(__m64*)(&y_buf[k<<3]);
			__m64 y30 = _m_punpcklbw(y70,jpg_decoder->zero);
			__m64 y74 = _m_punpcklbw(_m_psrlqi(y70,32),jpg_decoder->zero);
			__m64 r30 = _m_paddsw(y30,bc30);
			__m64 r74 = _m_paddsw(y74,bc74);

			__m64 g30 = _m_paddsw(y30,gc30);
			__m64 g74 = _m_paddsw(y74,gc74);

			__m64 b30 = _m_paddsw(y30,rc30);
			__m64 b74 = _m_paddsw(y74,rc74);

			__m64 r70 = _m_packuswb(r30,r74);
			__m64 g70 = _m_packuswb(g30,g74);
			__m64 b70 = _m_packuswb(b30,b74);

			__m64 m0_c,m1_c,m2_c;
			__m64 m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			      m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			__m64 m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			      m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
				  m1_c = _m_psllqi(m1_c,8);
	        __m64 m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			      m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
				  m2_c = _m_psllqi(m2_c,16);

			__m64 result = _m_por(_m_por(m0_c,m1_c),m2_c);
			__m64 temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			__m64 templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)d0=_m_por(temph,templ);
			//*(__m64*)d0= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);

			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d0+6)=_m_por(temph,templ);

			//*(__m64*)(d0 + 8) = _m_por(_m_por(m0_c,m1_c),m2_c);

			r70 = _m_psrlqi(r70,32);
			g70 = _m_psrlqi(g70,32);
			b70 = _m_psrlqi(b70,32);

			m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
	        m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d0+12)=_m_por(temph,templ);

			//*(__m64*)(d0 + 16)= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			//*(__m64*)(d0+18)=_m_por(temph,templ);
			*(__m64*)tmp=_m_por(temph,templ);
			for(int x = 0; x < 6; x++)
				*(d0++ + 18) = tmp[x];
			//*(__m64*)(d0 + 24) = _m_por(_m_por(m0_c,m1_c),m2_c);

			{
					__m64 y70 = *(__m64*)(&y_buf[(k<<3) + 8]);
			__m64 y30 = _m_punpcklbw(y70,jpg_decoder->zero);
			__m64 y74 = _m_punpcklbw(_m_psrlqi(y70,32),jpg_decoder->zero);
			__m64 r30 = _m_paddsw(y30,bc30);
			__m64 r74 = _m_paddsw(y74,bc74);

			__m64 g30 = _m_paddsw(y30,gc30);
			__m64 g74 = _m_paddsw(y74,gc74);

			__m64 b30 = _m_paddsw(y30,rc30);
			__m64 b74 = _m_paddsw(y74,rc74);

			__m64 r70 = _m_packuswb(r30,r74);
			__m64 g70 = _m_packuswb(g30,g74);
			__m64 b70 = _m_packuswb(b30,b74);

			__m64 m0_c,m1_c,m2_c;
			__m64 m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			      m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			__m64 m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			      m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
				  m1_c = _m_psllqi(m1_c,8);
	        __m64 m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			      m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
				  m2_c = _m_psllqi(m2_c,16);

			__m64 result = _m_por(_m_por(m0_c,m1_c),m2_c);
			__m64 temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			__m64 templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)d1=_m_por(temph,templ);
			//*(__m64*)d1= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d1 + 6)=_m_por(temph,templ);
			//*(__m64*)(d1 + 8) = _m_por(_m_por(m0_c,m1_c),m2_c);

			r70 = _m_psrlqi(r70,32);
			g70 = _m_psrlqi(g70,32);
			b70 = _m_psrlqi(b70,32);

			m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
	        m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d1 + 12)=_m_por(temph,templ);
			//*(__m64*)(d1 + 16)= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			//*(__m64*)(d1 + 18)=_m_por(temph,templ);
			*(__m64*)tmp=_m_por(temph,templ);
			for(int x = 0; x < 6; x++)
				*(d1++ + 18) = tmp[x];
			//*(__m64*)(d1 + 24) = _m_por(_m_por(m0_c,m1_c),m2_c);
			}
#endif
		}
	}

	#ifdef CREATE_BMP_FILE
	if(mcu_row == (jpg_decoder->mcus_per_row-1))
	{
		data_buf = mcu_row_buf;
		for(i = 0; i < jpg_decoder->max_mcu_y_size; ++i)
		{
			if(p_bmp_handle)fwrite(data_buf, dst_width, 1, p_bmp_handle);
			data_buf += dst_width;
		}
	}
	#endif

#ifdef  OPTIMIZE_INTRIN
	_m_empty();
#endif

	return 0;
}


int djpg_H1V1Convert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dest_buf,int dst_width,int mcu_row)
{//YUV 4:4:4
	int i;
	int j;
	int y;
	int cb;
	int cr;
	unsigned char *y_buf;
	unsigned char *cb_buf;
	unsigned char *cr_buf;
	unsigned char *d;
	#ifdef CREATE_BMP_FILE
	unsigned char *data_buf;
	data_buf = mcu_row_buf + mcu_row * jpg_decoder->max_mcu_x_size*BYTES_ONE_PIXEL; 
	#endif

	y_buf = yuv_buf[0];
	cb_buf = yuv_buf[1];
	cr_buf = yuv_buf[2];
	unsigned char tmp[32];

	for(i = 0; i < 8; ++i)
	{
		#ifdef CREATE_BMP_FILE
		d = data_buf + i*dst_width;
		#else
		d = dest_buf + i*dst_width;
		#endif
#ifndef  OPTIMIZE_INTRIN
		for (j = 0; j < 8; ++j)
		{
			y = y_buf[j];
			cb = cb_buf[j];
			cr = cr_buf[j];

			d[2] = djpg_decoder_clamp(y + jpg_decoder->crr[cr]);
			d[1] = djpg_decoder_clamp(y + (jpg_decoder->crg[cr] + jpg_decoder->cbg[cb]));
			d[0] = djpg_decoder_clamp(y + jpg_decoder->cbb[cb]);
			d += 3;
		}
#else
		__m64 cb70,cb74,cb30,bc30,bc74;
			cb70 = *(__m64*)cb_buf;
			cb30 = _m_punpcklbw(cb70,jpg_decoder->zero);
			cb30 = _m_psllwi(cb30,2);
			cb74 = _m_punpckhbw(cb70,jpg_decoder->zero);
			cb74 = _m_psllwi(cb74,2);

			cb30 = _m_psubsw(cb30,jpg_decoder->m512);
			bc30 = _m_pmulhw(cb30,jpg_decoder->cbbval);
			cb74 = _m_psubsw(cb74,jpg_decoder->m512);
			bc74 = _m_pmulhw(cb74,jpg_decoder->cbbval);

			__m64 cr70,cr74,cr30,rc30,rc74;
			cr70 = *(__m64*)cr_buf;
			cr30 = _m_punpcklbw(cr70,jpg_decoder->zero);
			cr30 = _m_psllwi(cr30,2);
			cr74 = _m_punpckhbw(cr70,jpg_decoder->zero);
			cr74 = _m_psllwi(cr74,2);
			cr30 = _m_psubsw(cr30,jpg_decoder->m512);
			rc30 = _m_pmulhw(cr30,jpg_decoder->crrval);
			cr74 = _m_psubsw(cr74,jpg_decoder->m512);
			rc74 = _m_pmulhw(cr74,jpg_decoder->crrval);

			__m64 crg74,crg30,cbg74,cbg30,gc74,gc30;
			crg30 = _m_pmulhw(cr30,jpg_decoder->crgval);
			crg74 = _m_pmulhw(cr74,jpg_decoder->crgval);

			cbg30 = _m_pmulhw(cb30,jpg_decoder->cbgval);
			cbg74 = _m_pmulhw(cb74,jpg_decoder->cbgval);

			gc30 = _m_paddsw(crg30,cbg30);
			gc74 = _m_paddsw(crg74,cbg74);

			__m64 y70 = *(__m64*)(y_buf);
			__m64 y30 = _m_punpcklbw(y70,jpg_decoder->zero);
			__m64 y74 = _m_punpcklbw(_m_psrlqi(y70,32),jpg_decoder->zero);
			__m64 r30 = _m_paddsw(y30,bc30);
			__m64 r74 = _m_paddsw(y74,bc74);

			__m64 g30 = _m_paddsw(y30,gc30);
			__m64 g74 = _m_paddsw(y74,gc74);

			__m64 b30 = _m_paddsw(y30,rc30);
			__m64 b74 = _m_paddsw(y74,rc74);

			__m64 r70 = _m_packuswb(r30,r74);
			__m64 g70 = _m_packuswb(g30,g74);
			__m64 b70 = _m_packuswb(b30,b74);

			__m64 m0_c,m1_c,m2_c;
			__m64 m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			      m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			__m64 m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			      m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
				  m1_c = _m_psllqi(m1_c,8);
	        __m64 m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			      m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
				  m2_c = _m_psllqi(m2_c,16);

			__m64 result = _m_por(_m_por(m0_c,m1_c),m2_c);
			__m64 temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			__m64 templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)d=_m_por(temph,templ);
			//*(__m64*)d= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d + 6)=_m_por(temph,templ);
			//*(__m64*)(d + 8) = _m_por(_m_por(m0_c,m1_c),m2_c);

			r70 = _m_psrlqi(r70,32);
			g70 = _m_psrlqi(g70,32);
			b70 = _m_psrlqi(b70,32);

			m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpcklwd(m0,jpg_decoder->zero);
			m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpcklwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
	        m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpcklwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			*(__m64*)(d + 12)=_m_por(temph,templ);
			//*(__m64*)(d + 16)= _m_por(_m_por(m0_c,m1_c),m2_c);

			//m0 = _m_punpcklbw(r70,jpg_decoder->zero);
			m0_c = _m_punpckhwd(m0,jpg_decoder->zero);
			//m1 = _m_punpcklbw(g70,jpg_decoder->zero);
			m1_c = _m_punpckhwd(m1,jpg_decoder->zero);
			m1_c = _m_psllqi(m1_c,8);
			//m2 = _m_punpcklbw(b70,jpg_decoder->zero);
			m2_c = _m_punpckhwd(m2,jpg_decoder->zero);
			m2_c = _m_psllqi(m2_c,16);
			result = _m_por(_m_por(m0_c,m1_c),m2_c);
			temph = _m_psrlqi(result,32);
			temph = _m_psllqi(temph,24);
			templ = _m_psllqi(result,32);
			templ = _m_psrlqi(templ,32);
			//*(__m64*)(d + 18)=_m_por(temph,templ);
			*(__m64*)tmp=_m_por(temph,templ);
			for(int x = 0; x < 6; x++)
				*(d++ + 18) = tmp[x];
			//*(__m64*)(d + 24) = _m_por(_m_por(m0_c,m1_c),m2_c);


#endif
		y_buf = y_buf + 8;
		cb_buf = cb_buf + 8;
		cr_buf = cr_buf + 8;

	}

	#ifdef CREATE_BMP_FILE
	if(mcu_row == (jpg_decoder->mcus_per_row-1))
	{
		data_buf = mcu_row_buf;
		for(i = 0; i < jpg_decoder->max_mcu_y_size; ++i)
		{
			if(p_bmp_handle)fwrite(data_buf, dst_width, 1, p_bmp_handle);
			data_buf += dst_width;
		}
	}
	#endif

#ifdef  OPTIMIZE_INTRIN
	_m_empty();
#endif

	return 0;

}

int djpg_GrayConvert(DJPG_DATA_PARAM *jpg_decoder,unsigned char *yuv_buf[JPGD_MAXBLOCKSPERMCU],unsigned char *dest_buf,int dst_width,int mcu_row)
{
	unsigned char *s;
	unsigned char *d;
	int i;
	#ifdef CREATE_BMP_FILE
	unsigned char *data_buf;
	data_buf = mcu_row_buf + mcu_row * jpg_decoder->max_mcu_x_size; 
	#endif

	for(i = 0; i < 8; ++i)
	{
		s = yuv_buf[0] + i*8;
		#ifdef CREATE_BMP_FILE
		d = data_buf + i*dst_width;
		#else		
		d = dest_buf + i*dst_width;
		#endif
		*(unsigned int *)d = *(unsigned int *)s;
		*(unsigned int *)(&d[4]) = *(unsigned int *)(&s[4]);
	}

	#ifdef CREATE_BMP_FILE
	if(mcu_row == (jpg_decoder->mcus_per_row-1))
	{
		data_buf = mcu_row_buf;
		for(i = 0; i < jpg_decoder->max_mcu_y_size; ++i)
		{
			if(p_bmp_handle)fwrite(data_buf, dst_width, 1, p_bmp_handle);
			data_buf += dst_width;
		}
	}
	#endif

	return 0;
}


#define PUT_2B(array,offset,value)  \
	(array[offset] = (char) ((value) & 0xFF), \
	 array[offset+1] = (char) (((value) >> 8) & 0xFF))
#define PUT_4B(array,offset,value)  \
	(array[offset] = (char) ((value) & 0xFF), \
	 array[offset+1] = (char) (((value) >> 8) & 0xFF), \
	 array[offset+2] = (char) (((value) >> 16) & 0xFF), \
	 array[offset+3] = (char) (((value) >> 24) & 0xFF))

#ifdef CREATE_BMP_FILE

FILE *djpg_create_bmp(DJPG_DATA_PARAM *jpg_decoder,char *p_file_name)
{
	FILE *m_file_handle = NULL;
	char bmpfileheader[14];
	char bmpinfoheader[40];
	int headersize, bfSize;
	int bits_per_pixel, cmap_entries;

	m_file_handle = fopen(p_file_name,"wb");

	if(m_file_handle)
	{
		#ifdef USER_MODE
		memset(bmpfileheader, 0 , sizeof(bmpfileheader));
		memset(bmpinfoheader, 0, sizeof(bmpinfoheader));
		#else
		RtlZeroMemory(bmpfileheader, sizeof(bmpfileheader));
		RtlZeroMemory(bmpinfoheader, sizeof(bmpinfoheader));
		#endif
		bits_per_pixel = BITS_ONE_PIXEL;
		cmap_entries = 0;

		headersize = 14 + 40 + cmap_entries * 4; /* Header and colormap */
		bfSize = headersize + (int) jpg_decoder->image_x_size * (int) jpg_decoder->image_y_size*BYTES_ONE_PIXEL;

		bmpfileheader[0] = 0x42;	/* first 2 bytes are ASCII 'B', 'M' */
		bmpfileheader[1] = 0x4D;
		PUT_4B(bmpfileheader, 2, bfSize); /* bfSize */
		/* we leave bfReserved1 & bfReserved2 = 0 */
		PUT_4B(bmpfileheader, 10, headersize); /* bfOffBits */

		/* Fill the info header (Microsoft calls this a BITMAPINFOHEADER) */
		PUT_2B(bmpinfoheader, 0, 40);	/* biSize */
		PUT_4B(bmpinfoheader, 4, jpg_decoder->image_x_size); /* biWidth */
		PUT_4B(bmpinfoheader, 8, jpg_decoder->image_y_size); /* biHeight */
		PUT_2B(bmpinfoheader, 12, 1);	/* biPlanes - must be 1 */
		PUT_2B(bmpinfoheader, 14, bits_per_pixel); /* biBitCount */
		PUT_2B(bmpinfoheader, 32, cmap_entries); /* biClrUsed */

		fwrite(bmpfileheader, sizeof(bmpfileheader), 1, m_file_handle);
		fwrite(bmpinfoheader, sizeof(bmpinfoheader), 1, m_file_handle);
	}

	return m_file_handle;
}
#endif


#ifdef PRINT_BITMAP_FILE
int djpg_print_rgb(DJPG_DATA_PARAM *jpg_decoder)
{

	int k;
	FILE *m_file_rgb24 = NULL;
	#ifdef DJPG_DBG_RGB_DATA
	int i;
	int j;
	int l;
	FILE *m_h2v1_file = NULL;
	#endif

	unsigned char *d0;

	char bmpfileheader[14];
	char bmpinfoheader[40];
	int headersize, bfSize;
	int bits_per_pixel, cmap_entries;

	#ifdef USER_MODE
	memset(bmpfileheader, 0 , sizeof(bmpfileheader));
	memset(bmpinfoheader, 0, sizeof(bmpinfoheader));
	#else
	RtlZeroMemory(bmpfileheader, sizeof(bmpfileheader));
	RtlZeroMemory(bmpinfoheader, sizeof(bmpinfoheader));
	#endif
	bits_per_pixel = BITS_ONE_PIXEL;
	cmap_entries = 0;

	headersize = 14 + 40 + cmap_entries * 4; /* Header and colormap */
	bfSize = headersize + (int) jpg_decoder->image_x_size * (int) jpg_decoder->image_y_size*BYTES_ONE_PIXEL;

	bmpfileheader[0] = 0x42;	/* first 2 bytes are ASCII 'B', 'M' */
	bmpfileheader[1] = 0x4D;
	PUT_4B(bmpfileheader, 2, bfSize); /* bfSize */
	/* we leave bfReserved1 & bfReserved2 = 0 */
	PUT_4B(bmpfileheader, 10, headersize); /* bfOffBits */

  /* Fill the info header (Microsoft calls this a BITMAPINFOHEADER) */
	PUT_2B(bmpinfoheader, 0, 40);	/* biSize */
	PUT_4B(bmpinfoheader, 4, jpg_decoder->image_x_size); /* biWidth */
	PUT_4B(bmpinfoheader, 8, jpg_decoder->image_y_size); /* biHeight */
	PUT_2B(bmpinfoheader, 12, 1);	/* biPlanes - must be 1 */
	PUT_2B(bmpinfoheader, 14, bits_per_pixel); /* biBitCount */
	PUT_2B(bmpinfoheader, 32, cmap_entries); /* biClrUsed */


	m_file_rgb24 = fopen("c:\\rgb_24_0.bmp","wb");

	d0 = jpg_decoder->djpg_out_buffer;

	if(m_file_rgb24)fwrite(bmpfileheader, sizeof(bmpfileheader), 1, m_file_rgb24);
	if(m_file_rgb24)fwrite(bmpinfoheader, sizeof(bmpinfoheader), 1, m_file_rgb24);
	for(k = 0; k < jpg_decoder->image_y_size; ++k)
	{
		if(m_file_rgb24)fwrite(d0, jpg_decoder->image_x_size*BYTES_ONE_PIXEL, 1, m_file_rgb24);
		d0 = d0 + jpg_decoder->image_x_size*BYTES_ONE_PIXEL;
	}

	if(m_file_rgb24)fclose(m_file_rgb24);

	#ifdef DJPG_DBG_RGB_DATA
	m_h2v1_file = fopen("E:\\TestJpg\\myjpg\\idct\\rgb24_all.dat","w");

	d0 = jpg_decoder->djpg_out_buffer;

	for(k = 0; k < jpg_decoder->image_y_size; ++k)
	{
		if(m_h2v1_file)
		fprintf(m_h2v1_file, "\n------------------ line = %d ---------------\n",k);

		for (i = jpg_decoder->max_mcus_per_row; i > 0; i--)
		{
			for (l = 0; l < 2; l++)
			{
				for (j = 0; j < 4; j++)
				{
					if(m_h2v1_file)
					fprintf(m_h2v1_file, "i = %d l = %d j = %d %x %x %x %x %x %x \n",i,l,j,d0[0],d0[1],d0[2],d0[3],d0[4],d0[5]);

					d0 += 6;
				}
			}
		}
	}
	if(m_h2v1_file)fclose(m_h2v1_file);

	#endif

	return 0;
}

#endif