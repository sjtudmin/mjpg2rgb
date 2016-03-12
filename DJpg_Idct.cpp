
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

#define CONST_BITS  13
#define PASS1_BITS  2
#define SCALEDONE ((int) 1)
#define CONST_SCALE (SCALEDONE << CONST_BITS)
#define FIX(x)  ((int) ((x) * CONST_SCALE + 0.5))
/*----------------------------------------------------------------------------*/
#define FIX_0_298631336  ((int)  2446)        /* FIX(0.298631336) */
#define FIX_0_390180644  ((int)  3196)        /* FIX(0.390180644) */
#define FIX_0_541196100  ((int)  4433)        /* FIX(0.541196100) */
#define FIX_0_765366865  ((int)  6270)        /* FIX(0.765366865) */
#define FIX_0_899976223  ((int)  7373)        /* FIX(0.899976223) */
#define FIX_1_175875602  ((int)  9633)        /* FIX(1.175875602) */
#define FIX_1_501321110  ((int)  12299)       /* FIX(1.501321110) */
#define FIX_1_847759065  ((int)  15137)       /* FIX(1.847759065) */
#define FIX_1_961570560  ((int)  16069)       /* FIX(1.961570560) */
#define FIX_2_053119869  ((int)  16819)       /* FIX(2.053119869) */
#define FIX_2_562915447  ((int)  20995)       /* FIX(2.562915447) */
#define FIX_3_072711026  ((int)  25172)       /* FIX(3.072711026) */
/*----------------------------------------------------------------------------*/
//#define DESCALE(x,n)  (((x) + (SCALEDONE << ((n)-1))) >> n)
#define DESCALE1(x,n)   ((x + 1024) >> 11)
#define DESCALE2(x,n)   ((x + 131072) >> 18)
#define DESCALE3(x,n)   ((x + 16) >> 5)
/*----------------------------------------------------------------------------*/
#define MULTIPLY(var,cnst)  ((var) * (cnst))
/*----------------------------------------------------------------------------*/

#define clamp(i) if (i & 0xFF00) i = (((~i) >> 15) & 0xFF);

#ifdef DJPG_DEBUG_IDCT
extern	FILE *m_file_name;
#endif

