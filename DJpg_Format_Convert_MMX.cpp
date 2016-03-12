

//#define USER_MODE

#ifdef USER_MODE
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <memory.h>
#else
#include <wdm.h>
#endif

#include "mmintrin.h"

#include "djpg_common.h"

#pragma warning(push)
#pragma warning( disable : 4035 4799 )
#define FIX(x, b) ((long) ((x) * (1L<<(b)) + 0.5))


void Djpg_H2V2Convert_mmx(void)
{
	static __m64 CRR_MUL;

	CRR_MUL = _m_from_int(FIX(1.402/4, 16));

	CRR_MUL = _m_punpcklwd(CRR_MUL, CRR_MUL);
	CRR_MUL = _m_punpckldq(CRR_MUL, CRR_MUL);

	return;
}