void djpg_idct(signed short *data, unsigned char *Pdst_ptr)
{
	int tmp0, tmp1, tmp2, tmp3;
	int tmp10, tmp11, tmp12, tmp13;
	int z1, z2, z3, z4, z5;
	signed short *dataptr;
	int rowctr;
	int dcval;

	#ifdef DJPG_DEBUG_IDCT
	static int idct_num=0;
	unsigned char *dst_buffer;
	int i;
	dst_buffer = Pdst_ptr;
	idct_num ++;
	#endif

	dataptr = data;
	for (rowctr = 7; rowctr >= 0; rowctr--)
	{
		if ((dataptr[1] | dataptr[2] | dataptr[3] | dataptr[4] |
         dataptr[5] | dataptr[6] | dataptr[7]) == 0)
		{
			dcval = (dataptr[0] << PASS1_BITS);

			dataptr[0] = (short)dcval;
			dataptr[1] = (short)dcval;
			dataptr[2] = (short)dcval;
			dataptr[3] = (short)dcval;
			dataptr[4] = (short)dcval;
			dataptr[5] = (short)dcval;
			dataptr[6] = (short)dcval;
			dataptr[7] = (short)dcval;

			dataptr += 8;       /* advance pointer to next row */
			continue;
		}

		z2 = dataptr[2];
		z3 = dataptr[6];

		z1 = MULTIPLY(z2 + z3, FIX_0_541196100);
		tmp2 = z1 + MULTIPLY(z3, - FIX_1_847759065);
		tmp3 = z1 + MULTIPLY(z2, FIX_0_765366865);

		tmp0 = (dataptr[0] + dataptr[4]) << CONST_BITS;
		tmp1 = (dataptr[0] - dataptr[4]) << CONST_BITS;

		tmp10 = tmp0 + tmp3;
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		tmp0 = dataptr[7];
		tmp1 = dataptr[5];
		tmp2 = dataptr[3];
		tmp3 = dataptr[1];

		z1 = tmp0 + tmp3;
		z2 = tmp1 + tmp2;
		z3 = tmp0 + tmp2;
		z4 = tmp1 + tmp3;
		z5 = MULTIPLY(z3 + z4, FIX_1_175875602);

		tmp0 = MULTIPLY(tmp0, FIX_0_298631336);
		tmp1 = MULTIPLY(tmp1, FIX_2_053119869);
		tmp2 = MULTIPLY(tmp2, FIX_3_072711026);
		tmp3 = MULTIPLY(tmp3, FIX_1_501321110);
		z1 = MULTIPLY(z1, - FIX_0_899976223);
		z2 = MULTIPLY(z2, - FIX_2_562915447);
		z3 = MULTIPLY(z3, - FIX_1_961570560);
		z4 = MULTIPLY(z4, - FIX_0_390180644);

		z3 += z5;
		z4 += z5;

		tmp0 += z1 + z3;
		tmp1 += z2 + z4;
		tmp2 += z2 + z3;
		tmp3 += z1 + z4;

		dataptr[0] = (signed short)DESCALE1(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
		dataptr[7] = (signed short)DESCALE1(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
		dataptr[1] = (signed short)DESCALE1(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
		dataptr[6] = (signed short)DESCALE1(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
		dataptr[2] = (signed short)DESCALE1(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
		dataptr[5] = (signed short)DESCALE1(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
		dataptr[3] = (signed short)DESCALE1(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
		dataptr[4] = (signed short)DESCALE1(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

		dataptr += 8;
	}

	dataptr = data;
	for (rowctr = 7; rowctr >= 0; rowctr--)
	{
		int i;

		if ((dataptr[8*1] | dataptr[8*2] | dataptr[8*3] |
			dataptr[8*4] | dataptr[8*5] | dataptr[8*6] |
			dataptr[8*7]) == 0)
		{
			dcval = DESCALE3(dataptr[0], PASS1_BITS+3);

			if ((dcval += 128) < 0)dcval = 0;
			else if (dcval > 255)dcval = 255;

			Pdst_ptr[8*0] = (unsigned char)dcval;
			Pdst_ptr[8*1] = (unsigned char)dcval;
			Pdst_ptr[8*2] = (unsigned char)dcval;
			Pdst_ptr[8*3] = (unsigned char)dcval;
			Pdst_ptr[8*4] = (unsigned char)dcval;
			Pdst_ptr[8*5] = (unsigned char)dcval;
			Pdst_ptr[8*6] = (unsigned char)dcval;
			Pdst_ptr[8*7] = (unsigned char)dcval;

			dataptr++;
			Pdst_ptr++;
			continue;
		}

		z2 = dataptr[8*2];
		z3 = dataptr[8*6];

		z1 = MULTIPLY(z2 + z3, FIX_0_541196100);
		tmp2 = z1 + MULTIPLY(z3, - FIX_1_847759065);
		tmp3 = z1 + MULTIPLY(z2, FIX_0_765366865);

		tmp0 = (dataptr[8*0] + dataptr[8*4]) << CONST_BITS;
		tmp1 = (dataptr[8*0] - dataptr[8*4]) << CONST_BITS;

		tmp10 = tmp0 + tmp3;
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		tmp0 = dataptr[8*7];
		tmp1 = dataptr[8*5];
		tmp2 = dataptr[8*3];
		tmp3 = dataptr[8*1];

		z1 = tmp0 + tmp3;
		z2 = tmp1 + tmp2;
		z3 = tmp0 + tmp2;
		z4 = tmp1 + tmp3;
		z5 = MULTIPLY(z3 + z4, FIX_1_175875602);

		tmp0 = MULTIPLY(tmp0, FIX_0_298631336);
		tmp1 = MULTIPLY(tmp1, FIX_2_053119869);
		tmp2 = MULTIPLY(tmp2, FIX_3_072711026);
		tmp3 = MULTIPLY(tmp3, FIX_1_501321110);
		z1 = MULTIPLY(z1, - FIX_0_899976223);
		z2 = MULTIPLY(z2, - FIX_2_562915447);
		z3 = MULTIPLY(z3, - FIX_1_961570560);
		z4 = MULTIPLY(z4, - FIX_0_390180644);

		z3 += z5;
		z4 += z5;

		tmp0 += z1 + z3;
		tmp1 += z2 + z4;
		tmp2 += z2 + z3;
		tmp3 += z1 + z4;


		i = DESCALE2(tmp10 + tmp3, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*0] = (unsigned char)i;

		i = DESCALE2(tmp10 - tmp3, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*7] = (unsigned char)i;

		i = DESCALE2(tmp11 + tmp2, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*1] = (unsigned char)i;

		i = DESCALE2(tmp11 - tmp2, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*6] = (unsigned char)i;

		i = DESCALE2(tmp12 + tmp1, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*2] = (unsigned char)i;

		i = DESCALE2(tmp12 - tmp1, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*5] = (unsigned char)i;

		i = DESCALE2(tmp13 + tmp0, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*3] = (unsigned char)i;

		i = DESCALE2(tmp13 - tmp0, CONST_BITS+PASS1_BITS+3) + 128;
		clamp(i)
		Pdst_ptr[8*4] = (unsigned char)i;

		dataptr++;
		Pdst_ptr++;
	}

	#ifdef DJPG_DEBUG_IDCT
	if(m_file_name)
	{
		fprintf(m_file_name,"\n--------- Block = %d ----------\n", idct_num);
		for(i = 0; i<64;++i)
		{
			fprintf(m_file_name,"%x ",dst_buffer[i]);
		}
	}
	#endif

}
